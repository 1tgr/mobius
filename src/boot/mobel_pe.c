/* $Id: mobel_pe.c,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

#include <dos.h>
#include <string.h>
#include <bios.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "mobel.h"

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned char bool;
typedef unsigned short wchar_t;
enum { false, true };

#include "../../include/kernel/ramdisk.h"

unsigned char drive;
char _got_32bit_cpu;
unsigned long _ext_mem_size, _conv_mem_size;
int errno;
mount_t _mount;

dev_t disk;
char kernel_cfg[] = "/kernel.cfg";
unsigned old_ss;
dword load_addr = LOAD_ADDR + PAGE_SIZE, kernel_addr;
void copy_to_extended(dword dest, void *src, unsigned bytes);
void start_kernel(dword kernel, dword ramdisk);

dword memory_top, kernel_size, rdsk_size;

void writechar(char ch)
{
	_AH = 0xE;
	_AL = ch;
	_BH = 0;
	geninterrupt(0x10);
}

int cputs(const char *str)
{
	for (; *str; str++)
	{
		if (*str == '\n')
			writechar('\r');
		writechar(*str);
	}
	return 0;
}

int vcprintf(const char *fmt, va_list list)
{
	int state, radix, chars;
	char *buf, temp[20];
	bool isUpper, leadingZero, isSigned, isLong;
	unsigned givenWidth, width;

	state = 0;
	chars = 0;
	for (; *fmt; fmt++)
	{
		switch (state)
		{
		case 0:	/* arbitrary character */
			if (*fmt == '%')
			{
				isLong = false;
				state = 1;
			}
			else
			{
				if (*fmt == '\n')
					writechar('\r');
				writechar(*fmt);
				chars++;
			}
			break;

		case 1:	/* character after % */
			givenWidth = 0;
			leadingZero = false;
			isUpper = false;
			isSigned = true;

			switch (*fmt)
			{
			case 's':
				buf = va_arg(list, char*);
				goto print;

			case 'l':
				isLong = true;
				break;

			case 'p':
				radix = 16;
				isUpper = false;
				leadingZero = true;
				givenWidth = 4;
				buf = temp;
				itoa((int) va_arg(list, void*), buf, radix);
				goto print;

			case 'X':
				isUpper = true;
			case 'x':
				radix = 16;
				goto is_number;

			case 'u':
				isSigned = false;
			case 'd':
				radix = 10;

is_number:
				buf = temp;
				if (isSigned)
				{
					if (isLong)
						ltoa(va_arg(list, long), buf, radix);
					else
						itoa(va_arg(list, int), buf, radix);
				}
				else
				{
					if (isLong)
						ltoa(va_arg(list, unsigned long), buf, radix);
					else
						itoa(va_arg(list, unsigned int), buf, radix);
				}
				goto print;

			case '%':
				buf = temp;
				buf[0] = '%';
				buf[1] = '\0';
				goto print;

			default:
				if (isdigit(*fmt))
				{
					fmt--;
					state = 20;
					continue;
				}
				else
					state = 0;
			}
			break;
print:
			if (isUpper)
				strupr(buf);
	
			if (givenWidth != 0)
			{
				char ch;
				
				if (leadingZero)
					ch = '0';
				else
					ch = ' ';

				for (width = strlen(buf); width < givenWidth; width++)
					writechar(ch);
			}
			else
				width = strlen(buf);

			cputs(buf);
			chars += width;
			state = 0;
			break;

		case 20:	/* digit after % */
			if (*fmt == '0')
				leadingZero = true;
			state++;
		case 21:
			if (isdigit(*fmt))
				givenWidth = (givenWidth * 10) + *fmt - '0';
			else
			{
				state = 1;
				fmt--;
			}
			break;
		}
	}

	return chars;
}

int cprintf(const char *fmt, ...)
{
	va_list list;
	int ret;

	va_start(list, fmt);
	ret = vcprintf(fmt, list);
	va_end(list);
	return ret;
}

int enable_frm(int enable_a20);

