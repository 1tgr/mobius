/*!
 *	\ingroup		programs
 *	\defgroup		console	Console server
 *
 *	Console server for The Möbius
 *
 *	- Sets up a server port (named CONSOLE_PORT = L"console") and waits for 
 *		connections.
 *	- Starts a new thread for each connection and creates an
 *		on-screen console.
 *	- Receives characters over the connection and displays them on the console.
 *	- Sends any keys pressed over the connection.
 *	- Allows switching between consoles (via Ctrl+Tab/Ctrl+Shift+Tab) and 
 *		closing of consoles (via Ctrl+F4).
 *
 *	Most console I/O for programs is handled by kernelu.dll.
 */

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <ctype.h>

#include <os/port.h>
#include <os/os.h>
#include <os/fs.h>
#include <os/devreq.h>
#include <os/device.h>
#include <os/keyboard.h>
#include <os/console.h>
#include <os/mouse.h>
#include <os/video.h>

//@{

//! Colours to be used for console title bars
#define CON_TITLE_COLOUR	0x1700
//! Colours to be used for the buttons on console title bars
#define CON_BUTTON_COLOUR	0x7900

//! The text-mode VGA device
addr_t vgatext;

typedef struct console_t console_t;

//! Describes a console
/*!
 *	The console server maintains one console_t structure for each connection
 *		to its server port.
 */
struct console_t
{
	//! Connection to the client program
	addr_t port;
	//! Base address of the console's video buffer, relative to the start
	//!	of video memory
	addr_t base;
	//! Location of the text cursor
	unsigned x, y;
	//! Attributes (colour/intensity) currently selected for new text
	word attrib;
	//! Variables describing the state of the escape sequence parser
	int esc, esc1, esc2, esc3;
	//! Width and height of the console
	short width, height;
	//! Console's title
	wchar_t* title;
};

//! Number of consoles currently allocated
int num_consoles;
console_t *consoles[8], *con_current;
int mouse_x, mouse_y;

void conWriteToScreen(console_t* con, word* buf, size_t length)
{
	request_t dreq;
	
	dreq.code = DEV_WRITE;
	dreq.params.write.length = length;
	dreq.params.write.buffer = buf;
	dreq.params.write.pos = con->base + (con->width * (con->y + 1) + con->x) * 2;
	con->x += length / sizeof(word);
	if (devUserRequest(vgatext, &dreq, sizeof(dreq)))
		thrWaitHandle(&dreq.event, 1, true);

	devUserFinishRequest(&dreq, true);
}

void conDrawMouse(bool draw)
{
	request_t dreq;
	word w = 0x1234;
	
	dreq.code = DEV_WRITE;
	dreq.params.write.length = sizeof(w);
	dreq.params.write.buffer = &w;
	dreq.params.write.pos = con_current->base + 
		(con_current->width * mouse_y + mouse_x) * 2;
	if (devUserRequest(vgatext, &dreq, sizeof(dreq)))
		thrWaitHandle(&dreq.event, 1, true);

	devUserFinishRequest(&dreq, true);
}

void conWriteTitle(console_t* con)
{
	request_t dreq;
	word* buf;
	int i;
	const wchar_t* ch;
	char n[2];
	int index;
	
	index = -1;
	for (i = 0; i < num_consoles; i++)
		if (consoles[i] == con)
		{
			index = i;
			break;
		}

	buf = malloc(con->width * sizeof(word));
	for (i = 0; i < con->width; i++)
		buf[i] = CON_TITLE_COLOUR | 0x20;

	i = 1;
	for (ch = L"Ctrl+F1=Help"; *ch && i < con->width; ch++, i++)
	{
		wcstombs(n, ch, 1);
		buf[i] = CON_TITLE_COLOUR | (byte) n[0];
	}

	i = (con->width - wcslen(con->title)) / 2;
	if (i < 0)
		i = 0;

	if (index > 0)
		buf[max(i - 2, 0)] = CON_TITLE_COLOUR | 27;
	
	for (ch = con->title; *ch && i < con->width; ch++, i++)
	{
		wcstombs(n, ch, 1);
		buf[i] = CON_TITLE_COLOUR | (byte) n[0];
	}

	if (index > -1 &&
		index < num_consoles - 1)
		buf[min(i + 1, con->width - 1)] = CON_TITLE_COLOUR | 26;

	buf[con->width - 1] = CON_BUTTON_COLOUR | 0xFE;

	dreq.code = DEV_WRITE;
	dreq.params.write.length = con->width * sizeof(word);
	dreq.params.write.buffer = buf;
	dreq.params.write.pos = con->base;
	if (devUserRequest(vgatext, &dreq, sizeof(dreq)))
		thrWaitHandle(&dreq.event, 1, true);

	devUserFinishRequest(&dreq, true);
	free(buf);
}

