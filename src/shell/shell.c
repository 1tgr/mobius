/* $Id: shell.c,v 1.21 2002/08/29 13:59:38 pavlovskii Exp $ */

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

void ShCmdHelp(const wchar_t *command, wchar_t *params)
{
    wchar_t text[1024];
    unsigned i, j;
    bool onlyOnce;

    if (*params == '\0')
    {
        onlyOnce = false;
        printf("Valid commands are:\n");
        for (i = 0; sh_commands[i].name != NULL; i++)
        {
            for (j = wcslen(sh_commands[i].name); j < 10; j++)
                printf(" ");
            _cputws(sh_commands[i].name, wcslen(sh_commands[i].name));
            if (i > 0 && i % 8 == 0)
                printf("\n");
        }

        printf("\n\nValid file actions are:\n");
        for (i = 0; sh_actions[i].name != NULL; i++)
        {
            for (j = wcslen(sh_actions[i].name); j < 10; j++)
                printf(" ");
            _cputws(sh_actions[i].name, wcslen(sh_actions[i].name));
            if (i > 0 && i % 8 == 0)
                printf("\n");
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
                wprintf(L"%s: %s", sh_commands[i].name, text);
                break;
            }
        }

        if (sh_commands[i].name == NULL)
            printf("%S: unrecognized command\n", params);
        
        if (onlyOnce)
        {
            printf("\n");
            break;
        }

        params = L"";
    }
}

void ShCmdExit(const wchar_t *command, wchar_t *params)
{
    sh_exit = true;
}

/*static bool ShNormalizePath(wchar_t *path)
{
    wchar_t *ch, *prev;
    dirent_standard_t standard;
    dirent_t dirent;
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
        if (!FsQueryFile(path, FILE_QUERY_STANDARD, &standard, sizeof(standard)))
            return false;

        if ((standard.attributes & FILE_ATTR_DIRECTORY) == 0)
        {
            errno = ENOTADIR;
            return false;
        }

        if (!FsQueryFile(path, FILE_QUERY_DIRENT, &dirent, sizeof(dirent)))
            return false;
        
        wcscpy(prev, dirent.name);
        prev = ch + 1;
        *ch = '/';
    }

    *ch = '\0';
    return true;
}*/

void ShCmdCd(const wchar_t *command, wchar_t *params)
{
    params = ShPrompt(L" Directory? ", params);
    if (*params == '\0')
        return;

    if (FsFullPath(params, sh_path))
    {
        //if (ShNormalizePath(sh_path))
            //wcscpy(ProcGetProcessInfo()->cwd, sh_path);
            FsChangeDir(sh_path);
        //else
            //_pwerror(sh_path);
    }
    else
        _pwerror(params);
}

typedef struct file_info_t file_info_t;
struct file_info_t
{
    dirent_t dirent;
    dirent_standard_t standard;
};

static int ShCompareDirent(const void *a, const void *b)
{
    const file_info_t *da, *db;

    da = a;
    db = b;
    if((da->standard.attributes & FILE_ATTR_DIRECTORY) &&
       (db->standard.attributes & FILE_ATTR_DIRECTORY) == 0)
        return -1;
    else if ((db->standard.attributes & FILE_ATTR_DIRECTORY) &&
             (da->standard.attributes & FILE_ATTR_DIRECTORY) == 0)
        return 1;
    else
        return _wcsicmp(((const dirent_t*) a)->name, 
            ((const dirent_t*) b)->name);
}

