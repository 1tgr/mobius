/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "common.h"

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
