/* $Id: device.c,v 1.24 2002/05/05 13:42:59 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/fs.h>
#include <kernel/memory.h>
#include <kernel/io.h>
#include <kernel/init.h>

/*#define DEBUG*/
#include <kernel/debug.h>

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

status_t DfsLookupFile(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **cookie)
{
    device_info_t *info;

    assert(path[0] == '/');
    path++;

    for (info = info_first; info != NULL; info = info->next)
    {
        if (_wcsicmp(path, info->name) == 0)
        {
            *cookie = info;
            return 0;
        }
    }

    return ENOTFOUND;
}

status_t DfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    device_info_t *info;
    dirent_standard_t *di;

    info = cookie;
    di = buf;
    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_STANDARD:
        wcsncpy(di->di.name, info->name, _countof(di->di.name) - 1);
	di->length = 0;
	di->attributes = FILE_ATTR_DEVICE;
        return 0;
    }

    return ENOTIMPL;
}

bool DfsReadWrite(fsd_t *fsd, file_t *file, page_array_t *pages, size_t length, 
                  fs_asyncio_t *io, bool is_reading)
{
    device_t *dev;
    device_info_t *info;
    request_dev_t *req_dev;
    io_callback_t cb;
    bool ret;

    req_dev = malloc(sizeof(request_dev_t));
    if (req_dev == NULL)
    {
    	io->op.result = errno;
        return false;
    }

    info = file->fsd_cookie;
    dev = info->dev;
    if (dev->flags & DEVICE_IO_DIRECT)
    {
        req_dev->header.code = is_reading ? DEV_READ_DIRECT : DEV_WRITE_DIRECT;
	req_dev->header.param = io;
	req_dev->params.dev_read_direct.buffer = 
            MemMapPageArray(pages, PRIV_PRES | PRIV_KERN | (is_reading ? PRIV_WR : 0));
	req_dev->params.dev_read_direct.length = length;
	req_dev->params.dev_read_direct.offset = file->pos;
    }
    else
    {
        req_dev->header.code = is_reading ? DEV_READ : DEV_WRITE;
	req_dev->header.param = io;
        req_dev->params.dev_read.pages = pages;
	req_dev->params.dev_read.length = length;
	req_dev->params.dev_read.offset = file->pos;
    }

    cb.type = IO_CALLBACK_FSD;
    cb.u.fsd = fsd;
    ret = IoRequest(&cb, dev, &req_dev->header);

    if (dev->flags & DEVICE_IO_DIRECT)
        MemUnmapTemp();

    /* xxx - bad - req_dev might have been freed here */
    if (req_dev->header.code != 0)
    {
        /*
         * This case only applies if DfsFinishIo hasn't been called yet: 
         *  that is, if either:
         * - ret == false
         * - req_dev->header.result == SIOPENDING
         * Either way, the FS needs to know what the result was
         */

        /* xxx - this is hack: we shouldn't have to modify io->original */
        io->original->result = io->op.result = req_dev->header.result;
    }

    if (!ret)
    {
	io->op.bytes = req_dev->params.buffered.length;
	free(req_dev);
        return false;
    }

    /*
     * DfsFinishIo *will* get called upon every successful request: 
     *  for async io: when the device finishes
     *   for sync io: by IoRequest
     * Therefore, since DfsFinishIo frees req_dev, we can't use it from now on.
     * Also, FsNotifyCompletion will free io, so we can't use that either.
     */

    return true;
}

void DfsFinishIo(fsd_t *fsd, request_t *req)
{
    request_dev_t *req_dev;
    fs_asyncio_t *io;

    req_dev = (request_dev_t*) req;
    io = req_dev->header.param;
    assert(io != NULL);

    FsNotifyCompletion(io, 
        req_dev->params.buffered.length, 
        req_dev->header.result);
    req_dev->header.code = 0;
    free(req_dev);
}

bool DfsRead(fsd_t *fsd, file_t *file, page_array_t *pages, size_t length, 
             fs_asyncio_t *io)
{
    return DfsReadWrite(fsd, file, pages, length, io, true);
}

