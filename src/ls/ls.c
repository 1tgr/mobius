#define INITGUID
#include <stdlib.h>
#include <os/os.h>
#include <os/filesys.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	IFolder* folder;
	folderitem_t item;
	wchar_t buf[MAX_PATH], attribs[9], spec[MAX_PATH], *wc, num[10];
	int i;

	wcscpy(spec, procCmdLine());
	if (*spec == '\0')
	{
		wcscpy(spec, thrGetInfo()->process->cwd);
		if (spec[0] != '/' || spec[1] != 0)
			wcscat(spec, L"/*");
	}
	
	wc = wcsrchr(spec, '/');
	if (wc)
	{
		*wc = '\0';
		wc++;
	}
	
	if (wc == NULL || *wc == '\0')
		wc = L"*";

	if (spec[0])
	{
		folder = (IFolder*) fsOpen(spec, &IID_IFolder);
		if (!folder)
		{
			wprintf(L"%s: Failed to open folder\n", spec);
			return 1;
		}
	}
	else
	{
		folder = fsRoot();
		IUnknown_AddRef(folder);
	}

	attribs[8] = '\0';

	item.size = sizeof(item);
	item.name = buf;
	item.name_max = countof(buf);
	if (SUCCEEDED(IFolder_FindFirst(folder, wc, &item)))
	{
		while (SUCCEEDED(IFolder_FindNext(folder, &item)))
		{
			wprintf(L"%s\t", item.name);

			for (i = (wcslen(item.name) + 4) & -4; i < 32; i += 4)
				putwchar('\t');

			if (item.attributes & ATTR_DIRECTORY)
				wcscpy(num, L"<DIR>\t");
			else if (item.attributes & ATTR_DEVICE)
				wcscpy(num, L"<DEV>\t");
			else
				swprintf(num, L"%u\t", item.length);

			_cputws(num);
			for (i = (wcslen(num) + 3) & -4; i < 12; i += 4)
				putwchar('\t');

			if (item.attributes & ATTR_READ_ONLY)
				attribs[0] = 'r';
			else
				attribs[0] = '-';

			if (item.attributes & ATTR_HIDDEN)
				attribs[1] = 'h';
			else
				attribs[1] = '-';

			if (item.attributes & ATTR_SYSTEM)
				attribs[2] = 's';
			else
				attribs[2] = '-';

			if (item.attributes & ATTR_VOLUME_ID)
				attribs[3] = 'v';
			else
				attribs[3] = '-';

			if (item.attributes & ATTR_DIRECTORY)
				attribs[4] = 'd';
			else
				attribs[4] = '-';

			if (item.attributes & ATTR_ARCHIVE)
				attribs[5] = 'a';
			else
				attribs[5] = '-';

			if (item.attributes & ATTR_DEVICE)
				attribs[6] = 'D';
			else
				attribs[6] = '-';

			if (item.attributes & ATTR_LINK)
				attribs[7] = 'L';
			else
				attribs[7] = '-';

			wprintf(L"%s\n", attribs);
		}
	}

	IUnknown_Release(folder);
	return 0;
}