#ifndef __OS_CONSOLE_H
#define __OS_CONSOLE_H

#include <os/devreq.h>

/*!
 *	\defgroup	kernelu_console	Console I/O
 *	\ingroup	kernelu
 *	@{
 */

//! Name of the server port created by the console server
#define CONSOLE_PORT	L"console"

//! Write a string to the console
#define CON_WRITE	REQUEST_CODE(1, 0, 'c', 'w')
//! Close the console
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

bool	conWrite(const wchar_t* str);
dword	conGetKey(bool wait);
bool	conKeyPressed();

//@}

#endif