/* $Id: device.c,v 1.15 2002/02/22 15:31:20 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/debug.h>
#include <kernel/fs.h>
#include <kernel/memory.h>
#include <kernel/io.h>

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

static void DevFsFinishIo(device_t *dev, request_t *req)
{
	device_file_t *file;
	request_dev_t *req_dev;
	request_fs_t *req_fs;

	req_dev = (request_dev_t*) req;
	req_fs = req->param;
	assert(req_fs != NULL);
	assert(req_fs->header.code == FS_READ || req_fs->header.code == FS_WRITE);
	file = HndLock(NULL, req_fs->params.buffered.file, 'file');
	assert(file != NULL);

	TRACE3("DevFsFinishIo: req_dev = %p req_fs = %p length = %u\n",
		req_dev, req_fs, req_dev->params.buffered.length);
	req_fs->params.buffered.length = req_dev->params.buffered.length;
	req_fs->header.result = req_dev->header.result;
	file->file.pos += req_dev->params.buffered.length;

	HndUnlock(NULL, req_fs->params.buffered.file, 'file');
	HndUnlock(NULL, req_fs->params.buffered.file, 'file');
	free(req_dev);

	IoNotifyCompletion(&req_fs->header);
}

static bool DevFsRequest(device_t *dev, request_t *req)
{
	request_fs_t *req_fs = (request_fs_t*) req;
	device_file_t *file;
	device_info_t *info;
	request_dev_t *req_dev;
	size_t len;
	dirent_t *buf;
	
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

		/*wprintf(L"DevFsRequest: open %s => %p", 
			req_fs->params.fs_open.name,
			info->dev);*/

		req_fs->params.fs_open.file = HndAlloc(NULL, sizeof(device_file_t), 'file');
		file = HndLock(NULL, req_fs->params.fs_open.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		/*wprintf(L"=> %lu\n", req_fs->params.fs_open.file);*/
		file->file.fsd = dev;
		file->file.pos = 0;
		file->file.flags = req_fs->params.fs_open.flags;
		file->dev = info->dev;
		HndUnlock(NULL, req_fs->params.fs_open.file, 'file');
		return true;

	case FS_OPENSEARCH:
		if (*req_fs->params.fs_opensearch.name == '/')
			req_fs->params.fs_opensearch.name++;

		req_fs->params.fs_opensearch.file = HndAlloc(NULL, sizeof(device_file_t), 'file');
		file = HndLock(NULL, req_fs->params.fs_opensearch.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		file->file.fsd = dev;
		file->file.pos = (uint64_t) (addr_t) info_first;
		file->file.flags = FILE_READ;
		file->dev = NULL;
		HndUnlock(NULL, req_fs->params.fs_opensearch.file, 'file');
		return true;

	case FS_READ:
	case FS_WRITE:
		file = HndLock(NULL, req_fs->params.fs_read.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		/*if (req->code == FS_READ)
			wprintf(L"DevFsRequest: read from %lu => %p buf = %p len = %lu\n",
				req_fs->params.fs_read.file,
				file->dev,
				req_fs->params.fs_read.buffer,
				req_fs->params.fs_read.length);*/
		/*else
			wprintf(L"DevFsRequest: write to %lu => %p buf = %p len = %lu\n",
				req_fs->params.fs_write.file,
				file->dev,
				req_fs->params.fs_write.buffer,
				req_fs->params.fs_write.length);*/

		if (file->dev != NULL)
		{
			/* Normal device file */
			req_dev = malloc(sizeof(request_dev_t));
			if (req_dev == NULL)
			{
				req->result = ENOMEM;
				HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
				return false;
			}

			req_dev->header.code = req->code == FS_WRITE ? DEV_WRITE : DEV_READ;
			req_dev->header.param = req;
			req_dev->params.dev_read.buffer = req_fs->params.buffered.buffer;
			req_dev->params.dev_read.length = req_fs->params.buffered.length;
			req_dev->params.dev_read.offset = file->file.pos;

			/*wprintf(L"DevFsRequest: req_dev = %p req_fs = %p length = %u\n",
				req_dev, req_fs, req_dev->params.buffered.length);*/
			if (!IoRequest(dev, file->dev, &req_dev->header))
			{
				req_fs->params.buffered.length = req_dev->params.buffered.length;
				req->result = req_dev->header.result;
				file->file.pos += req_dev->params.buffered.length;
				HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
				free(req_dev);
				return false;
			}
			else
			{
				req->result = req_dev->header.result;
				return true;
			}
		}
		else
		{
			/* Search */
			info = (device_info_t*) (addr_t) file->file.pos;

			if (info == NULL)
			{
				req->result = EEOF;
				HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
				return false;
			}
			
			len = req_fs->params.fs_read.length;

			req_fs->params.fs_read.length = 0;
			buf = req_fs->params.fs_read.buffer;
			while (req_fs->params.fs_read.length < len)
			{
				wcscpy(buf->name, info->name);
				buf->length = 0;
				buf->standard_attributes = FILE_ATTR_DEVICE;

				buf++;
				req_fs->params.fs_read.length += sizeof(dirent_t);

				info = info->next;
				file->file.pos = (uint64_t) (addr_t) info;
				if (info == NULL)
					break;
			}

			HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
			return true;
		}
	}

	req->result = ENOTIMPL;
	return false;
}

static driver_t devfs_driver =
{
	&mod_kernel,
	NULL,
	NULL,
	NULL,
	DevMountFs,
};

static const device_vtbl_t devfs_vtbl =
{
	DevFsRequest,	/* request */
	NULL,			/* isr */
	DevFsFinishIo,	/* finishio */
};

static device_t dev_fsd =
{
	&devfs_driver,
	NULL,
	NULL,
	NULL,
	&devfs_vtbl,
};

/*
 * IoOpenDevice and IoCloseDevice are in device.c to save io.c knowing about
 *	device_info_t.
 */

device_t *IoOpenDevice(const wchar_t *name)
{
	device_info_t *info;

	FOREACH (info, info)
		if (_wcsicmp(info->name, name) == 0)
			return info->dev;

	return NULL;
}

void IoCloseDevice(device_t *dev)
{
}

device_t *DevMountFs(driver_t *drv, const wchar_t *name, device_t *dev)
{
	return &dev_fsd;
}

/*!
 * \brief	Connects an IRQ line with a device
 *
 *	The device's \p irq function is called when the specified IRQ occurs
 *	\param	irq	The IRQ to connect
 *	\param	dev	The device to associate with the IRQ
 *	\return	\p true if the IRQ could be registered
 */
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

/*!
 *	\brief	Queues an asynchronous request on a device
 *
 *	This function does the following:
 *	- Sets \p req->result to \p SIOPENDING, indicating to the originator of
 *	the request that the request is asynchronous
 *	- Creates a copy of the original request structure in kernel space
 *	- Ensures that each of the buffer pages are mapped
 *	- Locks each of the buffer pages in physical memory
 *	- Adds an \p asyncio_t structure to the end of the device's queue
 *
 *	The physical addresses of each of the pages in the user buffer is stored
 *	as an \p addr_t array immediately after the \p asyncio_t structure.
 *
 *	\param	dev	Device to which the request applies
 *	\param	req	Request to be queued
 *	\param	size	Size of the \p req structure
 *	\param	user_buffer	Buffer in user space to lock
 *	\param	user_buffer_length	Length, in bytes, of \p user_buffer
 *	\return	A pointer to an \p asyncio_t structure
 */
asyncio_t *DevQueueRequest(device_t *dev, request_t *req, size_t size,
						   void *user_buffer,
						   size_t user_buffer_length)
{
	asyncio_t *io;
	unsigned pages;
	addr_t *ptr, user_addr, phys;
	
	req->result = SIOPENDING;

	pages = PAGE_ALIGN_UP(user_buffer_length) / PAGE_SIZE;
	io = malloc(sizeof(asyncio_t) + sizeof(addr_t) * pages);
	if (io == NULL)
		return NULL;

	io->owner = current;

	if (true || (addr_t) req < 0x80000000)
	{
		io->req = malloc(size);
		if (io->req == NULL)
		{
			free(io);
			return NULL;
		}

		memcpy(io->req, req, size);
		io->req->original = req;
	}
	else
		io->req = req;

	io->req_size = size;
	req->original = NULL;
	io->dev = dev;
	io->length = user_buffer_length;
	io->length_pages = pages;
	io->extra = NULL;
	io->mod_buffer_start = (unsigned) user_buffer % PAGE_SIZE;
	ptr = (addr_t*) (io + 1);
	user_addr = PAGE_ALIGN((addr_t) user_buffer);
	for (; pages > 0; pages--, user_addr += PAGE_SIZE)
	{
		phys = MemTranslate((void*) user_addr) & -PAGE_SIZE;
		assert(phys != NULL);
		MemLockPages(phys, 1, true);
		*ptr++ = phys;
	}

	/*req->event = io->req->event = EvtAlloc(NULL);*/
	LIST_ADD(dev->io, io);
	return io;
}

static void DevFinishIoApc(void *ptr)
{
	asyncio_t *io;

	io = ptr;
	TRACE1("DevFinishIoApc(%p): ", ptr);
	TRACE1("io->req = %p ", io->req);
	TRACE1("io->req->original = %p\n", io->req->original);
	if (io->req->original != NULL)
	{
		assert(io->owner == current);
		memcpy(io->req->original, io->req, io->req_size);
	}

	if (io->req->original != NULL)
	{
		request_t *req;
		req = io->req;
		io->req = io->req->original;
		free(req);
	}

	IoNotifyCompletion(io->req);
	/*EvtSignal(io->owner->process, io->req->event);*/

	free(io);
	/* Need to free io->req->event */
	/* No we don't, that's left to the caller */
}

/*!
 *	\brief	Notifies the kernel that the specified asynchronous request has
 *	been completed
 *
 *	This function does the following:
 *	- Unlocks each of the pages in the original user buffer
 *	- Removes the \p asyncio_t structure from the device's queue
 *	- Updates the \p result field of the original request
 *
 *	It then queues an APC which:
 *	- Copies the copy of the request structure (which may have been updated
 *	by the driver between \p DevQueueRequest and \p DevFinishIo) into the
 *	original request structure
 *	- Calls the originator's I/O completion function, if appropriate
 *	- Frees the \p asyncio_t structure
 *
 *	The APC is guaranteed to execute in the context of the request originator.
 *	If \p DevFinishIo is called from the same context as the originator then
 *	the APC routine is called directly.
 *
 *	\param	dev	Device where the asynchronous request is queued
 *	\param	io	Asynchronous request structure
 *	\param	result	Final result of the operation
 *
 */
void DevFinishIo(device_t *dev, asyncio_t *io, status_t result)
{
	addr_t *ptr;
	unsigned i;

	ptr = (addr_t*) (io + 1);
	for (i = 0; i < io->length_pages; i++, ptr++)
		MemLockPages(*ptr, 1, false);
	
	LIST_REMOVE(dev->io, io);
	/*LIST_ADD(io->owner->fio, io);*/
	/*ThrInsertQueue(io->owner, &thr_finished, NULL);*/

	io->req->result = result;

	if (io->owner == current)
	{
		TRACE0("Not queueing APC\n");
		DevFinishIoApc(io);
	}
	else
	{
		TRACE0("Queueing APC\n");
		ThrQueueKernelApc(io->owner, DevFinishIoApc, io);
	}
}

/*!
 *	\brief	Finds the n'th IRQ resource
 *
 *	\param	cfg	Pointer to the device's configuration list
 *	\param	n	Index of the IRQ to find; use 0 for the first IRQ, 1 for
 *		the second, etc.
 *	\param	dflt	Default value to return if the specified IRQ could not
 *		be found
 *	\return	The requested IRQ
 */
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

extern driver_t rd_driver, port_driver;

/*!
 *	\brief	Installs a new device driver
 *
 *	Currently device drivers are found in \p /System/Boot, and are given names
 *	of the form \p name.drv.
 *
 *	This function searches for the driver file, attempts to load it, and
 *	calls the driver's entry point routine.
 *
 *	There are several built-in drivers:
 *	- \p ram, the ramdisk device and file system driver
 *	- \p portfs, the port file system
 *	- \p devfs, the device file system
 *
 *	\param	name	Name of the device driver
 *	\return	A pointer to a device driver
 */
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
		/*wprintf(L"DevInstallNewDriver: loading %s\n", temp);*/

		mod = PeLoad(&proc_idle, temp, 0);
		if (mod == NULL)
			return NULL;
		
		FOREACH(drv, drv)
			if (drv->mod == mod)
				return drv;

		/*wprintf(L"DevInstallNewDriver: performing first-time init\n");*/
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

/*! 
 *	\brief	Adds a new device to the device manager's list
 *
 *	The new device will appear in the \p /System/Device directory, and it
 *	will also be available via \p IoOpenDevice. This function is most often 
 *	used by bus drivers, or other drivers that enumerate their own devices, 
 *	to add devices that they have found.
 *
 *	\param	dev	Pointer to the new device object
 *	\param	name	Name for the device
 *	\param	cfg	Device configuration list
 *	\return	\p true if the device was added
 */
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

/*!
 *	\brief	Unloads a driver loaded by \p DevInstallNewDriver
 *
 *	\param	driver	Driver to unload
 */
void DevUnloadDriver(driver_t *driver)
{
	if (driver->mod != NULL)
		PeUnload(&proc_idle, driver->mod);
	/*free(driver);*/
}

/*!
 *	\brief	Installs a new device
 *
 *	This function will ask the specified driver for the given device;
 *	the driver is loaded if not already present.
 *
 *	\param	driver	Name of the device driver
 *	\param	name	Name of the new device
 *	\param	cfg	Configuration list to assign to the new device
 *	\return	A pointer to the new device object
 */
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
		{
			assert(dev->vtbl != NULL);
			DevAddDevice(dev, name, cfg);
		}
		return dev;
	}
	else
		return NULL;
}

/*!
 *	\brief	Temporarily maps the user-mode buffer of an asynchronous request
 *
 *	Use this function in the interrupt routine of a driver when data need to be
 *	written to the user buffer.
 *
 *	Note that the pointer returned by this function refers to the start of the
 *	lowest page in the buffer; you must add \p io->mod_buffer_start to obtain
 *	a usable buffer address.
 *
 *	Call \p DevUnmapBuffer when you have finished with the buffer returned.
 *
 *	\param	io	Asynchronous request structure
 *	\return	A page-aligned pointer to the start of the buffer
 */
void *DevMapBuffer(asyncio_t *io)
{
	return MemMapTemp((addr_t*) (io + 1), io->length_pages, 
		PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
}
