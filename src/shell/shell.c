/* $Id: shell.c,v 1.4 2002/02/22 16:51:35 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/rtl.h>

int _cputws(const wchar_t *str, size_t count);

bool sh_exit;
wchar_t sh_path[MAX_PATH];

typedef struct shell_command_t shell_command_t;
struct shell_command_t
{
	const wchar_t *name;
	void (*func)(const wchar_t*, const wchar_t*);
	unsigned help_id;
};

extern shell_command_t sh_commands[];

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

const wchar_t *ShPrompt(const wchar_t *prompt, const wchar_t *params)
{
	static wchar_t buf[256];
	if (*params != '\0')
		return params;
	else
	{
		wprintf(L"%s", prompt);
		ShReadLine(buf, _countof(buf));
		return buf;
	}
}

void ShCmdHelp(const wchar_t *command, const wchar_t *params)
{
	wchar_t text[256];
	unsigned i;
	bool onlyOnce;

	if (*params == '\0')
	{
		onlyOnce = false;
		wprintf(L"Valid commands are:  ");
		for (i = 0; sh_commands[i].name != NULL; i++)
			wprintf(L"%s ", sh_commands[i].name);
	}
	else
		onlyOnce = true;

	while (1)
	{
		params = ShPrompt(L"\n Command? ", params);
		if (*params == '\0')
			break;

		for (i = 0; sh_commands[i].name != NULL; i++)
		{
			if (_wcsicmp(params, sh_commands[i].name) == 0)
			{
				if (!ResLoadString(NULL, sh_commands[i].help_id, text, _countof(text)))
					swprintf(text, L"no string for ID %u", sh_commands[i].help_id);
				wprintf(L"%s: %s", sh_commands[i].name, text);
				break;
			}
		}

		if (sh_commands[i].name == NULL)
			wprintf(L"%s: unrecognized command\n", params);
		
		if (onlyOnce)
		{
			wprintf(L"\n");
			break;
		}

		params = L"";
	}
}

void ShCmdExit(const wchar_t *command, const wchar_t *params)
{
	sh_exit = true;
}

void ShCmdCd(const wchar_t *command, const wchar_t *params)
{
	params = ShPrompt(L" Directory? ", params);
	if (*params == '\0')
		return;

	if (FsFullPath(params, sh_path))
		wcscpy(ProcGetProcessInfo()->cwd, sh_path);
	else
		wprintf(L"%s: invalid path\n", params);
}

void ShCmdDir(const wchar_t *command, const wchar_t *params)
{
	handle_t search;
	dirent_t dir;
	fileop_t op;
	
	if (*params == '\0')
		params = L"*";

	if (!FsFullPath(params, sh_path))
	{
		wprintf(L"%s: invalid path\n", params);
		return;
	}

	search = FsOpenSearch(params);
	wprintf(L"%s: search = %d\n", command, search);

	if (search != NULL)
	{
		op.event = NULL;
		while (FsRead(search, &dir, sizeof(dir), &op))
		{
			if (op.result == SIOPENDING)
				ThrWaitHandle(op.event);

			wprintf(L"%s\t%lu\t%x08%x\n", 
				dir.name, 
				(uint32_t) dir.length, 
				(uint32_t) dir.standard_attributes,
				(uint32_t) (dir.standard_attributes >> 32));
		}

		FsClose(search);
	}
}

void ShCmdType(const wchar_t *command, const wchar_t *params)
{
	static char buf[2048];
	static wchar_t str[_countof(buf)];

	handle_t file;
	size_t len;
	fileop_t op;
	unsigned i;
	
	params = ShPrompt(L" File? ", params);
	if (*params == '\0')
		return;

	file = FsOpen(params, FILE_READ);
	if (file == NULL)
	{
		wprintf(L"Failed to open %s\n", params);
		return;
	}

	op.event = EvtAlloc();
	while (true)
	{
		if (!FsRead(file, buf, sizeof(buf), &op))
		{
			wprintf(L"%s: read failed (%d)\n", params, op.result);
			break;
		}

		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);

		len = op.bytes;
		if (len == 0)
			break;
		
		if (len < _countof(buf))
			buf[len] = '\0';
		/*len = mbstowcs(str, buf, _countof(buf) - 1);
		if (len == -1)
			wprintf(L"invalid multibyte sequence\n");
		else
			_cputws(str, len);*/
		for (i = 0; i < len; i++)
			str[i] = (wchar_t) (unsigned char) buf[i];
		_cputws(str, len);
	}
	
	HndClose(op.event);
	FsClose(file);
}

void ShCmdCls(const wchar_t *command, const wchar_t *params)
{
	_cputws(L"\x1b[2J", 4);
}

shell_command_t sh_commands[] =
{
	{ L"help",	ShCmdHelp,	1 },
	{ L"exit",	ShCmdExit,	2 },
	{ L"cd",	ShCmdCd,	3 },
	{ L"dir",	ShCmdDir,	4 },
	{ L"type",	ShCmdType,	5 },
	{ L"cls",	ShCmdCls,	6 },
	{ NULL,		NULL,		0 },
};

bool ShInternalCommand(const wchar_t *command, const wchar_t *params)
{
	unsigned i;

	if (*command == '\0')
		return true;
	else for (i = 0; sh_commands[i].name; i++)
		if (_wcsicmp(command, sh_commands[i].name) == 0)
		{
			sh_commands[i].func(command, params);
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
	wprintf(L"The M�bius: command-line shell\n");

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
			wcscat(buf, L".exe");
			wprintf(L"Starting %s...\n", buf);
			spawned = ProcSpawnProcess(buf);
			wprintf(L"Handle is %d\n", spawned);
			ThrSleep(2000);
			HndClose(spawned);
		}
	}

	return EXIT_SUCCESS;
}
