#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>
#include <kernel/hash.h>
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>

bool vfsRequest(device_t* dev, request_t* req);

typedef struct vfs_t vfs_t;
struct vfs_t
{
	device_t dev;
	hashtable_t *mounts;
};

vfs_t vfs =
{
	{
		NULL,
		vfsRequest,
		NULL, NULL,
		NULL
	},
	NULL
};

extern device_t vfs_devices;
device_t *ramMountFs(driver_t *driver, const wchar_t *path, device_t *dev);

bool vfsRequest(device_t* dev, request_t* req)
{
	wchar_t mount[MAX_PATH], *ch;
	const wchar_t *path;
	vfs_t *vfs = (vfs_t*) dev;
	hashelem_t elem, *found;
	request_t openreq;
	status_t hr;

	switch (req->code)
	{
	case DEV_REMOVE:
	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case FS_OPEN:
		path = req->params.fs_open.name;
		if (*path == '/')
			path++;

		ch = wcschr(path, '/');
		if (ch)
			wcsncpy(mount, path, ch - path);
		else
			wcscpy(mount, path);

		path += wcslen(mount);

		req->params.fs_open.fd = NULL;
		found = hashFind(vfs->mounts, mount);
		if (found == NULL)
		{
			wprintf(L"%s: mount point not found\n", mount);
			req->result = ENOTFOUND;
			return false;
		}

		openreq.code = FS_OPEN;
		openreq.params.fs_open.name = path;
		openreq.params.fs_open.name_length = sizeof(wchar_t) * (wcslen(path) + 1);
		hr = devRequestSync((device_t*) found->data, &openreq);
		if (hr == 0)
		{
			req->params.fs_open.fd = openreq.params.fs_open.fd;
			hndSignal(req->event, true);
			return true;
		}
		else
		{
			req->result = openreq.result;
			return false;
		}

	case FS_MOUNT:
		elem.str = wcsdup(req->params.fs_mount.name);
		elem.data = req->params.fs_mount.dev;
		hashInsert(vfs->mounts, &elem);
		hndSignal(req->event, true);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

bool fsMount(const wchar_t* name, const wchar_t* fsd, device_t* device)
{
	request_t req;
	driver_t* drv;
	status_t hr;
	
	drv = devInstallNewDevice(fsd, NULL);
	if (drv == NULL || drv->mount_fs == NULL)
	{
		hndFree(drv);
		return false;
	}

	req.code = FS_MOUNT;
	req.params.fs_mount.name = name;
	req.params.fs_mount.name_length = sizeof(wchar_t) * (wcslen(name) + 1);
	req.params.fs_mount.dev = drv->mount_fs(drv, name, device);
	hndFree(drv);

	hr = devRequestSync(&vfs.dev, &req);
	if (hr)
		errno = hr;

	return hr == 0;
}

bool vfsInit()
{
	device_t *ram;
	wchar_t *name;
	request_t req;

	vfs.mounts = hashCreate(31);

	/*
	 * Mount devices and ramdisk directories manually because they're not real
	 *	drivers.
	 */

	req.code = FS_MOUNT;
	
	name = L"devices";
	req.params.fs_mount.name = name;
	req.params.fs_mount.name_length = sizeof(wchar_t) * (wcslen(name) + 1);
	req.params.fs_mount.dev = &vfs_devices;
	devRequestSync(&vfs.dev, &req);

	name = L"boot";
	ram = ramMountFs(NULL, name, NULL);
	wprintf(L"vfsInit: mounting ramdisk...");
	if (ram)
	{
		req.params.fs_mount.name = name;
		req.params.fs_mount.name_length = sizeof(wchar_t) * (wcslen(name) + 1);
		req.params.fs_mount.dev = ram;
		devRequestSync(&vfs.dev, &req);
		wprintf(L"done (%p)\n", ram);
	}
	else
		wprintf(L"failed\n");

	return true;
}

bool fsFullPath(const wchar_t* src, wchar_t* dst);

const wchar_t *procCwd()
{
	if (current &&
		current->process &&
		current->process->info)
		return current->process->info->cwd;
	else
		return L"/boot";
}

file_t* fsOpen(const wchar_t* path)
{
	wchar_t fullpath[256];
	request_t req;
	status_t hr;

	if (!fsFullPath(path, fullpath))
		return NULL;

	req.code = FS_OPEN;
	req.params.fs_open.name = fullpath;
	req.params.fs_open.name_length = sizeof(wchar_t) * (wcslen(path) + 1);
	hr = devRequestSync(&vfs.dev, &req);

	if (hr != 0)
	{
		errno = hr;
		return NULL;
	}
	else
		return req.params.fs_open.fd;
}

bool fsClose(file_t* fd)
{
	request_t req;
	status_t hr;

	if (fd == NULL)
	{
		errno = EINVALID;
		return false;
	}

	req.code = FS_CLOSE;
	req.params.fs_close.fd = fd;
	hr = devRequestSync(fd->fsd, &req);

	if (hr)
		errno = hr;

	return hr == 0;
}

size_t fsRead(file_t* fd, void* buffer, size_t length)
{
	request_t req;
	status_t hr;

	if (fd == NULL)
	{
		errno = EINVALID;
		return false;
	}

	req.code = FS_READ;
	req.params.fs_read.buffer = (addr_t) buffer;
	req.params.fs_read.length = length;
	req.params.fs_read.fd = fd;
	hr = devRequestSync(fd->fsd, &req);
	
	if (hr)
	{
		errno = hr;
		return (size_t) -1;
	}
	else
		return req.params.fs_read.length;
}

void fsSeek(file_t *fd, qword pos)
{
	/* xxx - need to get the FSD to do this */
	fd->pos = pos;
}

qword fsGetLength(file_t* fd)
{
	request_t req;
	status_t hr;

	if (fd == NULL)
	{
		errno = EINVALID;
		return false;
	}

	req.code = FS_GETLENGTH;
	req.params.fs_getlength.length = 0;
	req.params.fs_getlength.fd = fd;
	hr = devRequestSync(fd->fsd, &req);
	
	if (hr)
	{
		errno = hr;
		return (qword) -1;
	}
	else
		return req.params.fs_getlength.length;	
}