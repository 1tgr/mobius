/* $Id: shell.c,v 1.5 2002/02/24 19:13:30 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <iconv.h>

#include <os/syscall.h>
#include <os/rtl.h>

int _cputws(const wchar_t *str, size_t count);

bool sh_exit;
wchar_t sh_path[MAX_PATH];
iconv_t sh_iconv;

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
	return -1;
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
	if (search != NULL)
	{
		op.event = NULL;
		while (FsRead(search, &dir, sizeof(dir), &op))
		{
			if (op.result == SIOPENDING)
				ThrWaitHandle(op.event);

			if (dir.standard_attributes & FILE_ATTR_DIRECTORY)
				wprintf(L"[Directory]\t");
			else if (dir.standard_attributes & FILE_ATTR_DEVICE)
				wprintf(L"   [Device]\t");
			else
				wprintf(L"%10lu\t", (uint32_t) dir.length);

			wprintf(L"\t%s\n", dir.name);
		}

		FsClose(search);
	}
}

static void ShDumpFile(const wchar_t *name, void (*fn)(const void*, size_t))
{
	static char buf[8];

	handle_t file;
	size_t len;
	fileop_t op;
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
	{
		wprintf(L"Failed to open %s\n", name);
		return;
	}

	op.event = EvtAlloc();
	while (true)
	{
		if (!FsRead(file, buf, sizeof(buf), &op))
		{
			wprintf(L"%s: read failed (%d)\n", name, op.result);
			break;
		}

		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);

		len = op.bytes;
		if (len == 0)
			break;
		
		if (len < _countof(buf))
			buf[len] = '\0';
		fn(buf, len);
	}
	
	HndClose(op.event);
	FsClose(file);
}

static void ShTypeOutput(const void *buf, size_t len)
{
	static wchar_t str[2048];

	len = mbstowcs(str, buf, _countof(buf) - 1);
	if (len == -1)
		wprintf(L"invalid multibyte sequence\n");
	else
		_cputws(str, len);
	/*for (i = 0; i < len; i++)
		str[i] = (wchar_t) (unsigned char) buf[i];
	_cputws(str, len);*/
}

void ShCmdType(const wchar_t *command, const wchar_t *params)
{
	params = ShPrompt(L" File? ", params);
	if (*params == '\0')
		return;

	ShDumpFile(params, ShTypeOutput);
}

static void ShDumpOutput(const void *buf, size_t size)
{
	const uint8_t *ptr;
	int i, j;

	ptr = (const uint8_t*) buf;
	for (j = 0; j < size; j += i, ptr += i)
	{
		wprintf(L"%8x ", j);
		for (i = 0; i < 16; i++)
		{
			wprintf(L"%02x ", ptr[i]);
			if (i + j >= size)
				break;
		}

		wprintf(L"\n");
	}
}

void ShCmdDump(const wchar_t *command, const wchar_t *params)
{
	params = ShPrompt(L" File? ", params);
	if (*params == '\0')
		return;

	ShDumpFile(params, ShDumpOutput);
}

void ShCmdCls(const wchar_t *command, const wchar_t *params)
{
	_cputws(L"\x1b[2J", 4);
}

void ShCmdPoke(const wchar_t *command, const wchar_t *params)
{
	handle_t file;
	wchar_t str[] = L"Hello, world!\n";
	fileop_t op;

	params = ShPrompt(L" Device? ", params);
	if (*params == '\0')
		return;

	file = FsOpen(params, FILE_WRITE);
	if (file == NULL)
	{
		wprintf(L"Failed to open %s for writing\n", params);
		return;
	}

	op.event = file;
	if (FsWrite(file, str, sizeof(str), &op))
	{
		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);
	}
	else
		wprintf(L"Failed to write (%u)\n", op.result);

	FsClose(file);
}

shell_command_t sh_commands[] =
{
	{ L"help",	ShCmdHelp,	1 },
	{ L"exit",	ShCmdExit,	2 },
	{ L"cd",	ShCmdCd,	3 },
	{ L"dir",	ShCmdDir,	4 },
	{ L"type",	ShCmdType,	5 },
	{ L"cls",	ShCmdCls,	6 },
	{ L"poke",	ShCmdPoke,	7 },
	{ L"dump",	ShCmdDump,	8 },
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
	if (ResLoadString(proc->base, 4096, buf, _countof(buf)))
		_cputws(buf, wcslen(buf));
	/*sh_iconv = iconv_open("UCS-2", "ISO-8859-1");*/

	while (!sh_exit)
	{
		wprintf(L"[%s] ", proc->cwd);
		if (ShReadLine(buf, _countof(buf)) == -1)
			break;
		
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

			spawned = FsOpen(buf, 0);
			if (spawned)
			{
				FsClose(spawned);
				wprintf(L"Starting %s...\n", buf);
				spawned = ProcSpawnProcess(buf);
				wprintf(L"Handle is %d\n", spawned);
				ThrSleep(2000);
				HndClose(spawned);
			}
			else
				wprintf(L"%s: program not found\n", buf);
		}
	}

	/*iconv_close(sh_iconv);*/
	return EXIT_SUCCESS;
}
