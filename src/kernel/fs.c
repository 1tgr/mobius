/* $Id: fs.c,v 1.8 2002/01/06 22:46:08 pavlovskii Exp $ */

#include <kernel/fs.h>
#include <kernel/driver.h>

#include <os/rtl.h>
#include <os/defs.h>

#include <errno.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

device_t *root;

typedef struct vfs_mount_t vfs_mount_t;
struct vfs_mount_t
{
	vfs_mount_t *prev, *next;
	wchar_t *name;
	device_t *fsd;
};

typedef struct vfs_dir_t vfs_dir_t;
struct vfs_dir_t
{
	device_t dev;
	vfs_mount_t *vfs_mount_first, *vfs_mount_last;
};

bool VfsRequest(device_t *dev, request_t *req)
{
	vfs_dir_t *dir = (vfs_dir_t*) dev;
	request_fs_t *req_fs = (request_fs_t*) req;
	const wchar_t *ch;
	size_t len;
	vfs_mount_t *mount;

	switch (req->code)
	{
	/* Assumes params_fs_t.fs_create <=> params_fs_t.fs_open */
	case FS_CREATE:
	case FS_OPEN:
		if (req_fs->params.fs_open.name[0] == '/')
			req_fs->params.fs_open.name++;

		ch = wcschr(req_fs->params.fs_open.name, '/');
		if (ch == NULL)
			len = wcslen(req_fs->params.fs_open.name);
		else
			len = ch - req_fs->params.fs_open.name;

		/*wprintf(L"Path: %s Name: %s Len: %d\n", 
			req_fs->params.fs_open.name, ch, len);*/

		FOREACH (mount, dir->vfs_mount)
			if (_wcsnicmp(mount->name, req_fs->params.fs_open.name, len) == 0)
			{
				/*wprintf(L"=> %p\n", mount->fsd);*/
				req_fs->params.fs_open.name += len;
				return mount->fsd->vtbl->request(mount->fsd, req);
			}
		
		wprintf(L"%s: not found in root\n", req_fs->params.fs_open.name);
		req->result = ENOTFOUND;
		return false;
		
	case FS_MOUNT:
		if (req_fs->params.fs_mount.name[0] == '/')
			req_fs->params.fs_mount.name++;

		ch = wcschr(req_fs->params.fs_mount.name, '/');
		if (ch == NULL)
		{
			len = wcslen(req_fs->params.fs_mount.name);
			ch = req_fs->params.fs_mount.name + len;
		}
		else
			len = ch - req_fs->params.fs_mount.name;
		
		if (*ch == '\0')
		{
			/* Mounting on this directory */
			/*wprintf(L"VfsRequest(FS_MOUNT): mounting %p as %*s\n",
				req_fs->params.fs_mount.fsd, 
				len,
				req_fs->params.fs_mount.name);*/

			mount = malloc(sizeof(vfs_mount_t));
			mount->name = malloc(sizeof(wchar_t) * (len + 1));
			mount->name[len] = '\0';
			memcpy(mount->name, req_fs->params.fs_mount.name, 
				sizeof(wchar_t) * len);
			mount->fsd = req_fs->params.fs_mount.fsd;
			LIST_ADD(dir->vfs_mount, mount);

			return true;
		}
		else
		{
			/* Mounting on a subdirectory */
			/*wprintf(L"VfsRequest(FS_MOUNT): mounting on subdirectory %s\n", ch);*/

			FOREACH (mount, dir->vfs_mount)
				if (_wcsnicmp(mount->name, req_fs->params.fs_open.name, len) == 0)
				{
					/*wprintf(L"=> %p\n", mount->fsd);*/
					req_fs->params.fs_mount.name += len;
					return mount->fsd->vtbl->request(mount->fsd, req);
				}

			req->result = ENOTFOUND;
			return false;
		}
	}

	req->result = ENOTIMPL;
	return false;
}

static bool FsCheckAccess(file_t *file, uint32_t mask)
{
	return (file->flags & mask) == mask;
}

handle_t FsCreate(const wchar_t *path, uint32_t flags)
{
	request_fs_t req;
	wchar_t fullname[256];

	if (!FsFullPath(path, fullname))
		return NULL;

	req.header.code = FS_CREATE;
	req.params.fs_create.name = fullname;
	req.params.fs_create.name_size = (wcslen(fullname) * sizeof(wchar_t)) + 1;
	req.params.fs_create.file = NULL;
	req.params.fs_create.flags = flags;
	if (DevRequestSync(root, (request_t*) &req))
		return req.params.fs_create.file;
	else
	{
		errno = req.header.result;
		return NULL;
	}
}

handle_t FsOpen(const wchar_t *path, uint32_t flags)
{
	request_fs_t req;
	wchar_t fullname[256];

	if (!FsFullPath(path, fullname))
		return NULL;

	req.header.code = FS_OPEN;
	req.params.fs_open.name = fullname;
	req.params.fs_open.name_size = (wcslen(fullname) * sizeof(wchar_t)) + 1;
	req.params.fs_open.file = NULL;
	req.params.fs_open.flags = flags;
	if (DevRequestSync(root, (request_t*) &req))
		return req.params.fs_open.file;
	else
	{
		errno = req.header.result;
		return NULL;
	}
}

bool FsClose(handle_t file)
{
	request_fs_t req;
	file_t *fd;
	device_t *fsd;

	fd = HndLock(NULL, file, 'file');
	if (fd == NULL)
		return false;

	fsd = fd->fsd;
	HndUnlock(NULL, file, 'file');

	req.header.code = FS_CLOSE;
	req.params.fs_close.file = file;
	if (DevRequestSync(fsd, (request_t*) &req))
		return true;
	else
	{
		errno = req.header.result;
		return false;
	}
}

