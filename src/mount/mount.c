#define INITGUID
#include <os/filesys.h>
#include <os/os.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	wchar_t *dev, *dir, *name;
	IFolder* folder;
	IUnknown* obj;
	
	_cputws(L"mount: starting...\n");
	dev = procCmdLine();
	wprintf(L"cmdline = %s\n", dev);
	dir = wcschr(dev, ' ');
	if (dir == NULL)
	{
		_cputws(L"usage: mount <object> <folder>/<name>\n");
		return 1;
	}

	*dir = '\0';
	dir++;

	name = wcschr(dir, '/');
	if (name)
	{
		*name = '\0';
		name++;
	}
	else
	{
		name = dir;
		dir = NULL;
	}

	wprintf(L"mounting %s on %s as %s...", dev, dir, name);

	if (dir)
		folder = (IFolder*) fsOpen(dir, &IID_IFolder);
	else
	{
		folder = fsRoot();
		IUnknown_AddRef(folder);
	}

	if (!folder)
	{
		wprintf(L"%s: unable to open directory\n", dir);
		return 1;
	}

	obj = fsOpen(dev, &IID_IUnknown);
	if (!obj)
	{
		wprintf(L"%s: unable to open object\n", dev);
		IUnknown_Release(folder);
		return 1;
	}

	if (SUCCEEDED(IFolder_Mount(folder, name, obj)))
		_cputws(L"done\n");
	else
		_cputws(L"failed\n");

	IUnknown_Release(obj);
	IUnknown_Release(folder);
	return 0;
}