static void ShListDirectory(wchar_t *path, const wchar_t *spec, unsigned indent, 
                            bool do_recurse, bool is_full)
{
    handle_t search;
    file_info_t *entries;
    wchar_t *end;
    unsigned num_entries, alloc_entries, i, n, columns;
    size_t max_len;

    end = path + wcslen(path);
    if (end[-1] != '/')
    {
        wcscpy(end, L"/");
        end++;
    }
    //end++;

    //KeLeakBegin();
    search = FsOpenDir(path);

    if (search != NULL)
    {
        entries = malloc(sizeof(file_info_t));
        num_entries = alloc_entries = 0;
        max_len = 0;
        while (FsReadDir(search, &entries[num_entries].dirent, sizeof(dirent_t)))
        {
            if (wcslen(entries[num_entries].dirent.name) > max_len)
                max_len = wcslen(entries[num_entries].dirent.name);

            wcscpy(end, entries[num_entries].dirent.name);
            if (!FsQueryFile(path, 
                FILE_QUERY_STANDARD, 
                &entries[num_entries].standard, 
                sizeof(dirent_standard_t)))
            {
                _pwerror(path);
                continue;
            }

            num_entries++;
            if (num_entries >= alloc_entries)
            {
                alloc_entries = num_entries + 16;
                //wprintf(L"ShListDirectory: alloc_entries = %u\n", alloc_entries);
                entries = realloc(entries, sizeof(file_info_t) * (alloc_entries + 1));
            }
        }

        FsClose(search);

        qsort(entries, num_entries, sizeof(file_info_t), ShCompareDirent);
        columns = 80 / (max_len + 2) - 1;
        for (i = n = 0; i < num_entries; i++)
        {
            if (do_recurse)
            {
                if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                {
                    printf("%*s+ %S\n", indent * 2, "", entries[i].dirent.name);                
                    ShListDirectory(path, spec, indent + 1, true, is_full);
                }
                else if (_wcsmatch(spec, entries[i].dirent.name) == 0)
                    printf("%*s  %S\n", indent * 2, "", entries[i].dirent.name);
            }
            else if (_wcsmatch(spec, entries[i].dirent.name) == 0)
            {
                if (is_full)
                {
                    if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                        printf("[Directory]\t");
                    else if (entries[i].standard.attributes & FILE_ATTR_DEVICE)
                        printf("   [Device]\t");
                    else
                        printf("%10lu\t", (uint32_t) entries[i].standard.length);

                    printf("\t%S", entries[i].dirent.name);

                    printf("%*s%S\n", 
                        20 - wcslen(entries[i].dirent.name) + 1, "", 
                        entries[i].standard.mimetype);
                }
                else
                {
                    if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                        printf("\x1b[1m");

                    printf("%S  %*s", entries[i].dirent.name, 
                        max_len - wcslen(entries[i].dirent.name), "");

                    if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                        printf("\x1b[m");

                    if (n == columns)
                    {
                        printf("\n");
                        n = 0;
                    }
                    else
                        n++;
                }
            }
        }

        if (!do_recurse && !is_full && (n - 1) != columns)
            printf("\n");
        free(entries);
    }
    else
        _pwerror(path);

    //KeLeakEnd();
}

void ShCmdDir(const wchar_t *command, wchar_t *params)
{
    wchar_t **names, **values, *spec;

    ShParseParams(params, &names, &values, &params);

    spec = wcsrchr(params, '/');
    if (spec == NULL)
    {
        spec = params;
        params = L".";
    }
    else
    {
        *spec = '\0';
        spec++;
    }

    if (*spec == '\0')
        spec = L"*";
    if (*params == '\0')
        params = L".";

    if (!FsFullPath(params, sh_path))
    {
        _pwerror(params);
        return;
    }

    ShListDirectory(sh_path, spec, 0, 
        ShHasParam(names, L"s"), 
        ShHasParam(names, L"full"));

    free(names);
    free(values);
}

static void ShDumpFile(const wchar_t *name, void (*fn)(const void*, addr_t, size_t))
{
    static char buf[8192 + 1];

    handle_t file;
    size_t len;
    fileop_t op;
    addr_t origin;
    dirent_standard_t di;
    char *ptr, *dyn;

    di.length = 0;
    if (!FsQueryFile(name, FILE_QUERY_STANDARD, &di, sizeof(di)) ||
        di.length == 0)
    {
        di.length = sizeof(buf) - 1;
        ptr = buf;
        dyn = NULL;
    }
    else
    {
        dyn = malloc(di.length);
        ptr = dyn;
    }

    printf("ShDumpFile(%S): reading in chunks of %lu\n", name, (uint32_t) di.length);
    file = FsOpen(name, FILE_READ);
    if (file == NULL)
    {
        _pwerror(name);
        return;
    }

    op.event = EvtAlloc();
    origin = 0;
    do
    {
        //KeLeakBegin();

        //printf("[b]");
        if (!FsRead(file, ptr, di.length, &op))
        {
            /* Some catastrophic error */
            errno = op.result;
            _pwerror(name);
            break;
        }

        if (op.result == SIOPENDING)
            ThrWaitHandle(op.event);

        //KeLeakEnd();

        len = op.bytes;
        if (len == 0)
        {
            printf("FSD read zero bytes but didn't report an error\n");
            break;
        }
        
        if (len < di.length)
            ptr[len] = '\0';
        /*printf("%u", len);*/
        fn(ptr, origin, len);

        origin += len;
        if (len < di.length)
        {
            printf("FSD hit the end of the file: successful but only %u bytes read\n", len);
            break;
        }

        //printf("[e]");
        fflush(stdout);
    } while (true);

    free(dyn);
    HndClose(op.event);
    FsClose(file);
}

