/* $Id: mgltest.c,v 1.2 2002/03/06 01:39:43 pavlovskii Exp $ */

#include <os/syscall.h>
#include <gl/mgl.h>
#include <stdlib.h>
#include <os/rtl.h>
/*#include <os/mouse.h>
#include <os/fs.h>*/

mglrc_t *rc;
/*marshal_t mouse;*/

int main(void)
{
	int i;
	/*mouse_packet_t pkt;*/
	size_t length;
	MGLreal x, y;
	MGLrect dims;

	srand(SysUpTime());
	rc = mglCreateRc(NULL);
	mglGetDimensions(rc, &dims);
	glSetClearColour(0x000000);
	glClear();

	glSetColour(0x0000ff);
	glFillRect(0, 0, 500, 500);
	glSetColour(0x000005);
	glRectangle(0, 0, 500, 500);

	glSetColour(0x00000d);
	glFillRect(250, 250, 750, 750);
	glSetColour(0x000005);
	glRectangle(250, 250, 750, 750);
	
	for (i = 0; i < 16; i++)
	{
		glSetColour(i);
		glFillRect(i * 50, 0, (i + 1) * 50, 50);
	}

	glSetColour(0x000000);
	glMoveTo(100, 100);
	glLineTo(200, 100);
	glLineTo(200, 200);
	glLineTo(100, 100);

	glSetColour(0x000005);
	for (i = 0; i < 100; i++)
	{
		glMoveTo(rand() % dims.right, rand() % dims.bottom);
		glLineTo(rand() % dims.right, rand() % dims.bottom);
	}

	glFlush();

	/*mouse = fsOpen(L"/devices/ps2mouse");
	length = sizeof(pkt);
	x = 640;
	y = 480;
	while (fsRead(mouse, &pkt, &length))
	{
		if (pkt.buttons)
			break;

		x += pkt.dx;
		y += pkt.dy;

		//glFillRect(x - 1, y - 1, x + 1, y + 1);
		//glFlush();
		wprintf(L"%d, %d\t", x, y);
	}

	fsClose(mouse);*/
	ConReadKey();
	mglDeleteRc(rc);
	return 0;
}