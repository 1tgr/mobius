/* $Id$ */
#include <stdio.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>

#include "common.h"

void ShCmdPipe(const wchar_t *cmd, wchar_t *params)
{
    handle_t server, pipe[2];
    char buf[1];
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
    HndClose(pipe[1]);

    while (FsRead(pipe[0], buf, 0, _countof(buf), &bytes))
    {
        //for (i = 0; i < bytes; i++)
            //printf("%02X ", buf[i]);
        //printf("\n");

        fwrite(buf, 1, bytes, stdout);
        if (memchr(buf, '\n', bytes) != NULL)
            fflush(stdout);
    }

    fflush(stdout);
    _pwerror(L"read");
    ThrWaitHandle(server);
    wprintf(L"ShCmdPipe: server exited\n");
    HndClose(server);
    HndClose(pipe[0]);
}
