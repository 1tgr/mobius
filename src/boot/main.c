/* $Id: main.c,v 1.3 2002/02/24 19:13:11 pavlovskii Exp $ */

#include <stdlib.h>
#include <stddef.h>
#include <dos.h>
#include <string.h>
#include "mobel_pe.h"
#include "../../include/kernel/ramdisk.h"

#define BOOT_DELAY		108
#define RAMDISK_ADDR	0x100000UL
//#define RAMDISK_ADDR	0x30000UL

char _got_32bit_cpu;
unsigned long _ext_mem_size, _conv_mem_size;
unsigned long memory_top, kernel_size, rdsk_size = 4096;

unsigned char BootCopyToExtendedMemory(uint32_t dest, uint32_t src, unsigned bytes);
void start_kernel(unsigned long kernel_addr, unsigned long ramdisk_addr);
int enable_a20(void);
int enable_frm(int enablea20);

static int boot_go;

char boot_buf[64];
unsigned char boot_dev;
device_t boot_device;
ramdisk_t boot_ramdisk;
ramfile_t boot_ramfiles[170];
/*char far *boot_buffer = MK_FP(0x2000, 0);*/
unsigned long boot_kerneladdr;
bool boot_isinit;
unsigned long boot_lastoffset = 4096;

uint16_t boot_int15gdt[24] =
{
	/* descriptor 0 */
	0, 0, 0, 0,			/* null descriptor */
	/* descriptor 1 */
	0, 0, 0, 0,			/* GDT descriptor */

	/* descriptor 2 */
	0,					/* elt 8 = source length */
	0, 0, 				/* elt 9 = 24-bit source address/8-bit access rights */
	0,					/* zero */
	
	/* descriptor 3 */
	0,					/* elt 12 = destination length */
	0, 0, 				/* elt 13 = 24-bit destination address/8-bit access rights */
	0,					/* zero */

	/* descriptor 4 */
	0, 0, 0, 0,			/* zeros */
	/* descriptor 5 */
	0, 0, 0, 0,			/* zeros */
};

#define FP_TO_LINEAR(ptr)	(((unsigned long) FP_SEG(ptr) << 4) + (unsigned long) FP_OFF(ptr))

extern struct command
{
	const char *cmd;
	void (*func)(const char *, const char *);
	const char *text;
} boot_commands[];

void BootShowDevice(void)
{
	char ch;
	if (boot_dev < 0x80)
		ch = boot_dev + 'A';
	else
		ch = boot_dev - 0x80 + 'C';
	cputs("Booting from drive ");
	writechar(ch);
	cputs(":\n");
}

#if 0
unsigned char BootDoCopyToExtendedMemory(unsigned int length);

unsigned char BootCopyToExtendedMemory(unsigned long dest, unsigned long src, size_t length)
{
	/*boot_int15gdt[8] = 0xffff;
	boot_int15gdt[9] = ((uint16_t*) &src)[0];
	boot_int15gdt[10] = (uint8_t) ((uint16_t*) &src)[1] | 0x9300;
	
	boot_int15gdt[12] = 0xffff;
	boot_int15gdt[13] = ((uint16_t*) &dest)[0];
	boot_int15gdt[14] = (uint8_t) ((uint16_t*) &dest)[1] | 0x9300;

	return BootDoCopyToExtendedMemory(length);*/
	copy_to_extended(dest, src, length);
	return 0;
}
#endif