static void ShTypeOutput(const void *buf, addr_t origin, size_t len)
{
    fwrite(buf, len, 1, stdout);
    fflush(stdout);
}

void ShCmdType(const wchar_t *command, wchar_t *params)
{
    params = ShPrompt(L" File? ", params);
    if (*params == '\0')
        return;

    ShDumpFile(params, ShTypeOutput);
}

static void ShDumpOutput(const void *buf, addr_t origin, size_t size)
{
    const uint8_t *ptr;
    int i, j;

    ptr = (const uint8_t*) buf;
    for (j = 0; j < size; j += i, ptr += i)
    {
        printf("%8lx ", origin + j);
        for (i = 0; i < 16; i++)
        {
            printf("%02x ", ptr[i]);
            if (i + j >= size)
                break;
        }

        printf("\n");
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
    char str[] = "Hello, world!\n";
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
        {
            printf("poke: pending\n");
            ThrWaitHandle(op.event);
        }
    }
    else
    {
        errno = op.result;
        _pwerror(params);
    }

    printf("poke: finished\n");
    FsClose(file);
}

static handle_t ShExecProcess(const wchar_t *command, const wchar_t *params, 
                              bool wait)
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

        FsClose(spawned);
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

void ShCmdDetach(const wchar_t *command, wchar_t *params)
{
    wchar_t buf[MAX_PATH], *out, *in, *space;
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
        proc = *ProcGetProcessInfo();
        if (in)
            proc.std_in = FsOpen(in, FILE_READ);
        if (out)
            proc.std_out = FsOpen(out, FILE_WRITE);
        memset(proc.cmdline, 0, sizeof(proc.cmdline));
        wcsncpy(proc.cmdline, params, _countof(proc.cmdline) - 1);
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
            printf("         System page size: %uKB\n", 
                info.page_size / 1024);
            printf("    Total physical memory: %uMB\n", 
                (info.pages_physical * info.page_size) / 0x100000);
            printf("Available physical memory: %uMB (%u%%)\n", 
                (info.pages_total * info.page_size) / 0x100000,
                (info.pages_total * 100) / info.pages_physical);
            printf(" Reserved physical memory: %uMB (%u%%)\n", 
                (reserved_pages * info.page_size) / 0x100000,
                100 - (info.pages_total * 100) / info.pages_physical);
            printf("     Free physical memory: %uMB (%u%% of total, %u%% of available)\n", 
                (info.pages_free * info.page_size) / 0x100000,
                (info.pages_free * 100) / info.pages_physical,
                (info.pages_free * 100) / info.pages_total);
            printf("   Kernel reserved memory: %uKB (%u%% of total, %u%% of reserved)\n",
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
            printf("   System quantum: %u ms\n",
                times.quantum);
            ShSplitTime(times.uptime, &h, &m, &s, &ms);
            printf("    System uptime: %02u:%02u:%02u.%03u\n",
                h, m, s, ms);
            ShSplitTime(times.current_cputime, &h, &m, &s, &ms);
            printf("CPU time (thread): %02u:%02u:%02u.%03u (average %u%% CPU usage)\n",
                h, m, s, ms, (times.current_cputime * 100) / times.uptime);
        }
        else
            _pwerror(L"SysGetTimes");
    }

    free(names);
    free(values);
}

uint8_t *sh_v86stack;

