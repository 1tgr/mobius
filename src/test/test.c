/* $Id: test.c,v 1.12 2002/01/08 01:20:32 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/port.h>
#include <os/defs.h>

process_info_t *ProcGetProcessInfo(void);
int _cputws(const wchar_t *str, size_t count);

static char key[2048];
static wchar_t str[_countof(key) + 1];
	
void testFileIo(const wchar_t *name)
{
	handle_t file;
	unsigned i;
	size_t len;
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
	{
		wprintf(L"Failed to open %s\n", name);
		return;
	}

	for (i = 0; i < 8; i++)
	{
		FsSeek(file, 0);
		wprintf(L"\x1b[%um", i + 30);
		while ((len = FsRead(file, key, sizeof(key))))
		{
			if (len < sizeof(key))
				key[len] = '\0';
			len = mbstowcs(str, key, _countof(key) - 1);
			if (len == -1)
				wprintf(L"invalid multibyte sequence\n");
			else
				_cputws(str, len);
		}

		ThrSleep(5000);
	}

	FsClose(file);
}

void testBlockDeviceIo(const wchar_t *name)
{
	handle_t file;
	unsigned i, j;
	size_t len;

	file = FsOpen(name, FILE_READ);
	if (file == NULL)
		wprintf(L"Failed to open %s\n", name);
	else
	{
		j = 0;
		do
		{
			wprintf(L"\x1b[2J\x1b[%um", (j++ % 8) + 30);
			FsSeek(file, 19 * 512);
			if ((len = FsRead(file, key, sizeof(key))))
			{
				for (i = 0; i < len; i++)
				{
					if (key[i])
						str[i] = (wchar_t) (unsigned char) key[i];
					else
						str[i] = '.';
				}
	
				str[i] = '\0';
				_cputws(str, len);
			}
		} while (len);
	}
	FsClose(file);
}

void testCharDeviceIo(const wchar_t *name)
{
	handle_t file;
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
		wprintf(L"Failed to open %s\n", name);
	else while (FsRead(file, key, 4))
		wprintf(L"%c", (wchar_t) (key[0] | key[1] << 8));

	FsClose(file);
}

int main(void)
{
	wprintf(L"Hello from tty0!\n");
	wprintf(L"Here's an escape sequence: \x1b[31mThis should be red!\x1b[37m\n");
	wprintf(L"And this is \x1b[1;5;32mbright green and flashing!\n\x1b[37m");
	wprintf(L"This is the first of two lines of text.\x1b[0m\n"
		L"\x1b[1B\x1b[2CAnd this is the second...\n");
	wprintf(L"Now the third...\n"
		L"...and fourth lines\n");
	wprintf(L"Here's a tab, just for a laugh...\tHa ha!\n");
	
	/*testFileIo(L"/hd/test.txt");*/
	testBlockDeviceIo(SYS_DEVICES L"/fdc0");
	/*testCharDeviceIo(SYS_DEVICES L"/keyboard");*/

	wprintf(L"Bye now...\n");
	return EXIT_SUCCESS;
}