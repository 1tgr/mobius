/* $Id: shell.c,v 1.8 2002/02/26 15:46:34 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <iconv.h>
#include <string.h>

#include <os/syscall.h>
#include <os/rtl.h>

#define PARAMSEP	'\\'

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

void _pwerror(const wchar_t *text)
{
	wchar_t str[1024];
	int en;

	en = errno;
	if (en < 0)
		en = 0;

	if (!ResLoadString(NULL, en + 1024, str, _countof(str)))
		swprintf(str, L"errno = %d", en);

	wprintf(L"%s: %s\n", text, str);		
}

void ShCmdHelp(const wchar_t *command, wchar_t *params)
{
	wchar_t text[1024];
	unsigned i, j;
	bool onlyOnce;

	if (*params == '\0')
	{
		onlyOnce = false;
		wprintf(L"Valid commands are:\n");
		for (i = 0; sh_commands[i].name != NULL; i++)
		{
			for (j = wcslen(sh_commands[i].name); j < 8; j++)
				_cputws(L" ", 1);
			wprintf(L"%s", sh_commands[i].name);
			if (i > 0 && i % 8 == 0)
				wprintf(L"\n");
		}
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

static bool ShNormalizePath(wchar_t *path)
{
	wchar_t *ch, *prev;
	dirent_t di;
	bool isEnd;

	if (*path == '/')
	{
		ch = path;
		prev = path + 1;
	}
	else
	{
		ch = path - 1;
		prev = path;
	}

	isEnd = false;
	while (!isEnd)
	{
		ch = wcschr(ch + 1, '/');
		if (ch == NULL)
		{
			ch = path + wcslen(path);
			isEnd = true;
		}

		*ch = '\0';
		if (!FsQueryFile(path, FILE_QUERY_STANDARD, &di, sizeof(di)))
			return false;

		if ((di.standard_attributes & FILE_ATTR_DIRECTORY) == 0)
		{
			errno = ENOTADIR;
			return false;
		}

		wcscpy(prev, di.name);
		prev = ch + 1;
		*ch = '/';
	}

	*ch = '\0';
	return true;
}

void ShCmdCd(const wchar_t *command, wchar_t *params)
{
	params = ShPrompt(L" Directory? ", params);
	if (*params == '\0')
		return;

	if (FsFullPath(params, sh_path))
	{
		if (ShNormalizePath(sh_path))
			wcscpy(ProcGetProcessInfo()->cwd, sh_path);
		else
			_pwerror(sh_path);
	}
	else
		_pwerror(params);
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
		_pwerror(params);
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
	static char buf[2048];

	handle_t file;
	size_t len;
	fileop_t op;
	
	file = FsOpen(name, FILE_READ);
	if (file == NULL)
	{
		_pwerror(name);
		return;
	}

	op.event = EvtAlloc();
	while (true)
	{
		if (!FsRead(file, buf, sizeof(buf), &op))
		{
			/* Some catastrophic error */
			errno = op.result;
			_pwerror(name);
			break;
		}

		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);

		len = op.bytes;
		if (len == 0)
			/* FSD read zero bytes but didn't report an error */
			break;
		
		if (len < _countof(buf))
			buf[len] = '\0';
		fn(buf, len);

		if (len < sizeof(buf))
			/*
			 * FSD hit the end of the file: successful but fewer bytes read 
			 *	than requested
			 */
			break;
	}
	
	HndClose(op.event);
	FsClose(file);
}

static void ShTypeOutput(const void *buf, size_t len)
{
	static wchar_t str[2048];
	size_t bytes_converted, i;

	bytes_converted = mbstowcs(str, buf, _countof(str) - 1);
	if (bytes_converted == -1)
	{
		for (i = 0; i < len; i++)
			str[i] = (wchar_t) ((unsigned char*) buf)[i];
		_cputws(str, len);
	}
	else
		_cputws(str, bytes_converted);
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
		_pwerror(params);
		return;
	}

	op.event = file;
	if (FsWrite(file, str, sizeof(str), &op))
	{
		if (op.result == SIOPENDING)
			ThrWaitHandle(op.event);
	}
	else
	{
		errno = op.result;
		_pwerror(params);
	}

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
		_pwerror(buf);
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
		out = NULL;
	
	in = ShFindParam(names, values, L"in");
	if (*in == '\0')
		in = NULL;
	
	free(names);
	free(values);

	wcscpy(buf, params);
	wcscat(buf, L".exe");
	
	spawned = FsOpen(buf, 0);
	if (spawned)
	{
		FsClose(spawned);
		proc = *ProcGetProcessInfo();
		if (in)
			proc.std_in = FsOpen(in, FILE_READ);
		if (out)
			proc.std_out = FsOpen(out, FILE_WRITE);
		spawned = ProcSpawnProcess(buf, &proc);
		if (in)
			FsClose(proc.std_in);
		if (out)
			FsClose(proc.std_out);
		if (doWait)
			ThrWaitHandle(spawned);
		HndClose(spawned);
	}
	else
		_pwerror(buf);
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

