/* $Id: test.c,v 1.18 2002/02/27 18:33:55 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>
#include <ctype.h>

#include <os/syscall.h>
#include <os/port.h>
#include <os/defs.h>
#include <os/rtl.h>

static char key[2048];
static wchar_t str[_countof(key) * 2 + 1];

wchar_t _wgetch(void)
{
	static handle_t keyb, keyb_event;
	fileop_t op;
	uint32_t key;

	if (keyb == NULL)
	{
		keyb = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
		if (keyb == NULL)
			return (wchar_t) -1;
	}

	if (keyb_event == NULL)
	{
		keyb_event = EvtAlloc();
		if (keyb_event == NULL)
			return (wchar_t) -1;
	}
	
	op.event = keyb_event;
	do
	{
		if (!FsRead(keyb, &key, sizeof(key), &op))
			return false;

		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);

		if (op.result != 0 || op.bytes == 0)
			return -1;
	} while ((wchar_t) key == 0);

	return (wchar_t) key;
}

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
		FsSeek(file, 0, FILE_SEEK_SET);
		wprintf(L"\x1b[%um", (i % 8) + 30);
		while (true)
		{
			if (!FsRead(file, key, sizeof(key), &op))
				break;
			if (op.result == SIOPENDING)
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

	HndClose(op.event);
	FsClose(file);
}

void testBlockDeviceIo(const wchar_t *name)
{
	handle_t file;
	unsigned i, j;
	fileop_t op;
	wchar_t *ch;

	file = FsOpen(name, FILE_READ);
	if (file == NULL)
		wprintf(L"Failed to open %s\n", name);
	else
	{
		op.event = EvtAlloc();
		for (j = 0; j < 1; j++)
		{
			wprintf(/*L"\x1b[2J"*/ L"\x1b[%um", (j % 8) + 30);
			FsSeek(file, 19 * 512, FILE_SEEK_SET);
			if (!FsRead(file, key, sizeof(key), &op))
				break;
			if (op.result == SIOPENDING)
				ThrWaitHandle(op.event);
			
			if (op.result == SIOPENDING)
				wprintf(L"op.result is still SIOPENDING\n");
			else if (op.result == 0 && op.bytes > 0)
			{
				wprintf(L"Read %u bytes\n", op.bytes);
				ch = str;
				for (i = 0; i < op.bytes; i++)
				{
					swprintf(ch, L"%02X", key[i]);
					ch += 2;
				}
				_cputws(str, op.bytes * 2);
				/*for (i = 0; i < op.bytes; i++)
					wprintf(L"%02X", key[i]);*/
			}
			else
				break;
		}
		DbgWrite(L"Finished\n", 9);
		wprintf(L"Finished: op.result = %d, op.bytes = %u\n",
			op.result, op.bytes);
		HndClose(op.event);
	}
	FsClose(file);
}

void testCharDeviceIo(const wchar_t *name)
{
	handle_t file;
	fileop_t op;
	uint32_t key;
	
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
			while (FsRead(file, &key, sizeof(key), &op))
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
					wprintf(L"%c", (wchar_t) key);
					if (key == 27)
						break;
				}
				op.bytes = 0;
			}

			DbgWrite(L"Finished\n", 9);
			wprintf(L"Finished: op.result = %d, op.bytes = %u\n",
				op.result, op.bytes);
			HndClose(op.event);
		}
	}

	FsClose(file);
}

int main(void)
{
	wchar_t key;
	wprintf(L"Hello from tty0!\n");
	wprintf(L"Here's an escape sequence: \x1b[31mThis should be red!\x1b[37m\n");
	wprintf(L"And this is \x1b[1;5;32mbright green and flashing!\n\x1b[37m");
	wprintf(L"This is the first of two lines of text.\x1b[0m\n"
		L"\x1b[1B\x1b[2CAnd this is the second...\n");
	wprintf(L"Now the third...\n"
		L"...and fourth lines\n");
	wprintf(L"Here's a tab, just for a laugh...\tHa ha!\n");
	/*printf("The Möbius Operating System\n");*/

	do
	{
		wprintf(
			L"OS Test Menu\n"
			L"------------\n"
			L"1)\tTest file I/O (/hd/test.txt)\n"
			L"2)\tTest block device I/O (" SYS_DEVICES L"/fdc0)\n"
			L"3)\tTest character device I/O (" SYS_DEVICES L"/keyboard)\n"
			L"Esc\tQuit\n"
			L"Choice: ");
		key = _wgetch();
		switch (key)
		{
		case '1':
			testFileIo(L"/hd/test.txt");
			break;
		case '2':
			testBlockDeviceIo(SYS_DEVICES L"/fdc0");
			break;
		case '3':
			testCharDeviceIo(SYS_DEVICES L"/keyboard");
			break;
		}
	} while (key != 27);
	
	wprintf(L"Bye now...\n");
	return EXIT_SUCCESS;
}