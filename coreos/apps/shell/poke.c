/*
 * $History: poke.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:28
 * Updated in $/coreos/apps/shell
 * Stopped using a file handle for async I/O
 * Added history block
 */

#include <wchar.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"

void ShCmdPoke(const wchar_t *command, wchar_t *params)
{
    handle_t file;
    char str[] = "Hello, world!\n";
    fileop_t op;
    status_t ret;

    params = ShPrompt(L" Device? ", params);
    if (*params == '\0')
        return;

    file = FsOpen(params, FILE_WRITE);
    if (file == NULL)
    {
        _pwerror(params);
        return;
    }

    op.event = EvtCreate(false);
    ret = FsWriteAsync(file, str, 0, sizeof(str), &op);
    if (ret == SIOPENDING)
    {
        printf("poke: pending\n");
        ThrWaitHandle(op.event);
        ret = op.result;
    }

    if (ret > 0)
    {
        errno = ret;
        _pwerror(params);
    }

    printf("poke: finished\n");
    HndClose(op.event);
    HndClose(file);
}