static void ShSplitTime(unsigned in, unsigned *h, unsigned *m, unsigned *s, unsigned *ms)
{
	*h = in / 3600000;
	in -= *h * 3600000;
	*m = in / 60000;
	in -= *m * 60000;
	*s = in / 1000;
	*ms = in - *s * 1000;
}

void ShCmdInfo(const wchar_t *command, wchar_t *params)
{
	wchar_t **names, **values;
	sysinfo_t info;
	systimes_t times;
	
	ShParseParams(params, &names, &values, &params);

	if (ShHasParam(names, L"system"))
	{
		if (SysGetInfo(&info))
		{
			size_t reserved_pages;
			reserved_pages = info.pages_physical - info.pages_total;
			wprintf(L"         System page size: %uKB\n", 
				info.page_size / 1024);
			wprintf(L"    Total physical memory: %uMB\n", 
				(info.pages_physical * info.page_size) / 0x100000);
			wprintf(L"Available physical memory: %uMB (%u%%)\n", 
				(info.pages_total * info.page_size) / 0x100000,
				(info.pages_total * 100) / info.pages_physical);
			wprintf(L" Reserved physical memory: %uMB (%u%%)\n", 
				(reserved_pages * info.page_size) / 0x100000,
				100 - (info.pages_total * 100) / info.pages_physical);
			wprintf(L"     Free physical memory: %uMB (%u%% of total, %u%% of available)\n", 
				(info.pages_free * info.page_size) / 0x100000,
				(info.pages_free * 100) / info.pages_physical,
				(info.pages_free * 100) / info.pages_total);
			wprintf(L"   Kernel reserved memory: %uKB (%u%% of total, %u%% of reserved)\n",
				(info.pages_kernel * info.page_size) / 1024,
				(info.pages_kernel * 100) / info.pages_physical,
				(info.pages_kernel * 100) / reserved_pages);
		}
		else
			_pwerror(L"SysGetInfo");
	}

	if (ShHasParam(names, L"times"))
	{
		if (SysGetTimes(&times))
		{
			unsigned ms, s, m, h;
			wprintf(L"      System quantum: %u ms\n",
				times.quantum);
			ShSplitTime(times.uptime, &h, &m, &s, &ms);
			wprintf(L"       System uptime: %02u:%02u:%02u.%03u\n",
				h, m, s, ms);
			ShSplitTime(times.current_cputime, &h, &m, &s, &ms);
			wprintf(L"CPU time (this task): %02u:%02u:%02u.%03u (average %u%% CPU usage)\n",
				h, m, s, ms, (times.current_cputime * 100) / times.uptime);
		}
		else
			_pwerror(L"SysGetTimes");
	}

	free(names);
	free(values);
}

FARPTR i386LinearToFp(void *ptr)
{
	unsigned seg, off;
	off = (addr_t) ptr & 0xf;
	seg = ((addr_t) ptr - ((addr_t) ptr & 0xf)) / 16;
	return MK_FP(seg, off);
}

char *sbrk(size_t diff);

void ShInt21(context_v86_t *ctx)
{
	void *ptr;
	char *ch;
	uint8_t b;
	
	switch ((uint16_t) ctx->regs.eax >> 8)
	{
	case 0:
		ThrExitThread(0);
		break;

	case 1:
		do
		{
			b = (uint8_t) ShReadChar();
		} while (b == 0);

		wprintf(L"%c", b);
		ctx->regs.eax = (ctx->regs.eax & 0xffffff00) | b;
		break;

	case 2:
		wprintf(L"%c", ctx->regs.edx & 0xff);
		break;

	case 9:
		ptr = FP_TO_LINEAR(ctx->v86_ds, ctx->regs.edx);
		ch = strchr(ptr, '$');
		_cputs(ptr, ch - (char*) ptr);
		break;

	case 0x4c:
		ThrExitThread(ctx->regs.eax & 0xff);
		break;
	}
}

void ShV86Handler(void)
{
	context_v86_t ctx;
	uint8_t *ip;

	ThrGetV86Context(&ctx);
	
	ip = FP_TO_LINEAR(ctx.cs, ctx.eip);
	switch (ip[0])
	{
	case 0xcd:
		switch (ip[1])
		{
		case 0x20:
			ThrExitThread(0);
			break;

		case 0x21:
			ShInt21(&ctx);
			break;
		}

		ctx.eip += 2;
		break;

	default:
		wprintf(L"v86: illegal instruction %x\n", ip[0]);
		ThrExitThread(0);
		break;
	}

	ThrSetV86Context(&ctx);
	ThrContinueV86();
}