size_t FsRead(handle_t file, void *buf, size_t bytes)
{
	request_fs_t req;
	file_t *fd;
	device_t *fsd;

	fd = HndLock(NULL, file, 'file');
	if (fd == NULL)
		return 0;

	if (!FsCheckAccess(fd, FILE_READ))
	{
		HndUnlock(NULL, file, 'file');
		return 0;
	}

	fsd = fd->fsd;
	HndUnlock(NULL, file, 'file');

	/*wprintf(L"FsRead: file = %lx buf = %p bytes = %lu\n",
		file, buf, bytes);*/
	req.header.code = FS_READ;
	req.params.fs_read.length = bytes;
	req.params.fs_read.buffer = buf;
	req.params.fs_read.file = file;
	if (DevRequestSync(fsd, (request_t*) &req))
		return req.params.fs_read.length;
	else
	{
		errno = req.header.result;
		return 0;
	}
}

size_t FsWrite(handle_t file, const void *buf, size_t bytes)
{
	request_fs_t req;
	file_t *fd;
	device_t *fsd;

	fd = HndLock(NULL, file, 'file');
	if (fd == NULL)
		return 0;

	if (!FsCheckAccess(fd, FILE_WRITE))
	{
		HndUnlock(NULL, file, 'file');
		return 0;
	}

	fsd = fd->fsd;
	HndUnlock(NULL, file, 'file');

	req.header.code = FS_WRITE;
	req.params.fs_write.length = bytes;
	req.params.fs_write.buffer = buf;
	req.params.fs_write.file = file;
	if (DevRequestSync(fsd, (request_t*) &req))
		return req.params.fs_write.length;
	else
	{
		errno = req.header.result;
		return 0;
	}
}

addr_t FsSeek(handle_t file, addr_t ofs)
{
	file_t *fd;
	
	fd = HndLock(NULL, file, 'file');
	if (fd == NULL)
		return 0;

	fd->pos = ofs;
	HndUnlock(NULL, file, 'file');

	return ofs;
}

bool FsRequestSync(handle_t file, request_t *req)
{
	file_t *fd;
	device_t *fsd;
	/*handle_hdr_t *ptr;*/

	fd = HndLock(NULL, file, 'file');
	if (fd == NULL)
		return false;

	fsd = fd->fsd;
	HndUnlock(NULL, file, 'file');

	/*ptr = HndGetPtr(NULL, file, 'file');
	wprintf(L"FsRequestSync(%u): %p:%p at %S(%d)\n",
		file, fd, fd->fsd, ptr->file, ptr->line);*/
	return DevRequestSync(fsd, req);
}

static bool FsMountDevice(const wchar_t *path, device_t *dev)
{
	request_fs_t req;
	
	if (wcscmp(path, L"/") == 0)
	{
		DevClose(root);
		root = dev;
		return true;
	}
	else
	{
		req.header.code = FS_MOUNT;
		req.params.fs_mount.name = path;
		req.params.fs_mount.name_size = (wcslen(path) * sizeof(wchar_t)) + 1;
		req.params.fs_mount.fsd = dev;
		if (DevRequestSync(root, (request_t*) &req))
			return true;
		else
		{
			errno = req.header.result;
			return false;
		}
	}
}

bool FsMount(const wchar_t *path, const wchar_t *filesys, device_t *dev)
{
	driver_t *driver;
	device_t *fsd;
	
	driver = DevInstallNewDriver(filesys);
	if (driver == NULL)
	{
		wprintf(L"%s: unknown driver\n", filesys);
		return false;
	}

	if (driver->mount_fs == NULL)
	{
		DevUnloadDriver(driver);
		return false;
	}

	fsd = driver->mount_fs(driver, path, dev);
	if (fsd == NULL)
	{
		DevUnloadDriver(driver);
		return false;
	}

	assert(fsd->vtbl != NULL);
	assert(fsd->vtbl->request != NULL);
	if (!FsMountDevice(path, fsd))
	{
		DevClose(fsd);
		return false;
	}
	else
		return true;
}

static const IDeviceVtbl vfs_vtbl =
{
	VfsRequest,
	NULL
};

bool FsCreateVirtualDir(const wchar_t *path)
{
	vfs_dir_t *dir;

	dir = malloc(sizeof(vfs_dir_t));
	if (dir == NULL)
		return NULL;

	memset(dir, 0, sizeof(vfs_dir_t));
	dir->dev.vtbl = &vfs_vtbl;

	if (!FsMountDevice(path, &dir->dev))
	{
		free(dir);
		return false;
	}

	return true;
}

bool FsInit(void)
{
	bool b;

	FsCreateVirtualDir(L"/");
	FsCreateVirtualDir(L"/system");
	b = FsMount(SYS_BOOT, L"ram", NULL);
	assert(b || "Failed to mount ramdisk");
	b = FsMount(SYS_PORTS, L"portfs", NULL);
	assert(b || "Failed to mount ports");
	FsMount(SYS_DEVICES, L"devfs", NULL);
	assert(b || "Failed to mount devices");

	DevInstallDevice(L"ata", NULL, NULL);
	DevInstallDevice(L"fdc", L"fdc0", NULL);
	
	/*dev = DevOpen(L"ide0a");
	wprintf(L"FsInit: Mounting ide0a(%p) on /hd using fat\n", dev);
	FsMount(L"/hd", L"fat", dev);*/
	return true;
}