union rd
{
	byte raw[PAGE_SIZE];
	struct
	{
		ramdisk_t header;
		ramfile_t files[1];
	} s;
} ramdisk;

void process_line(const char *key, char *value)
{
	static byte buf[8192];
	file_t file;
	int bytes;
	unsigned blocks, i;
	ramfile_t *f;
	char *slash, *ch;
	
	if (stricmp(key, "load") == 0)
	{
		ch = slash = value;
		while ((ch = strchr(slash, '/')) != NULL)
			slash = ++ch;

		if (my_open(&file, (char*) value))
		{
			cprintf("%s: not found\n", value);
			return;
		}

		f = ramdisk.s.files + (unsigned) ramdisk.s.header.num_files;
		f->offset = load_addr - LOAD_ADDR;
		f->length = file.file_size;
		strncpy(f->name, slash, sizeof(f->name));

		cprintf("     %s at 0x%lx (%lu bytes) ", f->name, f->offset, f->length);

		blocks = (file.file_size + sizeof(buf) - 1) / sizeof(buf);
		for (i = 0; i < blocks; i++)
			writechar('_');
		for (i = 0; i < blocks; i++)
			writechar('\b');

		while ((bytes = my_read(&file, buf, sizeof(buf))) > 0)
		{
			copy_to_extended(load_addr, buf, bytes);
			load_addr += bytes;
			
			/*writechar(*twirlptr++);
			writechar('\b');
			if (*twirlptr == '\0')
				twirlptr = twirl;*/
			//cprintf("\r%3u%%", (unsigned) ((length * 100) / file.file_size));
			writechar('.');
		}

		cputs("\n");
		load_addr = (load_addr + PAGE_SIZE) & -PAGE_SIZE;
		ramdisk.s.header.num_files++;
		my_close(&file);
	}
	else
		cprintf("%s: invalid option\n", key);
}

int main(void)
{
	file_t file;
	char ch, str[128], *dest, *equals;
	
	old_ss = _SS;
	_SS = _DS;
	
	if (enable_frm(true))
		cprintf("Unable to set flat real mode\n");

	_ES = 0;
	__emit__(0x26, 0x67, 0xC6, 0x05, 0x00, 0x80, 0x0B, 0x00, 0x41);
	
	ramdisk.s.header.signature = RAMDISK_SIGNATURE_1;
	ramdisk.s.header.num_files = 0;

	//disk.int13_dev = drive = _DL;
	disk.int13_dev = drive = 0x0;
	if (fat_mount(&_mount, &disk, 0))
		return 0;

	cputs("\n");

	if (my_open(&file, kernel_cfg))
	{
		cprintf("%s: not found\n", kernel_cfg);
		return 0;
	}

	equals = dest = str;
	while (my_read(&file, &ch, 1))
	{
		if (ch == '=')
			equals = dest;
		else if (ch == '\r')
			continue;
		else if (ch == '\n' || ch == '#')
		{
			*dest = '\0';
			if (str[0])
			{
				if (equals > str)
				{
					*equals = '\0';
					equals++;
					process_line(str, equals);
				}
				else
					cprintf("%s: malformed line\n", str);
			}

			while (ch != '\n')
				my_read(&file, &ch, 1);
			
			equals = dest = str;
			continue;
		}

		*dest = ch;
		dest++;
	}

	my_close(&file);

	strcpy(str, "/kernel.exe");
	kernel_addr = load_addr;
	process_line("load", str);

	cprintf("%d files\n", ramdisk.s.header.num_files);
	copy_to_extended(LOAD_ADDR, ramdisk.raw, sizeof(ramdisk.raw));

	memory_top = _ext_mem_size + _conv_mem_size;
	kernel_size = load_addr - kernel_addr;
	rdsk_size = load_addr - LOAD_ADDR;
	start_kernel(kernel_addr, LOAD_ADDR);

	cputs("Press any key to restart\n");
	_DL = drive;
	_SS = old_ss;
	bioskey(0);
	return 0;
}
