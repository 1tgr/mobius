/* $Id: console.c,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

#include <stdlib.h>
#include <wchar.h>

#include <os/port.h>
#include <os/syscall.h>
#include <os/defs.h>

int _putws(const wchar_t *str)
{
	return DbgWrite(str, wcslen(str));
}

int main(void)
{
	handle_t server, client;
	wchar_t buf[50];
	size_t size;

	_putws(L"Hello from the console!\n");
	server = FsCreate(SYS_PORTS L"/console", 0);
	PortListen(server);

	while (true)
	{
		if (!ThrWaitHandle(server))
			break;

		client = PortAccept(server, FILE_READ | FILE_WRITE);
		
		if (client != NULL)
		{
			wprintf(L"console: got client %u\n", client);
			while ((size = FsRead(client, buf, sizeof(buf) - 1)) > 0)
			{
				buf[size] = '\0';
				wprintf(L"%s", buf);
			}

			FsClose(client);
		}
	}

	wprintf(L"console: finished\n");
	FsClose(server);
	return EXIT_SUCCESS;
}