bool BootLoadFile(const device_t *dev, const char *name)
{
	static char buf[512];
	fat_dirent_t di;
	unsigned long bytes_read, cluster, src, dest, bytes_copied, bytes_chunk;
	unsigned seg, off;
	unsigned pc, i;
	ramfile_t *file;

	src = ((unsigned long) _DS << 4) + (unsigned long) (unsigned short) buf;
	for (i = 0; i < boot_ramdisk.num_files; i++)
	{
		/*file = boot_ramfiles + i;
		movedata(FP_OFF(file), FP_SEG(file->name), 
			_DS, (unsigned) buf, sizeof(file->name));*/
		if (stricmp(boot_ramfiles[i].name, name) == 0)
		{
			cputs(name);
			cputs(" is already loaded\n");
			return true;
		}
	}

	if (BootFatLookupFile(dev, FAT_CLUSTER_ROOT, name, &di))
	{
		bytes_read = bytes_chunk = 0;
		seg = 0x2000;
		off = 0;
		cluster = di.first_cluster;
		dest = RAMDISK_ADDR + boot_lastoffset;
		
		if (stricmp(name, "kernel.exe") == 0)
		{
			kernel_size = di.file_length;
			boot_kerneladdr = dest;
		}

		pc = 0;
		cputs("0%   Loading ");
		cputs(name);
		cputs(" at ");
		/*cputl(seg);
		cputs(":");
		cputl(off);*/
		cputl(dest);
		while (bytes_read < di.file_length)
		{
			if (BootFatReadCluster(dev, cluster, buf) == 0)
			{
				cputs(name);
				cputs(": read aborted\n");
				break;
			}

			movedata(_DS, (unsigned) buf, seg, off, sizeof(buf));
			
			seg += sizeof(buf) / 16;
			bytes_read += dev->bytes_per_cluster;
			bytes_chunk += dev->bytes_per_cluster;

			if ((bytes_read * 100) / di.file_length != pc)
			{
				pc = (int) ((bytes_read * 100) / di.file_length);
				if (pc > 100)
					pc = 100;
				writechar('\r');
				cputld(pc);
				writechar('%');
			}

			if (bytes_chunk >= 0x4000 ||
				bytes_read >= di.file_length)
			{
				src = 0x20000UL;
				for (bytes_copied = 0; 
					bytes_copied < bytes_chunk; 
					bytes_copied += i, src += i)
				{
					if (bytes_chunk - bytes_copied > 0x4000)
						i = 0x4000;
					else
						i = bytes_chunk - bytes_copied;

					pc = BootCopyToExtendedMemory(
						dest + bytes_copied, 
						src + bytes_copied, 
						i);
					if (pc != 0)
					{
						cputs("BootCopyToExtendedMemory failed: ");
						cputl((unsigned) pc);
						return false;
					}
				}

				seg = 0x2000;
				dest += bytes_chunk;
				bytes_chunk = 0;
			}

			cluster = BootFatGetNextCluster(dev, cluster);
		}

		cputs("\n");
		file = boot_ramfiles + (int) boot_ramdisk.num_files;
		/*movedata(_DS, (unsigned) name, FP_SEG(file), FP_OFF(file->name), sizeof(file->name));*/
		strcpy(file->name, name);
		
		file->length = di.file_length;
		di.file_length = (di.file_length + 4095) & -4096;
		
		file->offset = boot_lastoffset;
		boot_lastoffset += di.file_length;
		rdsk_size += di.file_length; 
		boot_ramdisk.num_files++;

		/*seg = FP_SEG(addr) + di.file_length / 16;
		return MK_FP(seg, off);*/
		return true;
	}
	else
	{
		cputs(name);
		cputs(": file not found\n");
		return false;
	}
}

const char *BootPrompt(const char *prompt, const char *params)
{
	if (*params != '\0')
		return params;
	else
	{
		cputs(prompt);
		readline(boot_buf, sizeof(boot_buf));
		return boot_buf;
	}
}

bool BootDoInit(void)
{
	if (!boot_isinit)
	{
		if (BootFatInitDevice(boot_dev, &boot_device))
			boot_isinit = true;
		else
			cputs("Failed to initialize boot device\n");
	}

	return boot_isinit;
}