void conClear(console_t* con)
{
	word *buf;
	int i;

	buf = malloc(sizeof(word) * con->width * con->height);
	for (i = 0; i < con->width * con->height; i++)
		buf[i] = con->attrib | 0x20;

	con->x = con->y = 0;
	conWriteToScreen(con, buf, sizeof(word) * con->width * con->height);
	con->x = con->y = 0;
	free(buf);
}

void conSetAttrib(console_t* con, byte att)
{
	static const char ansi_to_vga[] =
	{
		0, 4, 2, 6, 1, 5, 3, 7
	};
	unsigned char new_att;

	new_att = con->attrib >> 8;
	if(att == 0)
		new_att &= ~0x08;		/* bold off */
	else if(att == 1)
		new_att |= 0x08;		/* bold on */
	else if(att >= 30 && att <= 37)
	{
		att = ansi_to_vga[att - 30];
		new_att = (new_att & ~0x07) | att;/* fg color */
	}
	else if(att >= 40 && att <= 47)
	{
		att = ansi_to_vga[att - 40] << 4;
		new_att = (new_att & ~0x70) | att;/* bg color */
	}

	con->attrib = new_att << 8;
}

void conUpdateBase(console_t* con)
{
	dword params[2];
	params[0] = con->base;
	params[1] = con->base + 
		(con->x + (con->y + 1) * con->width) * 2;
	devIoCtl(vgatext, 0, params, sizeof(params));
}

void conSwitch(console_t* con)
{
	con_current = con;
	conUpdateBase(con);
	conWriteTitle(con);
}

void conClose(console_t* con)
{
	unsigned i;

	for (i = 0; i < num_consoles; i++)
		if (consoles[i] == con)
			consoles[i] = NULL;

	if (con->port)
		portClose(con->port);

	free(con);
}

void conWriteInternal(console_t* con, const wchar_t* buf, size_t length)
{
	word* cells;
	const wchar_t *next_ctrl;
	wchar_t temp[100];
	int i, j, k;
	size_t len;

	//wprintf(L"conWriteInternal: writing %d...", length);
	j = 0;
	while (j < length)
	{
		if (con->esc == 0)
		{
			next_ctrl = NULL;
			for (k = j; k < length; k++)
				if (wcschr(L"\t\r\n\b\x1b", buf[k]))
				{
					next_ctrl = buf + k;
					break;
				}

			if (next_ctrl == NULL)
				next_ctrl = buf + length;

			if (next_ctrl > buf + j)
			{
				wchar_t w;
				char n[2];

				len = next_ctrl - (buf + j);
				wcsncpy(temp, buf + j, len);
				temp[len] = '\0';

				/*cells = (word*) temp;
				for (i = 0; i < len; i++)
				{
					w = temp[i];
					wcstombs(n, &w, 1);
					cells[i] = con->attrib | (byte) n[0];
				}

				conWriteToScreen(con, cells, len * sizeof(word));*/
				dputws(temp);
				j += len;
			}
			else
			{
				switch (*next_ctrl)
				{
				case '\t':
					con->x = (con->x + 4) & -3;
					break;
				case '\n':
					con->x = 0;
					con->y++;
					break;
				case '\r':
					con->x = 0;
					break;
				case '\b':
					if (con->x > 0)
					{
						con->x--;
						conWriteInternal(con, L" ", 1);
						con->x--;
					}
					break;
				case '\x1b':
					con->esc = 1;
					con->esc1 = 0;
					break;
				}

				j++;
			}
		}
		else
		{
			wchar_t c;

			c = buf[j++];
			
			if (con->esc == 1)
			{
				if (c == L'[')
				{
					con->esc++;
					con->esc1 = 0;
					continue;
				}
			}
			else if (con->esc == 2)
			{
				if (iswdigit(c))
				{
					con->esc1 = con->esc1 * 10 + c - L'0';
					continue;
				}
				else if (c == ';')
				{
					con->esc++;
					con->esc2 = 0;
					continue;
				}
				else if (c == 'J')
				{
					if (con->esc1 == 2)
						conClear(con);
				}
				else if (c == 'm')
					conSetAttrib(con, con->esc1);
				
				con->esc = 0;
				continue;
			}
			else if (con->esc == 3)
			{
				if (iswdigit(c))
				{
					con->esc2 = con->esc2 * 10 + c - '0';
					continue;
				}
				else if(c == ';')
				{
					con->esc++;	/* ESC[num1;num2; */
					con->esc3 = 0;
					continue;
				}
				/* ESC[num1;num2H -- move cursor to num1,num2 */
				else if(c == 'H')
				{
					if(con->esc2 < con->width)
						con->x = con->esc2;
					if(con->esc1 < con->height)
						con->y = con->esc1;
				}
				/* ESC[num1;num2m -- set attributes num1,num2 */
				else if(c == 'm')
				{
					conSetAttrib(con, con->esc1);
					conSetAttrib(con, con->esc2);
				}
				con->esc = 0;
				continue;
			}
			/* ESC[num1;num2;num3 */
			else if(con->esc == 4)
			{
				if (iswdigit(c))
				{
					con->esc3 = con->esc3 * 10 + c - '0';
					continue;
				}
				/* ESC[num1;num2;num3m -- set attributes num1,num2,num3 */
				else if(c == 'm')
				{
					conSetAttrib(con, con->esc1);
					conSetAttrib(con, con->esc2);
					conSetAttrib(con, con->esc3);
				}
				con->esc = 0;
				continue;
			}

			con->esc = 0;
		}
	}

	//wprintf(L"done\n");

	if (con == con_current)
		conUpdateBase(con);
}

