#include <stdlib.h>
#include <os/os.h>
#include <os/fs.h>
#include <string.h>
#include <os/device.h>

#if 0
//! Returns the root folder for the current process.
/*!
 * By default this is the root folder of the system file system; however, the 
 *	root may have been re-assigned.
 * \return An IFolder* pointer to the root of the current process's file 
 *	system. This interface must be Release()'d once it is finished with.
 */
IFolder* fsRoot()
{
	//IFolder* folder = (IFolder*) thrGetInfo()->process->root;
	//IUnknown_AddRef(folder);
	return thrGetInfo()->process->root;
}

//! Opens an object in the file system
/*!
 * \param folder	root folder in which to search for the file. If this is 
 *	NULL then the current process's root folder is used.
 * \param path		full path to the file to open.
 * \param iid		interface ID to be returned. Use IID_IStream for files and
 *	IID_IStorage for directories; any other interface ID may be available. Use
 *	IID_IUnknown to return a generic pointer to the object; otherwise, the 
 *	return value can be type-cast to the appropriate interface type.
 * \return A pointer to the specified interface of the object requested, or 
 *	NULL if unsucessful.
 */
IUnknown* fsOpenHelper(IFolder* folder, const wchar_t* path, REFIID iid)
{
	wchar_t* ch = wcschr(path, '/');
	folderitem_t item;
	wchar_t temp[MAX_PATH];
	IStream* ret;

	if (folder == NULL)
		folder = fsRoot();

	item.size = sizeof(item);
	if (ch)
	{
		wcscpy(temp, path);
		ch = wcschr(temp, '/');
		*ch = 0;
		item.name = temp;

		//wprintf(L"Opening folder %s\n", temp);
		if (temp[0] == 0)
			return fsOpenHelper(folder, ch + 1, iid);
		else if (SUCCEEDED(IFolder_Open(folder, &item, NULL)))
		{
			if (SUCCEEDED(IUnknown_QueryInterface(item.u.item_handle, &IID_IFolder, &folder)))
			{
				IUnknown_Release(item.u.item_handle);
				return fsOpenHelper(folder, ch + 1, iid);
			}
			else
			{
				IUnknown_Release(item.u.item_handle);
				return NULL;
			}
		}
		else
			return NULL;
	}
	else
	{
		ch = wcschr(path, '?');
		if (ch)
		{
			wcsncpy(temp, path, ch - path);
			ch++;
		}
		else
		{
			wcscpy(temp, path);
			ch = L"";
		}

		item.name = (wchar_t*) temp;
		//wprintf(L"Opening item \"%s\" params = \"%s\"\n", temp, ch);
		if (SUCCEEDED(IFolder_Open(folder, &item, ch)))
		{
			if (SUCCEEDED(IUnknown_QueryInterface(item.u.item_handle, iid, &ret)))
			{
				//wprintf(L"Succeeded\n");
				IUnknown_Release(item.u.item_handle);
				return (IUnknown*) ret;
			}
			else
			{
				//wprintf(L"Failed: no IStream interface");
				IUnknown_Release(item.u.item_handle);
				return NULL;
			}
		}
		else
		{
			//wprintf(L"Failed: no item found\n");
			return NULL;
		}
	}
}

IUnknown* fsOpen(const wchar_t* path, REFIID iid)
{
	static wchar_t fullpath[MAX_PATH];
	fsFullPath(path, fullpath);
	return fsOpenHelper(NULL, fullpath, iid);
}
#endif

addr_t fsOpen(const wchar_t* path)
{
	static wchar_t fullpath[MAX_PATH];
	addr_t ret;
	
	if (!fsFullPath(path, fullpath))
		return NULL;

	asm("int $0x30" : "=a" (ret) : "a" (0x700), "b" (fullpath));
	return ret;
}

bool fsClose(addr_t fd)
{
	bool ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x701), "b" (fd));
	return ret;
}

bool fsRequest(addr_t fd, request_t* req, size_t size)
{
	bool ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x702), "b" (fd), "c" (req), "d" (size));
	return ret;
}

bool fsRead(addr_t fd, void* buffer, size_t* length)
{
	request_t req;
	
	req.code = FS_READ;
	req.params.fs_read.buffer = (addr_t) buffer;
	req.params.fs_read.length = *length;
	req.params.fs_read.fd = fd;

	if (!fsRequest(fd, &req, sizeof(req)))
	{
		devUserFinishRequest(&req, true);
		*length = req.params.fs_read.length;
		return req.result;
	}

	thrWaitHandle(&req.event, 1, true);
	devUserFinishRequest(&req, true);
	*length = req.params.fs_read.length;

	if (req.result)
		sysSetErrno(req.result);

	return req.result == 0;
}

bool fsWrite(addr_t fd, const void* buffer, size_t* length)
{
	request_t req;
	
	req.code = FS_WRITE;
	req.params.fs_write.buffer = (addr_t) buffer;
	req.params.fs_write.length = *length;
	req.params.fs_write.fd = fd;

	if (!fsRequest(fd, &req, sizeof(req)))
	{
		devUserFinishRequest(&req, true);
		*length = req.params.fs_read.length;
		return req.result;
	}

	thrWaitHandle(&req.event, 1, true);
	devUserFinishRequest(&req, true);

	if (req.result)
		sysSetErrno(req.result);

	*length = req.params.fs_read.length;
	return req.result == 0;
}