/* $Id$ */
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

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
        HndClose(spawned);
        proc = *ProcGetProcessInfo();
        if (in)
            proc.std_in = FsOpen(in, FILE_READ);
        if (out)
            proc.std_out = FsOpen(out, FILE_WRITE);
        memset(proc.cmdline, 0, sizeof(proc.cmdline));
        wcsncpy(proc.cmdline, params, _countof(proc.cmdline) - 1);
        spawned = ProcSpawnProcess(buf, &proc);
        if (in)
            HndClose(proc.std_in);
        if (out)
            HndClose(proc.std_out);
        if (doWait)
            ThrWaitHandle(spawned);
        HndClose(spawned);
    }
    else
        _pwerror(buf);
}
