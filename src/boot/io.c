/* $Id: io.c,v 1.1 2002/02/22 15:31:19 pavlovskii Exp $ */

#include <dos.h>
#include <stddef.h>
#include "mobel_pe.h"

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

void cputl(unsigned long num)
{
	char str[9], *ptr;

	ptr = str + 8;
	*ptr = '\0';
	do
	{
		ptr--;
		*ptr = num % 16;
		if (*ptr >= 10)
			*ptr += 'a' - 10;
		else
			*ptr += '0';
		num /= 16;
	} while (num > 0);
	cputs(ptr);
}

void cputld(unsigned long num)
{
	char str[11], *ptr;

	ptr = str + 10;
	*ptr = '\0';
	do
	{
		ptr--;
		*ptr = num % 10;
		*ptr += '0';
		num /= 10;
	} while (num > 0);
	cputs(ptr);
}

char readkey(void)
{
	_AH = 0;
	geninterrupt(0x16);
	return _AL;
}

void readline(char *buf, int max)
{
	char ch, *ptr;
	ptr = buf;
	while (1)
	{
		_AH = 0;
		geninterrupt(0x16);
		ch = _AL;

		switch (ch)
		{
		case '\r':
			writechar('\r');
			writechar('\n');
			*ptr = '\0';
			return;

		case '\b':
			if (ptr > buf)
			{
				writechar('\b');
				writechar(' ');
				writechar('\b');
				ptr--;
			}
			break;

		default:
			if (ptr - buf < max)
			{
				*ptr++ = ch;
				writechar(ch);
			}
			break;
		}
	}
}