void BootCmdHelp(const char *cmd, const char *params)
{
	unsigned i;

	UNUSED(cmd);
	if (*params == '\0')
	{
		cputs("Valid commands are:  ");
		for (i = 0; boot_commands[i].cmd != NULL; i++)
		{
			cputs(boot_commands[i].cmd);
			cputs("  ");
		}
	}

	while (1)
	{
		params = BootPrompt("\n Command? ", params);
		if (*params == '\0')
			break;

		for (i = 0; boot_commands[i].cmd != NULL; i++)
		{
			if (stricmp(params, boot_commands[i].cmd) == 0)
			{
				cputs(boot_commands[i].cmd);
				cputs(": ");
				cputs(boot_commands[i].text);
				break;
			}
		}

		if (boot_commands[i].cmd == NULL)
		{
			cputs(params);
			cputs(": unrecognized command\n");
		}

		params = "";
	}
}

void BootCmdGo(const char *cmd, const char *params)
{
	UNUSED(cmd);
	UNUSED(params);
	boot_go = 1;
}

void BootCmdDevice(const char *cmd, const char *params)
{
	UNUSED(cmd);
	params = BootPrompt(" Device? ", params);
	if (*params != '\0')
	{
		if (*params >= 'a' && *params <= 'b')
			boot_dev = *params - 'a';
		else if (*params >= 'A' && *params <= 'B')
			boot_dev = *params - 'A';
		else if (*params >= 'c' && *params <= 'z')
			boot_dev = *params - 'c' + 0x80;
		else if (*params >= 'C' && *params <= 'Z')
			boot_dev = *params - 'Z' + 0x80;
		boot_isinit = false;
		BootShowDevice();
	}
}

void BootCmdLoad(const char *cmd, const char *params)
{
	UNUSED(cmd);
	params = BootPrompt(" File? ", params);
	if (*params != '\0' && BootDoInit())
		/*boot_lastfile = */BootLoadFile(&boot_device, params);
}

void BootCmdDir(const char *cmd, const char *params)
{
	static fat_dirent_t entries[512 / sizeof(fat_dirent_t)];
	char filename[FAT_MAX_NAME];
	unsigned long dir;
	int i, j;
	
	UNUSED(cmd);
	UNUSED(params);
	if (BootDoInit())
	{
		dir = FAT_CLUSTER_ROOT;
		while (!IS_EOC_CLUSTER(dir))
		{
			if (BootFatReadCluster(&boot_device, dir, entries))
			{
				for (i = 0; i < countof(entries) && entries[i].name[0]; i++)
				{
					if ((entries[i].attribs & FILE_ATTR_LONG_NAME) ||
						entries[i].name[0] == 0xe5)
						continue;

					BootFatAssembleName(entries + i, filename);
					cputs(filename);
					for (j = strlen(filename); j < 15; j++)
						writechar(' ');
					cputld(entries[i].file_length);
					cputs("\n");
				}

				if (i < countof(entries) && 
					entries[i].name[0] == '\0')
					break;
			}
			else
			{
				cputs("Failed to read root directory\n");
				break;
			}

			dir = BootFatGetNextCluster(&boot_device, dir);
		}
	}
}

struct command boot_commands[] =
{
	{ "help",	BootCmdHelp,	"Displays help for boot commands." },
	{ "go",		BootCmdGo,		"Continues booting The M\224bius." },
	{ "device",	BootCmdDevice,	"Changes to a different INT 13 device." },
	{ "load",	BootCmdLoad,	"Loads an individual file from the boot device." },
	{ "dir",	BootCmdDir,		"Lists the files on the boot device." },
	{ NULL,		NULL,			NULL }
};

unsigned long getclock(void)
{
	_AH = 0;
	geninterrupt(0x1a);
	return (_CX << 16) | _DX;
}

int kbhit(void)
{
	short far *head = MK_FP(0x40, 0x1A),
		far *tail = MK_FP(0x40, 0x1C);
	return *head != *tail;
}