#if 0
		0x31, 0xc0,		/* xor ax, ax */
		0x8e, 0xd8,		/* mov ds, ax */
		0x83, 0xc0,		/* mov es, ax */
		0x40,			/* inc ax */
		0xeb, 0xfd,		/* jmp 0 */
#elif 0
		0x83, 0xE9, 0x01,
		0xE2, 0xFB,
		0xC3,
#elif 0
		0xB8, 0x00, 0xA0,						/* mov ax, 0xb800 */
		0x8E, 0xC0,								/* mov es, ax*/
		0x26,									/* es: */
		0xC7, 0x06, 0x00, 0x00, 0x34, 0x12,		/* mov [0], word 0x1234 */
		0xC7, 0x06, 0x02, 0x00, 0x34, 0x12,		/* mov [2], word 0x1234 */
		0xC7, 0x06, 0x04, 0x00, 0x34, 0x12,		/* mov [2], word 0x1234 */
		0x66, 0xB8, 0x00, 0x02, 0x00, 0x00,		/* mov eax,0x200 */
		0xCD, 0x30,								/* int 0x30 */
#elif 0
		0x8C, 0xC8,			/* mov ax, cs */
		0x8E, 0xD8,			/* mov ds, ax */
		0xB4, 0x09,			/* mov ah, 9 */
		0xBA, 0x0C, 0x01,	/* mov dx, 0x10c */
		0xCD, 0x21,			/* int 0x21 */
		0xC3,				/* ret */
		'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\n', '$'
#endif

void ShCmdV86(const wchar_t *cmd, wchar_t *params)
{
	static const uint8_t code_exit[] =
	{
#if 0
		0x66, 0xB8, 0x00, 0x02, 0x00, 0x00,		/* mov eax,0x200 */
		0xCD, 0x30,								/* int 0x30 */
#else
		0xCD, 0x20,
#endif
	};

	static uint8_t *stack;
	uint8_t *code;
	FARPTR fp_code, fp_stackend;
	handle_t thr, file;
	dirent_t di;
	fileop_t op;

	params = ShPrompt(L" .COM file? ", params);
	if (*params == NULL)
		return;

	/*if (!FsQueryFile(params, FILE_QUERY_STANDARD, &di, sizeof(di)))
	{
		_pwerror(params);
		return;
	}*/
	di.length = 0x10000;

	file = FsOpen(params, FILE_READ);
	if (file == NULL)
	{
		wprintf(L"FsOpen: ");
		_pwerror(params);
		return;
	}

	code = sbrk(di.length + 0x100);
	memcpy(code, code_exit, sizeof(code_exit));

	op.event = file;
	if (!FsRead(file, code + 0x100, di.length, &op))
	{
		wprintf(L"FsRead: ");
		errno = op.result;
		_pwerror(params);
		FsClose(file);
		return;
	}

	if (op.result == SIOPENDING)
		ThrWaitHandle(op.event);

	FsClose(file);
	if (op.result > 0)
	{
		wprintf(L"FsRead(finished): ");
		errno = op.result;
		_pwerror(params);
		return;
	}

	fp_code = i386LinearToFp(code);
	fp_code = MK_FP(FP_SEG(fp_code), FP_OFF(fp_code) + 0x100);
		
	if (stack == NULL)
		stack = sbrk(65536);

	fp_stackend = i386LinearToFp(stack);
	memset(stack, 0, 65536);
	
	thr = ThrCreateV86Thread(fp_code, fp_stackend, 15, ShV86Handler);
	ThrWaitHandle(thr);
	/*HndClose(thr);*/
}

shell_command_t sh_commands[] =
{
	{ L"cd",		ShCmdCd,		3 },
	{ L"cls",		ShCmdCls,		6 },
	{ L"detach",	ShCmdDetach,	10 },
	{ L"dir",		ShCmdDir,		4 },
	{ L"dump",		ShCmdDump,		8 },
	{ L"exit",		ShCmdExit,		2 },
	{ L"help",		ShCmdHelp,		1 },
	{ L"info",		ShCmdInfo,		12 },
	{ L"parse",		ShCmdParse,		11 },
	{ L"poke",		ShCmdPoke,		7 },
	{ L"r",			ShCmdRun,		9 },
	{ L"run",		ShCmdRun,		9 },
	{ L"type",		ShCmdType,		5 },
	{ L"v86",		ShCmdV86,		13 },
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
	/*char inbuf[256], *outptr;
	const char *inptr;
	size_t inbytes, outbytes, len;*/
	
	proc = ProcGetProcessInfo();
	if (ResLoadString(proc->base, 4096, buf, _countof(buf)))
		_cputws(buf, wcslen(buf));
	/*sh_iconv = iconv_open("UCS-2LE", "ISO-8859-7");

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
			inbytes, outbytes);*/

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

	/*iconv_close(sh_iconv);*/
	return EXIT_SUCCESS;
}
