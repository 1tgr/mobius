#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <os/port.h>
#include <os/os.h>
#include <os/fs.h>
#include <os/devreq.h>
#include <os/device.h>
#include <os/keyboard.h>
#include <os/console.h>

addr_t debugger;
addr_t vgatext;

typedef struct console_t console_t;
struct console_t
{
	addr_t port;
	addr_t base;
	unsigned x, y;
	word attrib;
};

int num_consoles;
console_t *consoles[8], *con_current;

void conWriteToScreen(console_t* con, word* buf, size_t length)
{
	request_t dreq;
	
	dreq.code = DEV_WRITE;
	dreq.params.write.length = length;
	dreq.params.write.buffer = buf;
	dreq.params.write.pos = con->base + (80 * con->y + con->x) * 2;
	con->x += length / sizeof(wchar_t);
	if (devUserRequest(vgatext, &dreq, sizeof(dreq)))
		thrWaitHandle(&dreq.event, 1, true);

	devUserFinishRequest(&dreq, true);
}

void conWrite(console_t* con, wchar_t* buf, size_t length)
{
	word* cells;
	wchar_t *next_ctrl, temp[100];
	int i, j;
	size_t len;

	j = 0;
	while (j < length)
	{
		next_ctrl = wcspbrk(buf + j, L"\t\r\n\x1b");

		if (next_ctrl > buf)
		{
			len = next_ctrl - buf;
			wcsncpy(temp, buf, len);

			cells = (word*) buf;
			for (i = 0; i < len; i++)
				cells[i] = con->attrib | (cells[i] & 0xff);

			conWriteToScreen(con, cells, len * sizeof(word));
			j += len;
		}
		else
		{
			switch (*next_ctrl)
			{
			default:
				putwchar(*next_ctrl);
			}

			j++;
		}
	}
}

void conClientThread(addr_t param)
{
	console_t* con;
	console_request_t req;
	console_reply_t reply;
	size_t length, written;
	wchar_t buf[100];
	
	con = (console_t*) param;

	wprintf(L"[%d] conClientThread: port %x\n", thrGetId(), con->port);
	while (thrWaitHandle(&con->port, 1, true))
	{
		wprintf(L"[%d] reading...", thrGetId());
		length = sizeof(req);
		if (FAILED(fsRead(con->port, &req, &length)) ||
			length < sizeof(req))
		{
			wprintf(L"failed\n");
			continue;
		}

		wprintf(L"done\n");
		reply.code = req.code;
		switch (req.code)
		{
		case CON_WRITE:
			written = 0;
			while (written < req.params.write.length)
			{
				length = min(req.params.write.length, sizeof(buf));
				if (FAILED(fsRead(con->port, buf, &length)))
					break;

				conWrite(con, buf, length);
				written += length;
			}
			break;

		case CON_CLOSE:
			portClose(con->port);
			free(con);
			thrExit(0);

		default:
			reply.code = CON_UNKNOWN;
			break;
		}

		length = sizeof(reply);
		fsWrite(con->port, &reply, &length);
	}
}

void conKeyboardThread(dword param)
{
	addr_t keyboard;
	request_t req, vreq;
	dword key;
	dword params[2];

	keyboard = devOpen(L"keyboard", NULL);
	while (true)
	{
		req.code = DEV_READ;
		req.params.read.buffer = &key;
		req.params.read.length = sizeof(key);

		if (!devUserRequest(keyboard, &req, sizeof(req)))
			break;

		thrWaitHandle(&req.event, 1, true);
		devUserFinishRequest(&req, true);

		if (key >= KEY_F1 && key <= KEY_F12 &&
			consoles[key - KEY_F1])
		{
			con_current = consoles[key - KEY_F1];
			wprintf(L"Switching to console %d at %x\n",
				key - KEY_F1, con_current->base);

			vreq.code = DEV_IOCTL;
			params[0] = 0;
			params[1] = con_current->base;
			vreq.params.ioctl.buffer = params;
			vreq.params.ioctl.length = sizeof(params);
			if (devUserRequest(vgatext, &vreq, sizeof(vreq)))
				thrWaitHandle(&vreq.event, 1, true);
			devUserFinishRequest(&req, true);
		}
	}

	devClose(keyboard);
}

int main(int argc, char** argv)
{
	addr_t port, client;
	console_t* con;

	debugger = devOpen(L"debugger", NULL);
	vgatext = devOpen(L"vgatext", NULL);

	port = portCreate(CONSOLE_PORT);
	portListen(port);

	thrCreate(conKeyboardThread, 0);
	//thrCreate(conConnector, (dword) L"Hello from thread 1\n");
	//thrCreate(conConnector, (dword) L"And hello from thread 2\n");

	while (true)
	{
		thrWaitHandle(&port, 1, true);
		client = portAccept(port);
		con = malloc(sizeof(console_t));
		con->port = client;
		con->base = num_consoles * 4000;
		consoles[num_consoles] = con;
		num_consoles++;
		thrCreate(conClientThread, (dword) con);
	}

	devClose(debugger);
	devClose(vgatext);
	return 0;
}
