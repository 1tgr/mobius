/* $Id$ */
#include <stdio.h>
#include <wchar.h>

#include "common.h"

void ShCmdDevice(const wchar_t *cmd, wchar_t *params)
{
	handle_t dir;
	dirent_t di;
	dirent_device_t di_device;
	wchar_t name[MAX_PATH] = SYS_DEVICES L"/Classes", *ch;

	dir = FsOpenDir(name);
	if (dir == NULL)
	{
		_pwerror(name);
		return;
	}

	ch = name + wcslen(name);
	*ch++ = '/';
	while (FsReadDir(dir, &di, sizeof(di)))
	{
		wprintf(L"%s%*s", di.name, 20 - wcslen(di.name), L"");

		wcscpy(ch, di.name);
		if (FsQueryFile(name, FILE_QUERY_DEVICE, &di_device, sizeof(di_device)))
			wprintf(L"%s\n", di_device.description);
		else
			printf("\n");
	}

	HndClose(dir);
}
