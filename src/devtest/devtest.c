#include <stdlib.h>
#include <os/os.h>
#include <os/device.h>
#include <os/mouse.h>
#include <os/keyboard.h>
#include <os/fs.h>
#include <os/console.h>
#include <os/port.h>
#include <wchar.h>
#include <stdio.h>
#include <conio.h>
#include <ctype.h>

void TestConsole(wchar_t* buf)
{
	addr_t client;
	console_request_t req;
	console_reply_t reply;
	size_t length;
	wchar_t name[10];

	wprintf(L"[%d] conConnector\n", thrGetId());
	swprintf(name, L"client%d", thrGetId());

	client = portCreate(name);
	if (!portConnect(client, CONSOLE_PORT))
	{
		wprintf(CONSOLE_PORT L": port not found\n");
		portClose(client);
		return;
	}

	wprintf(L"[%d] Connecting...", thrGetId());
	thrWaitHandle(&client, 1, true);
	wprintf(L"done\n");
	
	//while (true)
	{
		length = sizeof(req);
		req.code = CON_WRITE;
		req.params.write.length = wcslen(buf);
		fsWrite(client, &req, &length);
		length = req.params.write.length * sizeof(wchar_t);
		fsWrite(client, buf, &length);

		thrWaitHandle(&client, 1, true);

		length = sizeof(reply);
		fsRead(client, &reply, &length);
		wprintf(L"[%d] Reply: code = %x\n", thrGetId(), reply.code);
	}
}

void TestMouseAndKeyboard()
{
	addr_t mouse, keyboard;
	request_t mreq, kreq;
	wchar_t str[50];
	dword key;
	mouse_packet_t pkt;
	addr_t events[2];
	unsigned first;

	mouse = devOpen(L"ps2mouse", NULL);
	if (mouse == NULL)
		_cputws(L"Failed to open mouse\n");

	keyboard = devOpen(L"keyboard", NULL);
	if (keyboard == NULL)
		_cputws(L"Failed to open keyboard\n");

	for (;;)
	{
		kreq.header.code = DEV_READ;
		kreq.params.read.buffer = &key;
		kreq.params.read.length = sizeof(key);
		if (devUserRequest(keyboard, &kreq, sizeof(kreq)) == 0)
			events[0] = (addr_t) kreq.header.event;
		else
			events[0] = NULL;

		mreq.header.code = DEV_READ;
		mreq.params.read.buffer = &pkt;
		mreq.params.read.length = sizeof(pkt);
		if (devUserRequest(mouse, &mreq, sizeof(mreq)) == 0)
			events[1] = (addr_t) mreq.header.event;
		else
			events[1] = NULL;
		
		first = thrWaitHandle(events, 1, false);
		devUserFinishRequest(&kreq, true);
		devUserFinishRequest(&mreq, true);
	
		if (first == 0)
			str[0] = (wchar_t) key;
		else
			str[0] = pkt.buttons + '0';

		str[1] = 0;
		_cputws(str);
		
		if (key == 27)
			break;
	}

	devClose(keyboard);
	devClose(mouse);
}

void dump(const byte* buf, size_t size)
{
	wchar_t temp[4];
	int j;

	for (j = 0; j < size; j++)
	{
		if (((buf[j] >> 4) & 0xf) > 9)
			temp[0] = ((buf[j] >> 4) & 0xf) + 'a' - 10;
		else
			temp[0] = ((buf[j] >> 4) & 0xf) + '0';
		
		if ((buf[j] & 0xf) > 9)
			temp[1] = (buf[j] & 0xf) + 'a' - 10;
		else
			temp[1] = (buf[j] & 0xf) + '0';

		temp[2] = ' ';
		temp[3] = 0;
		_cputws(temp);
	}

	_cputws(L"\n");
}

void TestStorage(const wchar_t *device)
{
	static byte sector[512 * 5], sector2[512 * 3];
	static request_t req, req2;

	addr_t ide;
	int i;
	
	ide = devOpen(device, NULL);
	if (!ide)
	{
		wprintf(L"Failed to open %s\n", device);
		return;
	}

	for (i = 0; i < 4; i++)
	{
		req.header.code = DEV_READ;
		req.params.read.buffer = sector;
		req.params.read.length = sizeof(sector);
		req.params.read.pos = i * 512;

		req2.header.code = DEV_READ;
		req2.params.read.buffer = sector2;
		req2.params.read.length = sizeof(sector2);
		req2.params.read.pos = (i + 2) * 512;
		
		devUserRequest(ide, &req, sizeof(req));
		devUserRequest(ide, &req2, sizeof(req2));

		thrWaitHandle((addr_t*) &req.header.event, 1, true);
		devUserFinishRequest(&req, true);
		
		if (req.header.result == 0)
		{
			_cputws(L"1: ");
			dump(sector, 8);
		}

		thrWaitHandle((addr_t*) &req2.header.event, 1, true);
		devUserFinishRequest(&req2, true);

		if (req2.header.result == 0)
		{
			_cputws(L"2: ");
			dump(sector2, 8);
		}
	}

	devClose(ide);
}

void TestFileSystem(const wchar_t* filename)
{
	addr_t fd;
	request_t req;
	static char buf[8];
	static wchar_t wide[countof(buf) + 1];

	fd = fsOpen(filename);
	
	memset(buf, 0, sizeof(buf));
	while (true)
	{
		req.header.code = FS_READ;
		req.params.fs_read.fd = fd;
		req.params.fs_read.buffer = (addr_t) buf;
		req.params.fs_read.length = sizeof(buf);
		
		if (!fsRequest(fd, &req, sizeof(req)))
		{
			wprintf(L"Request failed\n");
			break;
		}
		else
		{
			thrWaitHandle(&req.header.event, 1, true);
			devUserFinishRequest(&req, true);
			
			if (req.header.result == 0)
			{
				//wprintf(L"Request succeeded (length = %d): press any key\n", 
					//req.params.fs_read.length);
				wide[countof(buf)] = 0;
				mbstowcs(wide, buf, countof(buf));
				_cputws(wide);
				_cputws(L"\n-- More --\r");
				if (towlower(_wgetch()) == 'q')
					break;
			}
			else
			{
				wprintf(L"Request failed: %x\n", req.header.result);
				break;
			}
		}
	}
	
	fsClose(fd);
}

int main()
{
	wprintf(L"Device test program\n");

	//_cputws(L"Press any key to test the console\n");
	//TestConsole(L"Hello, world!");
	
	//_cputws(L"Press any key to test file system\n");
	//_wgetch();
	//TestFileSystem(L"text/Keyboard Layouts/uk-cap.txt");
	//TestFileSystem(L"/Mobius/windows/bootlog.txt");
	//TestFileSystem(L"/devices/keyboard");
	//TestFileSystem(L"coffbase.txt");

	_cputws(L"Press any key to test floppy\n");
	_wgetch();
	TestStorage(L"floppy0");

	_cputws(L"Press any key to test IDE\n");
	_wgetch();
	TestStorage(L"ide0a");

	//_cputws(L"Press any key to test the mouse and keyboard\n");
	//_wgetch();
	//_cputws(L"Testing mouse and keyboard... (press Esc to exit)\n");
	//TestMouseAndKeyboard();

	_cputws(L"Finished!\n");

	return 0;
}