bool DfsWrite(fsd_t *fsd, file_t *file, page_array_t *pages, size_t length, 
              fs_asyncio_t *io)
{
    return DfsReadWrite(fsd, file, pages, length, io, false);
}

bool DfsIoCtl(fsd_t *fsd, file_t *file, uint32_t code, void *buf, size_t length, 
              fs_asyncio_t *io)
{
    request_dev_t req;
    bool ret;
    device_info_t *info;

    if (!MemVerifyBuffer(buf, length))
    {
        io->op.result = EBUFFER;
        return false;
    }

    req.header.code = DEV_IOCTL;
    req.params.dev_ioctl.code = code;
    req.params.dev_ioctl.params = buf;
    req.params.dev_ioctl.size = length;
    req.params.dev_ioctl.unused = 0;

    info = file->fsd_cookie;
    ret = IoRequestSync(info->dev, &req.header);

    FsNotifyCompletion(io, req.params.dev_ioctl.size, req.header.result);
    return ret;
}

bool DfsPassthrough(fsd_t *fsd, file_t *file, uint32_t code, void *buf, size_t length, 
                    fs_asyncio_t *io)
{
    request_dev_t *req_dev;
    bool ret;
    device_info_t *info;

    if (!MemVerifyBuffer(buf, length))
    {
        io->op.result = EBUFFER;
        return false;
    }

    req_dev = malloc(sizeof(request_t) + length);
    req_dev->header.code = code;

    memcpy(&req_dev->params, buf, length);

    info = file->fsd_cookie;
    ret = IoRequestSync(info->dev, &req_dev->header);
    io->op.result = io->original->result = req_dev->header.result;

    memcpy(buf, &req_dev->params, length);

    FsNotifyCompletion(io, length, req_dev->header.result);
    free(req_dev);
    return ret;
}

status_t DfsOpenDir(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **dir_cookie)
{
    device_info_t **ptr;

    ptr = malloc(sizeof(device_info_t*));
    if (ptr == NULL)
        return errno;

    *ptr = info_first;
    *dir_cookie = ptr;
    return 0;
}

status_t DfsReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    device_info_t **ptr, *info;

    ptr = dir_cookie;
    if (*ptr == NULL)
        return EEOF;
    else
    {
        info = *ptr;
        wcsncpy(buf->name, info->name, _countof(buf->name));
        buf->vnode = 0;
        info = info->next;
        *ptr = info;
        return 0;
    }
}

void DfsFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    free(dir_cookie);
}

static driver_t devfs_driver =
{
    &mod_kernel,
    NULL,
    NULL,
    NULL,
    DevMountFs,
};

static const fsd_vtbl_t devfs_vtbl =
{
    NULL,           /* dismount */
    NULL,           /* get_fs_info */
    NULL,           /* create_file */
    DfsLookupFile,
    DfsGetFileInfo,
    NULL,           /* set_file_info */
    NULL,           /* free_cookie */
    DfsRead,
    DfsWrite,
    DfsIoCtl,
    DfsPassthrough,
    DfsOpenDir,
    DfsReadDir,
    DfsFreeDirCookie,
    NULL,           /* mount */
    DfsFinishIo,
    NULL,           /* flush_cache */
};

static fsd_t dev_fsd =
{
    &devfs_vtbl,
};

/*
 * IoOpenDevice and IoCloseDevice are in device.c to save io.c knowing about
 *    device_info_t.
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

fsd_t *DevMountFs(driver_t *drv, const wchar_t *dest)
{
    return &dev_fsd;
}

/*!
 * \brief    Connects an IRQ line with a device
 *
 *    The device's \p irq function is called when the specified IRQ occurs
 *    \param	irq    The IRQ to connect
 *    \param	dev    The device to associate with the IRQ
 *    \return	 \p true if the IRQ could be registered
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
 *    \brief	Queues an asynchronous request on a device
 *
 *    This function does the following:
 *    - Sets \p req->result to \p SIOPENDING, indicating to the originator of
 *    the request that the request is asynchronous
 *    - Creates a copy of the original request structure in kernel space
 *    - Ensures that each of the buffer pages are mapped
 *    - Locks each of the buffer pages in physical memory
 *    - Adds an \p asyncio_t structure to the end of the device's queue
 *
 *    The physical addresses of each of the pages in the user buffer is stored
 *    as an \p addr_t array immediately after the \p asyncio_t structure.
 *
 *    \param	dev    Device to which the request applies
 *    \param	req    Request to be queued
 *    \param	size	Size of the \p req structure
 *    \param	user_buffer    Buffer in user space to lock
 *    \param	user_buffer_length    Length, in bytes, of \p user_buffer
 *    \return	 A pointer to an \p asyncio_t structure
 */
