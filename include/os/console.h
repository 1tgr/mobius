#ifndef __OS_CONSOLE_H
#define __OS_CONSOLE_H

#define CONSOLE_PORT	L"console"

#define CON_UNKNOWN	REQUEST_CODE(0, 0, 'c', 0)
#define CON_WRITE	REQUEST_CODE(1, 0, 'c', 'w')
#define CON_CLOSE	REQUEST_CODE(1, 0, 'c', 'c')

typedef struct console_request_t console_request_t;
struct console_request_t
{
	dword code;

	union
	{
		struct
		{
			size_t length;
		} write;
	} params;
};

typedef struct console_reply_t console_reply_t;
struct console_reply_t
{
	dword code;
};

#endif