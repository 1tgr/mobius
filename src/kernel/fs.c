#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>
#include <kernel/hash.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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

struct vfs_file_t
{
	file_t file;
	device_t* dev;
};

bool vfsRequest(device_t* dev, request_t* req)
{
	wchar_t mount[MAX_PATH], *ch;
	const wchar_t *path;
	vfs_t *vfs = (vfs_t*) dev;
	hashelem_t elem, *found;
	request_t openreq;
	struct vfs_file_t* fd;
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
		if (hr != 0)
		{
			req->params.fs_open.fd = openreq.params.fs_open.fd;
			hndSignal(req->event, true);
			return true;
		}
		else
		{
			fd = hndAlloc(sizeof(struct vfs_file_t), NULL);
			fd->file.fsd = dev;
			fd->dev = (device_t*) found->data;
			req->params.fs_open.fd = &fd->file;
			//wprintf(L"%s, %s: %x\n", mount, path, openreq.result);
			//req->result = openreq.result;
			hndSignal(req->event, true);
			return true;
		}

	case FS_MOUNT:
		elem.str = wcsdup(req->params.fs_mount.name);
		elem.data = req->params.fs_mount.dev;
		hashInsert(vfs->mounts, &elem);
		hndSignal(req->event, true);
		return true;

	case FS_READ:
		fd = (struct vfs_file_t*) req->params.fs_read.fd;
		hr = devReadSync(fd->dev, 
			fd->file.pos,
			(void*) req->params.fs_read.buffer,
			&req->params.fs_read.length);
		req->result = hr;
		fd->file.pos += req->params.fs_read.length;
		return hr == 0;
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
	device_t *floppy, *ide;
	hashelem_t elem;

	vfs.mounts = hashCreate(31);

	floppy = devOpen(L"floppy0", NULL);
	if (floppy == NULL ||
		!fsMount(L"boot", L"fat", floppy))
		return false;

	ide = devOpen(L"ide0a", NULL);
	if (ide == NULL ||
		!fsMount(L"Mobius", L"fat", ide))
		return false;

	elem.str = L"keyboard";
	elem.data = devOpen(L"keyboard", NULL);
	hashInsert(vfs.mounts, &elem);
	return true;
}

file_t* fsOpen(const wchar_t* path)
{
	request_t req;
	status_t hr;

	req.code = FS_OPEN;
	req.params.fs_open.name = path;
	req.params.fs_open.name_length = sizeof(wchar_t) * (wcslen(path) + 1);
	hr = devRequestSync(&vfs.dev, &req);

	if (hr)
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

	req.code = FS_CLOSE;
	req.params.fs_close.fd = fd;
	hr = devRequestSync(fd->fsd, &req);

	if (hr)
		errno = hr;

	return hr == 0;
}

bool fsRead(file_t* fd, void* buffer, size_t* length)
{
	request_t req;
	status_t hr;

	req.code = FS_READ;
	req.params.fs_read.buffer = (addr_t) buffer;
	req.params.fs_read.length = *length;
	req.params.fs_read.fd = fd;
	hr = devRequestSync(fd->fsd, &req);
	*length = req.params.fs_read.length;

	if (hr)
		errno = hr;

	return hr == 0;
}