void *aligned_alloc(size_t bytes)
{
    return VmmAlloc(PAGE_ALIGN_UP(bytes) / PAGE_SIZE, NULL, MEM_READ | MEM_WRITE);
}

void ShCmdV86(const wchar_t *cmd, wchar_t *params)
{
    uint8_t *code;
    psp_t *psp;
    FARPTR fp_code, fp_stackend;
    handle_t thr, file;
    dirent_standard_t di;
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

    code = aligned_alloc(di.length + 0x100);
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
        
    if (sh_v86stack == NULL)
        sh_v86stack = aligned_alloc(65536);

    fp_stackend = i386LinearToFp(sh_v86stack);
    memset(sh_v86stack, 0, 65536);
    
    thr = ThrCreateV86Thread(fp_code, fp_stackend, 15, ShV86Handler);
    if (doWait)
        ThrWaitHandle(thr);

    /* xxx - need to clean up HndClose() implementation before we can use this */
    /*HndClose(thr);*/
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

void ShCmdMount(const wchar_t *cmd, wchar_t *params)
{
    unsigned num_names;
    wchar_t **names, **values;
    const wchar_t *fs, *path, *device;

    num_names = ShParseParams(params, &names, &values, NULL);
    fs = ShFindParam(names, values, L"fs");
    path = ShFindParam(names, values, L"path");
    device = ShFindParam(names, values, L"device");

    if (num_names == 0 ||
        fs == NULL ||
        path == NULL)
    {
        wprintf(L"usage: %s \\fs=<fs> \\path=<path> [\\device=<device>]\n",
            cmd);
        free(names);
        free(values);
        return;
    }

    wprintf(L"mount: mounting %s on %s as %s\n",
        device, path, fs);
    if (!FsMount(path, fs, device))
        _pwerror(L"mount");
}

void ShCmdDismount(const wchar_t *cmd, wchar_t *params)
{
}

void ShCmdPipe(const wchar_t *cmd, wchar_t *params)
{
    handle_t server, pipe[2];
    char buf[2];
    wchar_t handle[11];
    size_t bytes;

    if (!FsCreatePipe(pipe))
    {
        _pwerror(L"pipe");
        return;
    }

    wprintf(L"ShCmdPipe: client = %u, server = %u\n", pipe[0], pipe[1]);
    swprintf(handle, L"0x%x", pipe[1]);
    HndSetInheritable(pipe[1], true);
    server = ShExecProcess(L"/System/Boot/pserver", handle, false);

    do
    {
        if (FsReadSync(pipe[0], buf, _countof(buf) - 1, &bytes))
        {
            buf[bytes] = '\0';
            wprintf(L"%u [%S]\n", bytes, buf);
            //wprintf(L"%u bytes read\n", bytes);
        }
        else
        {
            _pwerror(L"read");
            break;
        }
    } while (buf[0] != '\n');

    ThrWaitHandle(server);
    wprintf(L"ShCmdPipe: server exited\n");
    HndClose(server);
    HndClose(pipe[0]);
    HndClose(pipe[1]);
}

shell_command_t sh_commands[] =
{
    { L"cd",        ShCmdCd,        3 },
    { L"cls",       ShCmdCls,       6 },
    { L"detach",    ShCmdDetach,    10 },
    { L"dir",       ShCmdDir,       4 },
    { L"dismount",  ShCmdDismount,  18 },
    { L"dump",      ShCmdDump,      8 },
    { L"echo",      ShCmdEcho,      16 },
    { L"exit",      ShCmdExit,      2 },
    { L"help",      ShCmdHelp,      1 },
    { L"info",      ShCmdInfo,      12 },
    { L"mount",     ShCmdMount,     17 },
    { L"off",       ShCmdOff,       14 },
    { L"parse",     ShCmdParse,     11 },
    { L"poke",      ShCmdPoke,      7 },
    { L"sleep",     ShCmdSleep,     15 },
    { L"type",      ShCmdType,      5 },
    { L"v86",       ShCmdV86,       13 },
    { L"pipe",      ShCmdPipe,      19 },
    { NULL,         NULL,           0 },
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
    /*L"detach console",
    L"sleep 100",
    L"poke ../ports/console",*/
    L"echo \x1b[37;40m\x1b[2J",
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
