/*
 * $History: cd.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:23
 * Updated in $/coreos/apps/shell
 * Removed call to ShNormalizePath
 * Added history block
 */
#include <wchar.h>
#include <errno.h>

#include "common.h"

static bool ShNormalizePath(wchar_t *path)
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
}


void ShCmdCd(const wchar_t *command, wchar_t *params)
{
    wchar_t new_path[MAX_PATH];

    params = ShPrompt(L" Directory? ", params);
    if (*params == '\0')
        return;

    if (FsFullPath(params, new_path))
    {
        if (FsChangeDir(new_path))
            wcscpy(sh_path, new_path);
        else
            _pwerror(new_path);
    }
    else
        _pwerror(params);
}


void ShCmdCdDotDot(const wchar_t *command, wchar_t *params)
{
    wchar_t dotdot[] = L"..";
    ShCmdCd(L"cd", dotdot);
}