bool conWrite(const wchar_t *str)
{
	conWriteInternal(con_current, str, wcslen(str));
	return true;
}

console_t *conCreate(addr_t port)
{
	console_t *con;
	wchar_t str[16];

	con = malloc(sizeof(console_t));
	con->port = port;
	con->base = num_consoles * 4000;
	con->attrib = 0x0700;
	con->esc = 0;
	con->width = 80;
	con->height = 24;
	swprintf(str, L"Console %d", num_consoles + 1);
	con->title = wcsdup(str);

	consoles[num_consoles++] = con;

	conWriteTitle(con);
	conClear(con);
	conSwitch(con);

	return con;
}

void conClientThread(addr_t param)
{
	console_t* con;
	console_request_t req;
	//console_reply_t reply;
	size_t length, written;
	wchar_t buf[100];
	
	con = (console_t*) param;

	//wprintf(L"[%d] conClientThread: port %x\n", thrGetId(), con->port);
	while (thrWaitHandle(&con->port, 1, true))
	{
		//wprintf(L"[%d] reading...", thrGetId());
		length = sizeof(req);
		if (!fsRead(con->port, &req, &length) ||
			length < sizeof(req))
		{
			_pwerror(L"console");
			continue;
		}

		//wprintf(L"done\n");
		//reply.code = req.code;
		switch (req.code)
		{
		case CON_WRITE:
			written = 0;
			while (written < req.params.write.length)
			{
				length = min(req.params.write.length, countof(buf));
				length *= sizeof(wchar_t);
				if (!fsRead(con->port, buf, &length))
					break;

				length /= sizeof(wchar_t);
				conWriteInternal(con, buf, length);
				written += length;
			}
			break;

		case CON_CLOSE:
			conClose(con);
			thrExit(0);

		default:
			//reply.code = CON_UNKNOWN;
			break;
		}

		//length = sizeof(reply);
		//fsWrite(con->port, &reply, &length);
	}
}

void conHelpThread(dword param)
{
	console_t *con, *old_current;

	old_current = con_current;
	con = conCreate(NULL);
	conWrite(L"\x1b[30;47m\x1b[2JThe Möbius Help");
	thrSleep(1000);
	conClose(con);
	conSwitch(old_current);
	thrExit(0);
}

