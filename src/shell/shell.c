#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/rtl.h>

int _cputws(const wchar_t *str, size_t count);

bool sh_exit;

wchar_t ShReadChar(void)
{
	static handle_t keyb, keyb_event;
	fileop_t op;
	uint32_t key;

	if (keyb == NULL)
	{
		keyb = ProcGetProcessInfo()->std_in;
		if (keyb == NULL)
			return (wchar_t) -1;
	}

	if (keyb_event == NULL)
	{
		keyb_event = EvtAlloc();
		if (keyb_event == NULL)
			return (wchar_t) -1;
	}
	
	op.event = keyb_event;
	do
	{
		if (!FsRead(keyb, &key, sizeof(key), &op))
			return false;

		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);

		if (op.result != 0 || op.bytes == 0)
			return -1;
	} while ((wchar_t) key == 0);

	return (wchar_t) key;
}

size_t ShReadLine(wchar_t *buf, size_t max)
{
	wchar_t ch;
	size_t read;

	read = 0;
	while ((ch = ShReadChar()) != (wchar_t) -1)
	{
		switch (ch)
		{
		case '\n':
			_cputws(L"\n", 1);
			*buf = '\0';
			return read;

		case '\b':
			if (read > 0)
			{
				buf--;
				read--;
				_cputws(L"\b \b", 3);
			}
			break;

		default:
			if (read < max)
			{
				_cputws(&ch, 1);
				*buf++ = ch;
				read++;
			}
			break;
		}
	}

	*buf = '\0';
	return read;
}

bool ShInternalCommand(const wchar_t *command, const wchar_t *params)
{
	wchar_t temp[MAX_PATH];

	if (*command == '\0')
		return true;
	else if (_wcsicmp(command, L"exit") == 0)
	{
		sh_exit = true;
		return true;
	}
	else if (_wcsicmp(command, L"cd") == 0)
	{
		FsFullPath(params, temp);
		wcscpy(ProcGetProcessInfo()->cwd, temp);
		return true;
	}

	return false;
}

int main(void)
{
	wchar_t buf[256], *space;
	process_info_t *proc;
	handle_t spawned;

	proc = ProcGetProcessInfo();
	wprintf(L"The Möbius: command-line shell\n");

	while (!sh_exit)
	{
		wprintf(L"[%s] ", proc->cwd);
		ShReadLine(buf, _countof(buf));
		
		space = wcschr(buf, ' ');
		if (space)
		{
			*space = '\0';
			space++;
		}
		else
			space = buf + wcslen(buf);

		if (!ShInternalCommand(buf, space))
		{
			wprintf(L"Starting %s...\n", buf);
			spawned = ProcSpawnProcess(buf);
			wprintf(L"Handle is %d\n", spawned);
		}
	}

	return EXIT_SUCCESS;
}
