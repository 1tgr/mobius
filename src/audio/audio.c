/* $Id: audio.c,v 1.1 2002/08/17 22:52:06 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

#include <os/rtl.h>
#include <os/syscall.h>
#include <os/defs.h>

#include "resource.h"

wchar_t au_msg[256], au_device_name[] = SYS_DEVICES L"/Classes/audio0";
handle_t au_device;
uint8_t au_buf[8192];

void AuShowHelp(void)
{
    wchar_t *str;
    size_t len;

    len = ResGetStringLength(NULL, 1);
    str = malloc(sizeof(wchar_t) * (len + 1));
    if (str == NULL)
        return;

    ResLoadString(NULL, 1, str, len);
    wprintf(L"%s", str);
}

int AuCmdPlay(int argc, wchar_t **argv)
{
    handle_t file;
    size_t bytes, total;

    if (argc == 0)
    {
        ResLoadString(NULL, 3, au_msg, _countof(au_msg));
        wprintf(L"%s", au_msg);
        return EXIT_FAILURE;
    }

    file = FsOpen(argv[0], FILE_READ);
    if (file == NULL)
    {
        _pwerror(argv[0]);
        return errno;
    }

    au_device = FsOpen(au_device_name, FILE_WRITE);
    if (au_device == NULL)
    {
        _pwerror(au_device_name);
        return errno;
    }

    total = 0;
    while (FsReadSync(file, au_buf, _countof(au_buf), &bytes))
    {
        wprintf(L"AuCmdPlay: %u/%u\n", bytes, total);
        fflush(stdout);
        FsWriteSync(au_device, au_buf, bytes, NULL);
        total += bytes;
    }

    FsClose(file);
    FsClose(au_device);
    return EXIT_SUCCESS;
}

int AuCmdVolume(int argc, wchar_t **argv)
{
    return EXIT_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc <= 1 ||
        _wcsicmp(argv[1], L"\\help") == 0)
    {
        AuShowHelp();
        return EXIT_SUCCESS;
    }

    if (_wcsicmp(argv[1], L"\\play") == 0)
        return AuCmdPlay(argc - 2, argv + 2);
    else if (_wcsicmp(argv[1], L"\\volume") == 0)
        return AuCmdVolume(argc - 2, argv + 2);
    else
    {
        ResLoadString(NULL, 2, au_msg, _countof(au_msg));
        wprintf(au_msg, argv[1], argv[0]);
        return EXIT_FAILURE;
    }
}