void conKeyboardThread(dword param)
{
	addr_t keyboard;
	request_t req;
	dword key, old_key;
	size_t length;
	unsigned con_current_index = 0;
	//wchar_t ch;
	
	//wprintf(L"Keyboard thread (%d)\n", thrGetId());
	keyboard = devOpen(L"keyboard", NULL);
	while (true)
	{
		req.code = DEV_READ;
		req.params.read.buffer = &key;
		req.params.read.length = sizeof(key);

		//wprintf(L"Request...");
		if (!devUserRequest(keyboard, &req, sizeof(req)))
			break;

		//wprintf(L"done\nWait...");
		thrWaitHandle(&req.event, 1, true);
		//wprintf(L"done\n");
		devUserFinishRequest(&req, true);

		//wprintf(L"Key pressed\n");

		old_key = key;
		if (key & KBD_BUCKY_CTRL)
		{
			if (consoles[con_current_index] != con_current)
				con_current_index = -1;

			switch (key & ~KBD_BUCKY_ANY)
			{
			case KEY_F1:
				thrCreate(conHelpThread, 0, 16);
				break;

			case KEY_F4:
				conClose(con_current);

			case '\t':
				if (key & KBD_BUCKY_SHIFT)
					do
					{
						con_current_index = (con_current_index - 1) % num_consoles;
					} while (consoles[con_current_index] == NULL);
				else
					do
					{
						con_current_index = (con_current_index + 1) % num_consoles;
					} while (consoles[con_current_index] == NULL);

				conSwitch(consoles[con_current_index]);
				continue;
			}
		}
		
		if (con_current)
		{
			length = sizeof(old_key);
			fsWrite(con_current->port, &old_key, &length);

			//ch = old_key;
			//if (ch)
				//conWriteInternal(con_current, &ch, 1);
		}
	}

	devClose(keyboard);
	thrExit(0);
}

void conMouseThread(dword param)
{
	addr_t mouse;
	request_t req;
	mouse_packet_t packet;

	mouse = devOpen(L"mouse", NULL);
	while (true)
	{
		req.code = DEV_READ;
		req.params.read.buffer = &packet;
		req.params.read.length = sizeof(packet);

		if (!devUserRequest(mouse, &req, sizeof(req)))
			break;

		thrWaitHandle(&req.event, 1, true);
		devUserFinishRequest(&req, true);

		conDrawMouse(false);
		mouse_x += packet.dx;
		mouse_y += packet.dy;
		conDrawMouse(true);
	}

	devClose(mouse);
	thrExit(0);
}

void NtProcessStartup()
{
	addr_t port, client;
	addr_t video;
	console_t* con;
	request_t *req;
	vid_rect_t *rect;
	vid_text_t *text;
	size_t size;
	int i;

	vgatext = devOpen(L"vgatext", NULL);
	video = devOpen(L"vga4", NULL);

	size = sizeof(*req) - sizeof(union request_params_t) + sizeof(vid_rect_t);
	req = malloc(size);
	req->code = VID_FILLRECT;
	rect = (vid_rect_t*) &req->params;
	rect->colour = 6;

	for (i = 0; i < 16; i++)
	{
		rect->rect.left = i * 32;
		rect->rect.top = 0;
		rect->rect.right = rect->rect.left + 32;
		rect->rect.bottom = rect->rect.top + 32;
		rect->colour = i;
		devUserRequestSync(video, req, size);
	}

	free(req);
	
	size = sizeof(*req) - sizeof(union request_params_t) + sizeof(vid_text_t);
	req = malloc(size);
	req->code = VID_TEXTOUT;
	text = (vid_text_t*) &req->params;
	text->buffer = (addr_t) L"Hello, world!";
	text->length = wcslen((wchar_t*) text->buffer) * sizeof(wchar_t);
	text->backColour = (pixel_t) -1;

	for (i = 0; i < 10; i++)
	{
		text->x = rand() % 100;
		text->y = rand() % 100;
		text->foreColour = rand() % 16;
		devUserRequestSync(video, req, size);
	}

	free(req);

	devClose(video);
	
	port = portCreate(CONSOLE_PORT);
	portListen(port);

	thrCreate(conKeyboardThread, 0, 16);
	//thrCreate(conMouseThread, 0, 16);
	wprintf(L"Console server\n");
	while (true)
	{
		thrWaitHandle(&port, 1, true);
		client = portAccept(port);
		con = conCreate(client);
		thrCreate(conClientThread, (dword) con, 16);
	}

	devClose(vgatext);
	procExit(0);
}

//@}