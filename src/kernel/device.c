/* $Id: device.c,v 1.2 2001/11/05 22:41:06 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/debug.h>
#include <kernel/fs.h>

#include <stdio.h>
#include <wchar.h>
#include <errno.h>

#include <os/defs.h>

extern module_t mod_kernel;
extern thread_queue_t thr_finished;

typedef struct device_info_t device_info_t;
struct device_info_t
{
	device_info_t *next, *prev;
	const wchar_t *name;
	device_t *dev;
	device_config_t *cfg;
};

irq_t *irq_first[16], *irq_last[16];
device_info_t *info_first, *info_last;
driver_t *drv_first, *drv_last;

device_t *DevMountFs(driver_t *drv, const wchar_t *name, device_t *dev);

typedef struct device_file_t device_file_t;
struct device_file_t
{
	file_t file;
	device_t *dev;
};

bool DevFsRequest(device_t *dev, request_t *req)
{
	request_fs_t *req_fs = (request_fs_t*) req;
	device_file_t *file;
	device_info_t *info;
	request_dev_t req_dev;
	bool ret;

	switch (req->code)
	{
	case FS_OPEN:
		if (*req_fs->params.fs_open.name == '/')
			req_fs->params.fs_open.name++;

		FOREACH(info, info)
			if (_wcsicmp(req_fs->params.fs_open.name, info->name) == 0)
				break;

		if (info == NULL)
		{
			req->result = ENOTFOUND;
			return false;
		}

		wprintf(L"DevFsRequest: open %s => %p", 
			req_fs->params.fs_open.name,
			info->dev);

		req_fs->params.fs_open.file = HndAlloc(NULL, sizeof(device_file_t), 'file');
		file = HndLock(NULL, req_fs->params.fs_open.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		wprintf(L"=> %lu\n", req_fs->params.fs_open.file);
		file->file.fsd = dev;
		file->file.pos = 0;
		file->file.flags = req_fs->params.fs_open.flags;
		file->dev = info->dev;
		HndUnlock(NULL, req_fs->params.fs_open.file, 'file');
		return true;

	case FS_READ:
	case FS_WRITE:
		file = HndLock(NULL, req_fs->params.fs_read.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		if (req->code == FS_READ)
			wprintf(L"DevFsRequest: write to %lu => %p buf = %p len = %lu\n",
				req_fs->params.fs_read.file,
				file->dev,
				req_fs->params.fs_read.buffer,
				req_fs->params.fs_read.length);

		req_dev.header.code = req->code == FS_WRITE ? DEV_WRITE : DEV_READ;
		req_dev.params.dev_read.buffer = req_fs->params.fs_read.buffer;
		req_dev.params.dev_read.length = req_fs->params.fs_read.length;
		req_dev.params.dev_read.offset = file->file.pos;
		ret = DevRequestSync(file->dev, &req_dev.header);

		HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
		req->result = req_dev.header.result;
		file->file.pos += req_dev.params.dev_read.length;
		return ret;
	}

	req->result = ENOTIMPL;
	return false;
}

driver_t devfs_driver =
{
	&mod_kernel,
	NULL,
	NULL,
	NULL,
	DevMountFs,
};

device_t dev_fsd =
{
	DevFsRequest,	/* request */
	NULL,			/* isr */
	NULL,			/* finishio */
	&devfs_driver,
	NULL,
	NULL,
	NULL,
};

device_t *DevMountFs(driver_t *drv, const wchar_t *name, device_t *dev)
{
	return &dev_fsd;
}

bool DevRequest(device_t *dev, request_t *req)
{
	TRACE1("\t\tDevRequest: dev = %p\n", dev);
	req->event = NULL;
	req->result = 0;

	if (dev == NULL)
	{
		wprintf(L"DevRequest fails on NULL device\n");
		return false;
	}

	if (dev->request == NULL)
	{
		wprintf(L"DevRequest fails on NULL request function\n");
		return false;
	}

	return dev->request(dev, req);
}

bool DevRequestSync(device_t *dev, request_t *req)
{
	TRACE1("\tDevRequestSync: dev = %p\n", dev);
	if (DevRequest(dev, req))
	{
		/*assert(req->event == NULL);*/
		if (req->event)
		{
			if (current == &thr_idle)
			{
				wprintf(L"DevRequestSync: busy-waiting\n");
				while (!EvtIsSignalled(NULL, req->event))
					ArchProcessorIdle();
			}
			else
			{
				wprintf(L"DevRequestSync: doing proper wait\n");
				ThrWaitHandle(current, req->event, 'evnt');
			}

			/*EvtFree(NULL, req->event);*/
		}

		return true;
	}
	else
		return false;
}

device_t *DevOpen(const wchar_t *name)
{
	device_info_t *info;

	FOREACH (info, info)
		if (_wcsicmp(info->name, name) == 0)
			return info->dev;

	return NULL;
}

void DevClose(device_t *dev)
{
}

