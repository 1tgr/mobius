#include <os/os.h>
#include <os/port.h>
#include <os/console.h>
#include <stdlib.h>

bool conGetLine(wchar_t* buffer, size_t max, bool echo)
{
	wchar_t *ch = buffer, str[2] = { 0, 0 };

	while (true)
	{
		*ch = conGetKey(true);
		
		switch (*ch)
		{
		case 0:
			break;

		case '\n':
			*ch = 0;
			conWrite(L"\n");
			return true;

		case '\b':
			if (ch > buffer)
			{
				conWrite(L"\b \b");
				ch--;
			}
			break;

		case 27:
			return false;

		default:
			str[0] = *ch;
			
			if (echo)
				conWrite(str);
			else
				conWrite(L"*");

			if (ch < buffer + max)
				ch++;
		}
	}
}

void NtProcessStartup()
{
	addr_t proc, port;
	process_info_t *proc_info;
	wchar_t user[128], pass[128];

	/* Start the console server */
	procLoad(L"console.exe", NULL, 16, NULL, NULL);

	/*
	 * Keep trying to connect -- allow console to start up.
	 * Create a console for the login screen
	 */
	port = portCreate(L"init");
	do
	{
		thrSleep(10);
	} while (!portConnect(port, CONSOLE_PORT));

	thrWaitHandle(&port, 1, true);

	/* Assign our stdin/stdout */
	proc_info = thrGetInfo()->process;
	proc_info->stdin = proc_info->stdout = port;

	while (true)
	{
		/* Display a banner and a login prompt */
		conWrite(L"\x1b[2J"
			L"\x1b[1;34H\x1b[30;42m The Möbius "
			L"\x1b[4;5H\x1b[37;40mLogin:"
			L"\x1b[5;2H\x1b[37;40mPassword:"
			L"\x1b[4;12H");

		if (!conGetLine(user, countof(user), true))
			continue;

		conWrite(L"\x1b[5;12H");
		if (!conGetLine(pass, countof(pass), false))
			continue;

		conWrite(L"\n\n");

		/* Start a new shell in this console and wait for it to finish */
		proc = procLoad(L"short.exe", NULL, 16, NULL, NULL);
		thrWaitHandle(&proc, 1, true);
	}

	procExit(0);
}