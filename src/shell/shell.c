/* $Id: shell.c,v 1.15 2002/03/07 15:52:03 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <iconv.h>
#include <string.h>
#include <unistd.h>    /* just for sbrk */

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/keyboard.h>

#include "common.h"

bool sh_exit;
wchar_t sh_path[MAX_PATH];
iconv_t sh_iconv;

void ShCtrlCThread(void *param)
{
    handle_t keyboard;
    fileop_t op;
    uint32_t key;

    keyboard = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
    op.event = keyboard;
    while (FsRead(keyboard, &key, sizeof(key), &op))
    {
	if (op.result == SIOPENDING)
	    ThrWaitHandle(op.event);

	if (op.result != 0)
	    break;

	if (key == (KBD_BUCKY_CTRL | 'c'))
	{
	    printf("ShCtrlCThread: exiting\n");
	    ProcExitProcess(0);
	}
    }

    _pwerror(L"ShCtrlCThread");
    FsClose(keyboard);
    ThrExitThread(0);
}

/*void ShCtrlCThread(void *param)
{
    char ch;
    for (;;)
    {
	do
	{
	    ch = ShReadKey();
	} while (ch == 0);
	_cputs(&ch, 1);
	fflush(stdout);
    }
}*/

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
		wprintf(L" ", 1);
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
		if (!ResLoadString(NULL, sh_commands[i].help_id, text, _countof(text)) ||
		    text[0] == '\0')
		    swprintf(text, L"no string for ID %u", sh_commands[i].help_id);
		_cputws(sh_commands[i].name, wcslen(sh_commands[i].name));
		_cputs(": ", 2);
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
    dirent_device_t dev;
    fileop_t op;
    wchar_t *star;
    
    if (*params == '\0')
	params = L"*";

    if (!FsFullPath(params, sh_path))
    {
	_pwerror(params);
	return;
    }

    star = wcsrchr(sh_path, '*');
    if (star == NULL)
	star = sh_path + wcslen(sh_path);

    search = FsOpenSearch(sh_path);
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

	    wprintf(L"\t%s", dir.name);
	    if (dir.standard_attributes & FILE_ATTR_DEVICE)
	    {
		wcscpy(star, dir.name);
		if (FsQueryFile(sh_path, FILE_QUERY_DEVICE, &dev, sizeof(dev)))
		{
		    printf("%*s", 20 - wcslen(dir.name) + 1, "");
		    _cputws(dev.description, wcslen(dev.description));
		}
	    }

	    _cputs("\n", 1);
	}

	FsClose(search);
    }
}

static void ShDumpFile(const wchar_t *name, void (*fn)(const void*, addr_t, size_t))
{
    static char buf[512];

    handle_t file;
    size_t len;
    fileop_t op;
    addr_t origin;
    
    file = FsOpen(name, FILE_READ);
    if (file == NULL)
    {
	_pwerror(name);
	return;
    }

    op.event = EvtAlloc();
    origin = 0;
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
	fn(buf, origin, len);

	origin += len;
	if (len < sizeof(buf))
	    /*
	     * FSD hit the end of the file: successful but fewer bytes read 
	     *	  than requested
	     */
	    break;
    }
    
    HndClose(op.event);
    FsClose(file);
}

static void ShTypeOutput(const void *buf, addr_t origin, size_t len)
{
    _cputs(buf, len);
}

void ShCmdType(const wchar_t *command, wchar_t *params)
{
    FILE *f;

    params = ShPrompt(L" File? ", params);
    if (*params == '\0')
	return;

    /*ShDumpFile(params, ShTypeOutput);*/
    f = _wfopen(params, L"rt");
    if (f == NULL)
    	_pwerror(params);
    else
        fclose(f);
}

