/* $Id: shell.c,v 1.6 2002/02/25 01:28:14 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <iconv.h>
#include <string.h>

#include <os/syscall.h>
#include <os/rtl.h>

#define PARAMSEP	'+'

int _cputws(const wchar_t *str, size_t count);

bool sh_exit;
wchar_t sh_path[MAX_PATH];
iconv_t sh_iconv;

typedef struct shell_command_t shell_command_t;
struct shell_command_t
{
	const wchar_t *name;
	void (*func)(const wchar_t*, wchar_t*);
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

wchar_t *ShPrompt(const wchar_t *prompt, wchar_t *params)
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

unsigned ShParseParams(wchar_t *params, wchar_t ***names, wchar_t ***values, 
					   wchar_t ** unnamed)
{
	unsigned num_names;
	wchar_t *ch;
	int state;

	num_names = 0;
	state = 0;
	if (unnamed)
		*unnamed = L"";
	for (ch = params; *ch;)
	{
		switch (state)
		{
		case 0:	/* normal character */
			if (*ch == PARAMSEP ||
				ch == params ||
				(iswspace(*ch) &&
				ch[1] != '\0' &&
				!iswspace(ch[1]) &&
				ch[1] != PARAMSEP))
			{
				state = 1;
				num_names++;
			}
			break;

		case 1:	/* part of name */
			if (iswspace(*ch) ||
				*ch == PARAMSEP)
			{
				state = 0;
				continue;
			}
			break;
		}

		ch++;
	}

	*names = malloc(sizeof(wchar_t*) * (num_names + 1));
	*values = malloc(sizeof(wchar_t*) * (num_names + 1));
	(*names)[num_names] = NULL;
	(*values)[num_names] = NULL;

	num_names = 0;
	state = 0;
	for (ch = params; *ch;)
	{
		switch (state)
		{
		case 0:	/* normal character */
			if (*ch == PARAMSEP)
			{
				/* option with name */
				state = 1;
				*ch = '\0';
				(*names)[num_names] = ch + 1;
				(*values)[num_names] = L"";
				num_names++;
			}
			else if (iswspace(*ch))
			{
				/* unnamed option */
				*ch = '\0';
				if (ch[1] != '\0' &&
					!iswspace(ch[1]) &&
					ch[1] != PARAMSEP)
				{
					state = 1;
					(*names)[num_names] = L"";
					(*values)[num_names] = ch + 1;
					num_names++;
					if (unnamed && **unnamed == '\0')
						*unnamed = ch + 1;
				}
			}
			else if (ch == params)
			{
				/* unnamed option at start of string */
				state = 1;
				(*names)[num_names] = L"";
				(*values)[num_names] = ch;
				num_names++;
				if (unnamed && **unnamed == '\0')
					*unnamed = ch;
			}
			break;

		case 1:	/* part of name */
			if (iswspace(*ch) ||
				*ch == PARAMSEP)
			{
				state = 0;
				continue;
			}
			else if (*ch == '=')
			{
				*ch = '\0';
				(*values)[num_names - 1] = ch + 1;
			}
			break;
		}

		 ch++;
	}

	return num_names;
}

bool ShHasParam(wchar_t **names, const wchar_t *look)
{
	unsigned i;
	for (i = 0; names[i]; i++)
		if (_wcsicmp(names[i], look) == 0)
			return true;
	return false;
}

wchar_t *ShFindParam(wchar_t **names, wchar_t **values, const wchar_t *look)
{
	unsigned i;
	for (i = 0; names[i]; i++)
		if (_wcsicmp(names[i], look) == 0)
			return values[i];
	return L"";
}