void DbgDumpBuffer(const void far* buf, size_t size)
{
	const uint8_t far *ptr;
	int i, j;

	ptr = (const uint8_t far*) buf;
	for (j = 0; j < size; j += i, ptr += i)
	{
		cputl(FP_SEG(ptr));
		writechar(':');
		cputl(FP_OFF(ptr));
		writechar(' ');
		for (i = 0; i < 16; i++)
		{
			if (ptr[i] < 0x10)
				writechar('0');
			cputl(ptr[i]);
			writechar(' ');
			if (i + j >= size)
				break;
		}

		cputs("\n");
	}
}

int main(void)
{
	char *space;
	unsigned long end, prev, cur;
	int hit, secs, i;

	static const char *files[] =
	{
		/*"tty.drv",
		"keyboard.drv",
		"shell.exe",
		"libsys.dll",
		"libc.dll",
		"fat.drv",
		"fdc.drv",
		"test.txt",
		"ata.drv",
		"cmos.drv",*/
		"kernel.exe",
	};
	
	if (enable_frm(0xffff) != 0)
	{
		cputs("Unable to enable flat real mode\n");
		readkey();
		return EXIT_FAILURE;
	}

	boot_ramdisk.signature = RAMDISK_SIGNATURE_1;
	boot_ramdisk.num_files = 0;
	memory_top = _conv_mem_size + _ext_mem_size;
	cputs("\rThe M\224bius: Boot Menu              \n");

	hit = 0;
#if BOOT_DELAY > 0
	cputs("Press any key for boot options");
	while (kbhit())
	{
		_AH = 0;
		geninterrupt(0x16);
	}

	prev = getclock();
	end = prev + BOOT_DELAY;
	secs = 18;
	while (prev < end)
	{
		cur = getclock();
		if (cur != prev)
		{
			prev = cur;

			if (secs == 0)
			{
				writechar('.');
				secs = 18;
			}
			else
				secs--;
		}

		hit = kbhit();
		if (hit)
			break;
	}
#endif

	writechar('\r');
	if (hit)
	{
		while (kbhit())
		{
			_AH = 0;
			geninterrupt(0x16);
		}
		for (i = 0; i < 79; i++)
			writechar(' ');
		writechar('\r');
		BootShowDevice();

		while (!boot_go)
		{
			cputs("[Boot] ");
			readline(boot_buf, sizeof(boot_buf));
			space = strchr(boot_buf, ' ');
			if (space)
			{
				*space = '\0';
				space++;
			}
			else
				space = boot_buf + strlen(boot_buf);

			for (i = 0; boot_commands[i].cmd != NULL; i++)
				if (stricmp(boot_buf, boot_commands[i].cmd) == 0)
					boot_commands[i].func(boot_buf, space);
		}
	}

	if (!BootDoInit())
		return EXIT_FAILURE;

	for (i = 0; i < countof(files); i++)
		/*boot_lastfile = */BootLoadFile(&boot_device, files[i]);

	BootCopyToExtendedMemory(RAMDISK_ADDR, 
		FP_TO_LINEAR(&boot_ramdisk), sizeof(boot_ramdisk));
	BootCopyToExtendedMemory(RAMDISK_ADDR + sizeof(boot_ramdisk), 
		FP_TO_LINEAR(boot_ramfiles), sizeof(boot_ramfiles));

	if (boot_kerneladdr == 0L)
		cputs("Kernel not loaded\n");
	else
	{
		BootCopyToExtendedMemory(
			((unsigned long) _DS << 4) + (unsigned long) (unsigned short) &boot_buf,
			boot_kerneladdr + 0xc04b,
			sizeof(boot_buf));
		cputl(boot_kerneladdr + 0xc04b);
		writechar(' ');
		cputl(*(unsigned long*) boot_buf);
		readkey();
		/*enable_frm();*/
		start_kernel(boot_kerneladdr, RAMDISK_ADDR);
	}

	cputs("Press any key to restart...\n");
	readkey();
	return EXIT_SUCCESS;
}