bool DevRegisterIrq(uint8_t irq, device_t *dev)
{
	irq_t *i;
	
	if (irq > 15)
		return false;

	i = malloc(sizeof(irq_t));
	i->dev = dev;
	i->next = NULL;

	if (irq_last[irq] != NULL)
		irq_last[irq]->next = i;
	if (irq_first[irq] == NULL)
		irq_first[irq] = i;
	irq_last[irq] = i;
	return true;
}

asyncio_t *DevQueueRequest(device_t *dev, request_t *req, size_t size)
{
	asyncio_t *io;
	
	io = malloc(sizeof(asyncio_t));
	if (io == NULL)
		return NULL;

	io->req = malloc(size);
	if (io->req == NULL)
	{
		free(io);
		return NULL;
	}

	LIST_ADD(dev->io, io);
	io->owner = current;
	memcpy(io->req, req, size);
	io->dev = dev;
	io->length = 0;
	io->buffer = NULL;

	req->event = io->req->event = EvtAlloc(NULL);
	return io;
}

void DevFinishIo(device_t *dev, asyncio_t *io)
{
	LIST_REMOVE(dev->io, io);
	LIST_ADD(io->owner->fio, io);
	ThrInsertQueue(io->owner, &thr_finished, NULL);
}

uint8_t DevCfgFindIrq(const device_config_t *cfg, unsigned n, uint8_t dflt)
{
	unsigned i, j;
	device_resource_t *res = DEV_CFG_RESOURCES(cfg);

	for (i = j = 0; i < cfg->resource_count; i++)
		if (res[i].type == resIrq)
		{
			if (j == n)
				return res[i].u.irq;

			j++;
		}

	return dflt;
}

size_t DevRead(device_t *dev, uint64_t ofs, void *buf, size_t size)
{
	request_dev_t req;
	req.header.code = DEV_READ;
	req.params.dev_read.offset = ofs;
	req.params.dev_read.buffer = buf;
	req.params.dev_read.length = size;

	TRACE1("DevRead: dev = %p\n", dev);
	if (DevRequestSync(dev, &req.header))
		return req.params.dev_read.length;
	else
		return (size_t) -1;
}

extern driver_t rd_driver, port_driver;

driver_t *DevInstallNewDriver(const wchar_t *name)
{
	if (_wcsicmp(name, L"ram") == 0)
		return &rd_driver;
	else if (_wcsicmp(name, L"portfs") == 0)
		return &port_driver;
	else if (_wcsicmp(name, L"devfs") == 0)
		return &devfs_driver;
	else
	{
		wchar_t temp[50];
		module_t *mod;
		driver_t *drv;
		bool (*DrvInit)(driver_t *drv);

		swprintf(temp, SYS_BOOT L"/%s.drv", name);
		wprintf(L"DevInstallNewDriver: loading %s\n", temp);
		
		mod = PeLoad(&proc_idle, temp, 0);
		if (mod == NULL)
			return NULL;
		
		FOREACH(drv, drv)
			if (drv->mod == mod)
				return drv;

		wprintf(L"DevInstallNewDriver: performing first-time init\n");
		drv = malloc(sizeof(driver_t));
		if (drv == NULL)
		{
			PeUnload(&proc_idle, mod);
			return NULL;
		}

		drv->mod = mod;
		drv->add_device = NULL;
		drv->mount_fs = NULL;

		DrvInit = (void*) mod->entry;
		if (!DrvInit(drv))
		{
			PeUnload(&proc_idle, mod);
			free(drv);
			return NULL;
		}
		
		LIST_ADD(drv, drv);
		return drv;
	}
}

bool DevAddDevice(device_t *dev, const wchar_t *name, device_config_t *cfg)
{
	device_info_t *info;

	/*wprintf(L"DevAddDevice: added %s\n", name);*/
	
	info = malloc(sizeof(device_info_t));
	if (info == NULL)
		return false;

	info->name = _wcsdup(name);
	info->dev = dev;
	info->cfg = cfg;
	LIST_ADD(info, info);
	return true;
}

void DevUnloadDriver(driver_t *driver)
{
	if (driver->mod != NULL)
		PeUnload(&proc_idle, driver->mod);
	/*free(driver);*/
}

device_t *DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
						   device_config_t *cfg)
{
	device_t *dev;
	driver_t *drv;

	drv = DevInstallNewDriver(driver);
	if (drv != NULL &&
		drv->add_device != NULL)
	{
		dev = drv->add_device(drv, name, cfg);
		if (dev != NULL)
			DevAddDevice(dev, name, cfg);
		return dev;
	}
	else
		return NULL;
}

void DevRunHandlers(void)
{
	asyncio_t *io, *ionext;
	for (io = current->fio_first; io; io = ionext)
	{
		ionext = io->next;
		assert(io->dev);
		assert(io->dev->finishio);

		LIST_REMOVE(io->owner->fio, io);
		io->dev->finishio(io->dev, io);
		free(io->req);
		free(io);
	}
}