#include <os/os.h>
#include <gl/mgl.h>
#include <stdlib.h>
#include <os/mouse.h>
#include <os/fs.h>

mglrc_t *rc;
addr_t mouse;

int main(void)
{
	//int i;
	mouse_packet_t pkt;
	size_t length;
	MGLreal x, y;

	srand(sysUpTime());
	rc = mglCreateRc(NULL);
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
	
	glSetColour(0x00000e);
	glMoveTo(100, 100);
	glLineTo(200, 100);
	glLineTo(200, 200);
	glLineTo(100, 100);

	glSetColour(0x000005);
	/*for (i = 0; i < 100; i++)
	{
		glMoveTo(rand() % 1280, rand() % 960);
		glLineTo(rand() % 1280, rand() % 960);
	}*/

	mouse = fsOpen(L"/devices/ps2mouse");
	length = sizeof(pkt);
	x = 640;
	y = 480;
	glMoveTo(x, y);
	while (fsRead(mouse, &pkt, &length))
	{
		if (pkt.buttons)
			break;
		x += pkt.dx;
		y += pkt.dy;
		glLineTo(x, y);
	}

	fsClose(mouse);
	mglDeleteRc(rc);
	return 0;
}