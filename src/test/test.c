/* $Id: test.c,v 1.15 2002/01/15 00:13:06 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>

#include <os/syscall.h>
#include <os/port.h>
#include <os/defs.h>

process_info_t *ProcGetProcessInfo(void);
int _cputws(const wchar_t *str, size_t count);

static char key[2048];
static wchar_t str[_countof(key) * 2 + 1];

void testFileIo(const wchar_t *name)
{
	handle_t file;
	unsigned i;
	size_t len;
	fileop_t op;
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
	{
		wprintf(L"Failed to open %s\n", name);
		return;
	}

	op.event = EvtAlloc();
	for (i = 0; i < 16; i++)
	{
		FsSeek(file, 0);
		wprintf(L"\x1b[%um", (i % 8) + 30);
		while (true)
		{
			if (!FsRead(file, key, sizeof(key), &op))
				break;
			ThrWaitHandle(op.event);
			len = op.bytes;
			if (len == 0)
				break;
			
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

	EvtFree(op.event);
	FsClose(file);
}

void testBlockDeviceIo(const wchar_t *name)
{
	handle_t file;
	unsigned i, j;
	size_t len;
	fileop_t op;
	wchar_t *ch;

	file = FsOpen(name, FILE_READ);
	if (file == NULL)
		wprintf(L"Failed to open %s\n", name);
	else
	{
		j = 0;
		op.event = EvtAlloc();
		for (j = 0; j < 1; j++)
		{
			wprintf(/*L"\x1b[2J"*/ L"\x1b[%um", (j % 8) + 30);
			FsSeek(file, 19 * 512);
			if (!FsRead(file, key, sizeof(key), &op))
				break;
			if (op.result == SIOPENDING)
			{
				DbgWrite(L"Wait start\n", 11);
				/*while (EvtIsSignalled(op.event))*/
					ThrWaitHandle(op.event);
				DbgWrite(L"Wait end\n", 9);
			}

			if (op.result == SIOPENDING)
				wprintf(L"op.result is still SIOPENDING\n");
			else if (op.result == 0 && op.bytes > 0)
			{
				ch = str;
				for (i = 0; i < op.bytes; i++)
				{
					swprintf(ch, L"%02X", key[i]);
					ch += 2;
				}
				_cputws(str, len * 2);
			}
			else
				break;
		}
		DbgWrite(L"Finished\n", 9);
		wprintf(L"Finished: op.result = %d, op.bytes = %u\n",
			op.result, op.bytes);
		EvtFree(op.event);
	}
	FsClose(file);
}

void testCharDeviceIo(const wchar_t *name)
{
	handle_t file;
	fileop_t op;
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
		wprintf(L"Failed to open %s\n", name);
	else
	{
		op.event = EvtAlloc();
		op.bytes = 0;
		wprintf(L"op = %p op.event = %u sig = %u\n", 
			&op,
			op.event, 
			EvtIsSignalled(op.event));
		if (op.event != NULL)
		{
			while (FsRead(file, key, 4, &op))
			{
				if (op.result == SIOPENDING)
				{
					DbgWrite(L"Wait start\n", 11);
					/*while (EvtIsSignalled(op.event))*/
						ThrWaitHandle(op.event);
					DbgWrite(L"Wait end\n", 9);
				}

				/*wprintf(L"op.result = %d op.bytes = %u\n",
					op.result, op.bytes);*/
				if (op.result == SIOPENDING)
					wprintf(L"op.result is still SIOPENDING\n");
				else
				{
					if (op.result > 0 ||
						op.bytes == 0)
						break;
					wprintf(L"%c", (wchar_t) (key[0] | key[1] << 8));
				}
				op.bytes = 0;
			}

			DbgWrite(L"Finished\n", 9);
			wprintf(L"Finished: op.result = %d, op.bytes = %u\n",
				op.result, op.bytes);
			EvtFree(op.event);
		}
	}

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
	/*printf("The Möbius Operating System\n");*/

	/*testFileIo(L"/hd/test.txt");*/
	testBlockDeviceIo(SYS_DEVICES L"/fdc0");
	/*testCharDeviceIo(SYS_DEVICES L"/keyboard");*/

	wprintf(L"Bye now...\n");
	return EXIT_SUCCESS;
}