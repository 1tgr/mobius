/* $Id: shell.c,v 1.1.1.1 2002/12/21 09:51:10 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/keyboard.h>

#include "common.h"

bool sh_exit;
wchar_t sh_path[MAX_PATH];
int sh_width, sh_height;

void ShCtrlCThread(void *param)
{
    handle_t keyboard;
    fileop_t op;
    uint32_t key;
	status_t ret;

    keyboard = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
    op.event = keyboard;
    while ((ret = FsReadAsync(keyboard, &key, 0, sizeof(key), &op)) <= 0)
    {
        if (ret == SIOPENDING)
		{
            ThrWaitHandle(op.event);
			ret = op.result;
		}

        if (ret != 0)
            break;

        if (key == (KBD_BUCKY_CTRL | 'c'))
        {
            printf("ShCtrlCThread: exiting\n");
            ProcExitProcess(0);
        }
    }

    _pwerror(L"ShCtrlCThread");
    HndClose(keyboard);
    ThrExitThread(0);
}


bool ShGetTerminalSize(int *width, int *height)
{
	char str[50], *ch, *semicolon;
	size_t length;

	printf("\x1b[s"
		"\x1b[999;999f"
		"\x1b[6n");
	fflush(stdout);

	ch = str;
	while (ch < str + _countof(str) - 1)
	{
		*ch++ = fgetc(stdin);
		if (ch[-1] == 'R')
			break;
	}

	*ch = '\0';
	length = ch - str;
	if (str[0] == '\x1b' &&
		str[1] == '[' &&
		str[length - 1] == 'R')
	{
		str[length - 1] = '\0';
		semicolon = strchr(str + 2, ';');
		if (semicolon == NULL)
		{
			*height = atoi(str + 2) + 1;
			*width = 1;
		}
		else
		{
			*semicolon = '\0';
			*height = atoi(str + 2) + 1;
			*width = atoi(semicolon + 1) + 1;
		}
	}
	else
	{
		*width = 1;
		*height = 1;
	}

	printf("\x1b[u");
	fflush(stdout);
	return true;
}


void ShCmdExit(const wchar_t *command, wchar_t *params)
{
    sh_exit = true;
}


void ShCmdCls(const wchar_t *command, wchar_t *params)
{
    _cputws(L"\x1b[2J", 4);
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


void ShCmdOff(const wchar_t *cmd, wchar_t *params)
{
    SysShutdown(SHUTDOWN_POWEROFF);
}


void ShCmdSleep(const wchar_t *cmd, wchar_t *params)
{
    params = ShPrompt(L" Delay? ", params);
    if (*params == '\0')
        return;

    ThrSleep(wcstol(params, NULL, 0));
}


void ShCmdEcho(const wchar_t *cmd, wchar_t *params)
{
    wprintf(L"%s\n", params);
}


void ShCmdCrash(const wchar_t *cmd, wchar_t *params)
{
	printf("Breakpoint\n");
    __asm__("int3");
	printf("Write to kernel memory\n");
    strcpy((char*) 0xf0000000, "hello");
}


shell_command_t sh_commands[] =
{
	{ L"cd",		ShCmdCd,		3 },
	{ L"cd..",		ShCmdCdDotDot,	3 },
	{ L"cls",		ShCmdCls,		6 },
	{ L"color",		ShCmdColour,	23 },
	{ L"colour",	ShCmdColour,	23 },
	{ L"crash",		ShCmdCrash,		20 },
	{ L"detach",	ShCmdDetach,	10 },
	{ L"device",	ShCmdDevice,	22 },
	{ L"dir",		ShCmdDir,		4 },
	{ L"dismount",	ShCmdDismount,	18 },
	{ L"dump",		ShCmdDump,		8 },
	{ L"echo",		ShCmdEcho,		16 },
	{ L"exit",		ShCmdExit,		2 },
	{ L"help",		ShCmdHelp,		1 },
	{ L"info",		ShCmdInfo,		12 },
	{ L"mount",		ShCmdMount,		17 },
	{ L"off",		ShCmdOff,		14 },
	{ L"parse",		ShCmdParse,		11 },
	{ L"pipe",		ShCmdPipe,		19 },
	{ L"poke",		ShCmdPoke,		7 },
	{ L"sleep",		ShCmdSleep,		15 },
	{ L"test",		ShCmdTest,		24 },
	{ L"thrash",	ShCmdThrash,	21 },
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


shell_action_t sh_actions[] =
{
    { L"edit",  L"text/*",  L"/hd/mobius/ziing", },
    { L"play",  L"audio/*", L"/hd/mobius/audio", },
    { NULL,     NULL,       NULL, },
};


handle_t ShExecProcess(const wchar_t *command, const wchar_t *params, bool wait)
{
    wchar_t buf[MAX_PATH], *space;
    handle_t spawned;
    static process_info_t info;

    if (command == NULL ||
        *command == '\0')
    {
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
    }
    else
    {
        wcscpy(buf, command);
        wcscat(buf, L".exe");
    }

    spawned = FsOpen(buf, 0);
    if (spawned)
    {
        int code;

        HndClose(spawned);
        info = *ProcGetProcessInfo();
        memset(info.cmdline, 0, sizeof(info.cmdline));
        //wcsncpy(info.cmdline, params, _countof(info.cmdline) - 1);

        if (*params == '\0')
            wcscpy(info.cmdline, buf);
        else
            swprintf(info.cmdline, L"%s %s", buf, params);

        spawned = ProcSpawnProcess(buf, &info);

        if (wait)
        {
            ThrWaitHandle(spawned);
            code = ProcGetExitCode(spawned);
            wprintf(L"The process %s has exited with code %d\n", 
                buf, code);
            HndClose(spawned);
        }

        return spawned;
    }
    else
        return false;
}


bool ShAction(const wchar_t *command, wchar_t *params)
{
    unsigned i;
    dirent_standard_t info;
    bool got_dirent;

    got_dirent = false;
    for (i = 0; sh_actions[i].name != NULL; i++)
    {
        if (_wcsicmp(command, sh_actions[i].name) == 0)
        {
            params = ShPrompt(L" File? ", params);
            if (*params == '\0')
                return true;

            if (!got_dirent)
            {
                if (!FsQueryFile(params, FILE_QUERY_STANDARD, &info, sizeof(info)))
                {
                    _pwerror(params);
                    return true;
                }

                got_dirent = true;
            }

            if (_wcsmatch(info.mimetype, sh_actions[i].mimetype) == 0)
            {
                if (ShExecProcess(sh_actions[i].program, params, true) == NULL)
                    _pwerror(sh_actions[i].program);
                return true;
            }
        }
    }

    if (got_dirent)
    {
        wprintf(L"%s: no %s action for %s files\n",
            params, command, info.mimetype);
        return true;
    }
    else
        return false;
}


bool ShExternalCommand(const wchar_t *command, wchar_t *params)
{
    return ShExecProcess(command, params, true) != NULL;
}


bool ShInvalidCommand(const wchar_t *command, wchar_t *params)
{
    wprintf(L"%s: invalid command\n", command);
    return true;
}


wchar_t *sh_startup[] =
{
    //L"echo \x1b[37;40m",
    //L"cls",
    NULL,
};


bool (*sh_filters[])(const wchar_t *, wchar_t *) =
{
    ShInternalCommand,
    ShAction,
    ShExternalCommand,
    ShInvalidCommand,
};


int main(void)
{
    wchar_t str[256], *buf, *space, **startup_ptr;
    process_info_t *proc;
    shell_line_t *line;
    unsigned i;

    proc = ProcGetProcessInfo();
    if (ResLoadString(proc->base, 4096, str, _countof(str)))
        _cputws(str, wcslen(str));

	ShGetTerminalSize(&sh_width, &sh_height);
	printf("Shell: terminal is %dx%d\n", sh_width, sh_height);

    startup_ptr = sh_startup;
    while (!sh_exit)
    {
        if (*startup_ptr == NULL)
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
        }
        else
            buf = *startup_ptr++;

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

        for (i = 0; i < _countof(sh_filters); i++)
            if (sh_filters[i](buf, space))
                break;    
    }

    return EXIT_SUCCESS;
}
