/* $Id: test.c,v 1.4 2002/01/03 01:24:02 pavlovskii Exp $ */

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
	wchar_t name[] = SYS_DEVICES L"/fdc0";
	uint8_t buf[512];
	
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
		bytes = FsRead(file, buf, sizeof(buf));
		FsClose(file);
		wprintf(L"Read %u bytes\n", bytes);
		wprintf(L"The disk says: %02x %02x %02x\n", buf[0], buf[1], buf[2]);
	}

	/*info = ProcGetProcessInfo();
	while (FsRead(info->std_in, &key, sizeof(key)) == sizeof(key))
		wprintf(L"Key: %u\n", key);*/
	
	for (;;)
		;

	wprintf(L"Bye now...\n");
	return EXIT_SUCCESS;
}