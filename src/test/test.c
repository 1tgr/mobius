/* $Id: test.c,v 1.5 2002/01/03 15:44:08 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/port.h>
#include <os/defs.h>

process_info_t *ProcGetProcessInfo(void);

int main(void)
{
	/*uint32_t key;
	process_info_t *info;*/
	handle_t file;
	size_t bytes;
	wchar_t name[] = SYS_DEVICES L"/keyboard";
	uint32_t key;
	
	wprintf(L"Hello from tty0!\n");
	wprintf(L"Here's an escape sequence: \x1b[31mThis should be red!\x1b[37m\n");
	wprintf(L"And this is \x1b[1;5;32mbright green and flashing!\n\x1b[37m");
	wprintf(L"This is the first of two lines of text.\x1b[0m\n"
		L"\x1b[1B\x1b[2CAnd this is the second...\n");
	wprintf(L"Now the third...\n"
		L"...and fourth lines\n");
	wprintf(L"Here's a tab, just for a laugh...\tHa ha!\n");
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
		wprintf(L"Failed to open %s\n", name);
	else
	{
		do
		{
			bytes = FsRead(file, &key, sizeof(key));
			/*wprintf(L"Read %u bytes; the device says: %02x %02x %02x %02x\n", 
				bytes, buf[0], buf[1], buf[2], buf[3]);*/
			wprintf(L"%c", (wchar_t) key);
		} while (bytes > 0);

		FsClose(file);
	}

	/*info = ProcGetProcessInfo();
	while (FsRead(info->std_in, &key, sizeof(key)) == sizeof(key))
		wprintf(L"Key: %u\n", key);*/
	
	wprintf(L"Bye now...\n");
	return EXIT_SUCCESS;
}