void ShCmdHelp(const wchar_t *command, wchar_t *params)
{
	wchar_t text[1024];
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
				wprintf(L"%s: ", sh_commands[i].name);
				_cputws(text, wcslen(text));
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

void ShCmdExit(const wchar_t *command, wchar_t *params)
{
	sh_exit = true;
}

void ShCmdCd(const wchar_t *command, wchar_t *params)
{
	params = ShPrompt(L" Directory? ", params);
	if (*params == '\0')
		return;

	if (FsFullPath(params, sh_path))
		wcscpy(ProcGetProcessInfo()->cwd, sh_path);
	else
		wprintf(L"%s: invalid path\n", params);
}

void ShCmdDir(const wchar_t *command, wchar_t *params)
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

void ShCmdType(const wchar_t *command, wchar_t *params)
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

void ShCmdDump(const wchar_t *command, wchar_t *params)
{
	params = ShPrompt(L" File? ", params);
	if (*params == '\0')
		return;

	ShDumpFile(params, ShDumpOutput);
}

void ShCmdCls(const wchar_t *command, wchar_t *params)
{
	_cputws(L"\x1b[2J", 4);
}

void ShCmdPoke(const wchar_t *command, wchar_t *params)
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

void ShCmdRun(const wchar_t *command, wchar_t *params)
{
	wchar_t buf[MAX_PATH];
	handle_t spawned;

	params = ShPrompt(L" Program? ", params);
	if (*params == '\0')
		return;

	wcscpy(buf, params);
	wcscat(buf, L".exe");

	spawned = FsOpen(buf, 0);
	if (spawned)
	{
		FsClose(spawned);
		spawned = ProcSpawnProcess(buf, ProcGetProcessInfo());
		ThrWaitHandle(spawned);
		HndClose(spawned);
	}
	else
		wprintf(L"%s: program not found\n", buf);
}

void ShCmdDetach(const wchar_t *command, wchar_t *params)
{
	wchar_t buf[MAX_PATH], *out, *in;
	handle_t spawned;
	process_info_t proc;
	wchar_t **names, **values;
	bool doWait;

	ShParseParams(params, &names, &values, &params);
	params = ShPrompt(L" Program? ", params);
	if (*params == '\0')
		return;

	doWait = ShHasParam(names, L"wait");
	
	out = ShFindParam(names, values, L"out");
	if (*out == '\0')
		out = SYS_DEVICES L"/tty1";
	
	in = ShFindParam(names, values, L"in");
	if (*in == '\0')
		in = SYS_DEVICES L"/tty1";
	
	free(names);
	free(values);

	wcscpy(buf, params);
	wcscat(buf, L".exe");
	
	spawned = FsOpen(buf, 0);
	if (spawned)
	{
		FsClose(spawned);
		proc = *ProcGetProcessInfo();
		proc.std_in = FsOpen(in, FILE_READ);
		proc.std_out = FsOpen(out, FILE_WRITE);
		spawned = ProcSpawnProcess(buf, &proc);
		FsClose(proc.std_in);
		FsClose(proc.std_out);
		if (doWait)
			ThrWaitHandle(spawned);
		HndClose(spawned);
	}
	else
		wprintf(L"%s: program not found\n", buf);
}

void ShCmdParse(const wchar_t *command, wchar_t *params)
{
	wchar_t **names, **values;
	unsigned num_names, i;

	wprintf(L"Raw params: \"%s\"\n", params);
	num_names = ShParseParams(params, &names, &values, NULL);
	for (i = 0; i < num_names; i++)
		wprintf(L"\"%s\" => \"%s\"\n", names[i], values[i]);
	
	free(names);
	free(values);
}

shell_command_t sh_commands[] =
{
	{ L"help",		ShCmdHelp,		1 },
	{ L"exit",		ShCmdExit,		2 },
	{ L"cd",		ShCmdCd,		3 },
	{ L"dir",		ShCmdDir,		4 },
	{ L"type",		ShCmdType,		5 },
	{ L"cls",		ShCmdCls,		6 },
	{ L"poke",		ShCmdPoke,		7 },
	{ L"dump",		ShCmdDump,		8 },
	{ L"run",		ShCmdRun,		9 },
	{ L"r",			ShCmdRun,		9 },
	{ L"detach",	ShCmdDetach,	10 },
	{ L"parse",		ShCmdParse,		11 },
	{ NULL,			NULL,			0 },
};

bool ShInternalCommand(const wchar_t *command, wchar_t *params)
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
	char inbuf[256], *outptr;
	const char *inptr;
	size_t inbytes, outbytes, len;
	
	proc = ProcGetProcessInfo();
	if (ResLoadString(proc->base, 4096, buf, _countof(buf)))
		_cputws(buf, wcslen(buf));
	sh_iconv = iconv_open("UCS-2LE", "ISO-8859-7");

	strcpy(inbuf, "Hello, world: £€\n");
	inbytes = strlen(inbuf);
	outbytes = sizeof(buf);
	inptr = inbuf;
	outptr = (char*) buf;
	len = iconv(sh_iconv, &inptr, &inbytes, &outptr, &outbytes);
	if (len != -1)
	{
		*((wchar_t*) outptr) = '\0';
		ShDumpOutput(buf, wcslen(buf) * sizeof(wchar_t));
		_cputws(buf, wcslen(buf));
	}
	else
		wprintf(L"invalid input sequence: inbytes = %u, outbytes = %u\n", 
			inbytes, outbytes);

	while (!sh_exit)
	{
		wprintf(L"[%s] ", proc->cwd);
		if (ShReadLine(buf, _countof(buf)) == -1)
			break;
		
		space = wcschr(buf, PARAMSEP);
		if (space)
		{
			memmove(space + 1, space, (wcslen(space) + 1) * sizeof(wchar_t));
			*space = ' ';
		}

		space = wcschr(buf, ' ');
		if (space)
		{
			*space = '\0';
			space++;
		}
		else
			space = buf + wcslen(buf);

		if (!ShInternalCommand(buf, space))
			wprintf(L"%s: invalid command\n", buf);
	}

	iconv_close(sh_iconv);
	return EXIT_SUCCESS;
}
