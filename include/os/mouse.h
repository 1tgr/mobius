#ifndef __OS_MOUSE_H
#define __OS_MOUSE_H

typedef struct mouse_packet_t mouse_packet_t;
struct mouse_packet_t
{
	int dx;
	int dy;
	dword buttons;
	int dwheel;
};

#define MOUSE_BUTTON_RIGHT	1
#define MOUSE_BUTTON_MIDDLE	2
#define MOUSE_BUTTON_LEFT	4

#endif