static void ShDumpOutput(const void *buf, addr_t origin, size_t size)
{
    /*const uint8_t *ptr;
    int i, j;

    ptr = (const uint8_t*) buf;
    for (j = 0; j < size; j += i, ptr += i)
    {
	wprintf(L"%8x ", origin + j);
	for (i = 0; i < 16; i++)
	{
	    wprintf(L"%02x ", ptr[i]);
	    if (i + j >= size)
		break;
	}

	wprintf(L"\n");
    }*/
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
    wchar_t buf[MAX_PATH], *space;
    handle_t spawned;
    static process_info_t info;

    params = ShPrompt(L" Program? ", params);
    if (*params == '\0')
	return;

    space = wcschr(params, ' ');
    if (space)
    {
	wcsncpy(buf, params, space - params);
	wcscpy(buf + (space - params), L".exe");
    }
    else
    {
	wcscpy(buf, params);
	wcscat(buf, L".exe");
    }

    spawned = FsOpen(buf, 0);
    if (spawned)
    {
	FsClose(spawned);
	info = *ProcGetProcessInfo();
	memset(info.cmdline, 0, sizeof(info.cmdline));
	wcsncpy(info.cmdline, params, _countof(info.cmdline));
	spawned = ProcSpawnProcess(buf, &info);
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

    doWait = false;
    if (ShHasParam(names, L"wait"))
	doWait = true;
    
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
	    wprintf(L"   System quantum: %u ms\n",
		times.quantum);
	    ShSplitTime(times.uptime, &h, &m, &s, &ms);
	    wprintf(L"    System uptime: %02u:%02u:%02u.%03u\n",
		h, m, s, ms);
	    ShSplitTime(times.current_cputime, &h, &m, &s, &ms);
	    wprintf(L"CPU time (thread): %02u:%02u:%02u.%03u (average %u%% CPU usage)\n",
		h, m, s, ms, (times.current_cputime * 100) / times.uptime);
	}
	else
	    _pwerror(L"SysGetTimes");
    }

    free(names);
    free(values);
}

void ShCmdV86(const wchar_t *cmd, wchar_t *params)
{
    static uint8_t *stack;
    uint8_t *code;
    psp_t *psp;
    FARPTR fp_code, fp_stackend;
    handle_t thr, file;
    dirent_t di;
    fileop_t op;
    bool doWait;
    wchar_t **names, **values;

    ShParseParams(params, &names, &values, &params);
    params = ShPrompt(L" .COM file? ", params);
    if (*params == '\0')
	return;

    doWait = true;
    if (ShHasParam(names, L"nowait"))
	doWait = false;
    
    free(names);
    free(values);

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
    psp = (psp_t*) code;
    memset(psp, 0, sizeof(*psp));
    psp->int20 = 0x20cd;

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
    if (doWait)
	ThrWaitHandle(thr);

    /* xxx - need to clean up HndClose() implementation before we can use this */
    /*HndClose(thr);*/
}

shell_command_t sh_commands[] =
{
    { L"cd",	    ShCmdCd,	    3 },
    { L"cls",	    ShCmdCls,	    6 },
    { L"detach",    ShCmdDetach,    10 },
    { L"dir",	    ShCmdDir,	    4 },
    { L"dump",	    ShCmdDump,	    8 },
    { L"exit",	    ShCmdExit,	    2 },
    { L"help",	    ShCmdHelp,	    1 },
    { L"info",	    ShCmdInfo,	    12 },
    { L"parse",     ShCmdParse,	    11 },
    { L"poke",	    ShCmdPoke,	    7 },
    { L"r",	    ShCmdRun,	    9 },
    { L"run",	    ShCmdRun,	    9 },
    { L"type",	    ShCmdType,	    5 },
    { L"v86",	    ShCmdV86,	    13 },
    { NULL,	    NULL,	    0 },
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
    wchar_t str[256], *buf, *space;
    process_info_t *proc;
    shell_line_t *line;
    /*char inbuf[256], *outptr;
    const char *inptr;
    size_t inbytes, outbytes, len;*/
    
    /*ThrCreateThread(ShCtrlCThread, NULL, 16);*/

    proc = ProcGetProcessInfo();
    if (ResLoadString(proc->base, 4096, str, _countof(str)))
    	_cputws(str, wcslen(str));
    
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
	fflush(stdout);

	line = ShReadLine();
	if (line == NULL)
	{
	    _pwerror(L"ShReadLine");
	    break;
	}
	
	buf = _wcsdup(line->text);
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