asyncio_t *DevQueueRequest(device_t *dev, request_t *req, size_t size,
			   page_array_t *pages,
			   size_t user_buffer_length)
{
    asyncio_t *io;

    if (req != NULL)
    {
        /*
         * It should be possible to pass a NULL request here, as long as the caller set 
         *  req->result.
         */
        req->result = SIOPENDING;
        req->original = NULL;
    }

    /*user_addr = PAGE_ALIGN((addr_t) user_buffer);
    pages = PAGE_ALIGN_UP(user_buffer_length) / PAGE_SIZE;
    mod = (addr_t) user_buffer % PAGE_SIZE;
    if (mod > 0 &&
        mod + user_buffer_length >= PAGE_SIZE)
        pages++;

    io = malloc(sizeof(asyncio_t) + sizeof(addr_t) * pages);
    if (io == NULL)
        return NULL;*/
    io = malloc(sizeof(asyncio_t));
    if (io == NULL)
        return NULL;

    /*io->pages = MemCreatePageArray(user_buffer, user_buffer_length);
    if (io->pages == NULL)
    {
        free(io);
        return NULL;
    }*/

    io->pages = MemCopyPageArray(pages->num_pages, pages->mod_first_page, 
        pages->pages);
    if (io->pages == NULL)
    {
        free(io);
        return NULL;
    }

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
    io->dev = dev;
    io->length = user_buffer_length;
    //io->length_pages = pages;
    io->extra = NULL;
    /*io->mod_buffer_start = mod;
    ptr = (addr_t*) (io + 1);
    for (; pages > 0; pages--, user_addr += PAGE_SIZE)
    {
        phys = MemTranslate((void*) user_addr) & -PAGE_SIZE;
        if (phys == NULL)
        {
            wprintf(L"buffer = %p: virt = %x is not mapped\n", 
                user_buffer, user_addr);
            assert(phys != NULL);
        }

        MemLockPages(phys, 1, true);
        *ptr++ = phys;
    }*/
    
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
 *    \brief	Notifies the kernel that the specified asynchronous request has
 *    been completed
 *
 *    This function does the following:
 *    - Unlocks each of the pages in the original user buffer
 *    - Removes the \p asyncio_t structure from the device's queue
 *    - Updates the \p result field of the original request
 *
 *    It then queues an APC which:
 *    - Copies the copy of the request structure (which may have been updated
 *    by the driver between \p DevQueueRequest and \p DevFinishIo) into the
 *    original request structure
 *    - Calls the originator's I/O completion function, if appropriate
 *    - Frees the \p asyncio_t structure
 *
 *    The APC is guaranteed to execute in the context of the request originator.
 *    If \p DevFinishIo is called from the same context as the originator then
 *    the APC routine is called directly.
 *
 *    \param	dev    Device where the asynchronous request is queued
 *    \param	io    Asynchronous request structure
 *    \param	result	  Final result of the operation
 *
 */
void DevFinishIo(device_t *dev, asyncio_t *io, status_t result)
{
    /*ptr = (addr_t*) (io + 1);
    for (i = 0; i < io->length_pages; i++, ptr++)
	MemLockPages(*ptr, 1, false);*/
    MemDeletePageArray(io->pages);
    io->pages = NULL;

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
 *    \brief	Finds the n'th IRQ resource
 *
 *    \param	cfg    Pointer to the device's configuration list
 *    \param	n    Index of the IRQ to find; use 0 for the first IRQ, 1 for
 *	  the second, etc.
 *    \param	dflt	Default value to return if the specified IRQ could not
 *	  be found
 *    \return	 The requested IRQ
 */
uint8_t DevCfgFindIrq(const device_config_t *cfg, unsigned n, uint8_t dflt)
{
    unsigned i, j;
    device_resource_t *res = DEV_CFG_RESOURCES(cfg);

    for (i = j = 0; i < cfg->num_resources; i++)
	if (res[i].cls == resIrq)
	{
	    if (j == n)
		return res[i].u.irq;

	    j++;
	}

    return dflt;
}

/*!
 *    \brief	Finds the n'th memory resource
 *
 *    \param	cfg    Pointer to the device's configuration list
 *    \param	n    Index of the memory range to find; use 0 for the first 
 *          memory range, 1 for the second, etc.
 *    \param	dflt	Default value to return if the specified memory range 
 *          could not be found
 *    \return	 The requested memory base address
 */
device_resource_t *DevCfgFindMemory(const device_config_t *cfg, unsigned n)
{
    unsigned i, j;
    device_resource_t *res = DEV_CFG_RESOURCES(cfg);

    for (i = j = 0; i < cfg->num_resources; i++)
	if (res[i].cls == resMemory)
	{
	    if (j == n)
		return res + i;

	    j++;
	}

    return NULL;
}

extern driver_t rd_driver, port_driver;

/*!
 *    \brief	Installs a new device driver
 *
 *    Currently device drivers are found in \p /System/Boot, and are given names
 *    of the form \p name.drv.
 *
 *    This function searches for the driver file, attempts to load it, and
 *    calls the driver's entry point routine.
 *
 *    There are several built-in drivers:
 *    - \p ram, the ramdisk device and file system driver
 *    - \p portfs, the port file system
 *    - \p devfs, the device file system
 *
 *    \param	name	Name of the device driver
 *    \return	 A pointer to a device driver
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
 *    \brief	Adds a new device to the device manager's list
 *
 *    The new device will appear in the \p /System/Device directory, and it
 *    will also be available via \p IoOpenDevice. This function is most often 
 *    used by bus drivers, or other drivers that enumerate their own devices, 
 *    to add devices that they have found.
 *
 *    \param	dev    Pointer to the new device object
 *    \param	name	Name for the device
 *    \param	cfg    Device configuration list
 *    \return	 \p true if the device was added
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
 *    \brief	Unloads a driver loaded by \p DevInstallNewDriver
 *
 *    \param	driver	  Driver to unload
 */
void DevUnloadDriver(driver_t *driver)
{
    if (driver->mod != NULL)
	PeUnload(&proc_idle, driver->mod);
    /*free(driver);*/
}

/*!
 *    \brief	Installs a new device
 *
 *    This function will ask the specified driver for the given device;
 *    the driver is loaded if not already present.
 *
 *    \param	driver	  Name of the device driver
 *    \param	name	Name of the new device
 *    \param	cfg    Configuration list to assign to the new device
 *    \return	 A pointer to the new device object
 */
device_t *DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
			   device_config_t *cfg)
{
    device_t *dev;
    driver_t *drv;

    if (driver == NULL)
	driver = name;

    dev = IoOpenDevice(name);
    if (dev != NULL)
	return dev;

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
        else
            wprintf(L"%s.%s: failed to initialise\n", driver, name);
	return dev;
    }
    else
	return NULL;
}

/*!
 *    \brief	Temporarily maps the user-mode buffer of an asynchronous request
 *
 *    Use this function in the interrupt routine of a driver when data need to be
 *    written to the user buffer.
 *
 *    Note that the pointer returned by this function refers to the start of the
 *    lowest page in the buffer; you must add \p io->mod_buffer_start to obtain
 *    a usable buffer address.
 *
 *    Call \p DevUnmapBuffer when you have finished with the buffer returned.
 *
 *    \param	io    Asynchronous request structure
 *    \return	 A page-aligned pointer to the start of the buffer
 */
void *DevMapBuffer(asyncio_t *io)
{
    /*return MemMapTemp((addr_t*) (io + 1), io->length_pages, 
	PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);*/
    return MemMapPageArray(io->pages, PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
}
