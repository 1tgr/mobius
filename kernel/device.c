/* $Id: device.c,v 1.1 2002/12/21 09:49:11 pavlovskii Exp $ */

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
#include <string.h>

#include <os/defs.h>

typedef struct device_class_t device_class_t;
struct device_class_t
{
    uint16_t device_class;
    const wchar_t* name;
    const wchar_t *base;
    unsigned count;
};

static device_class_t classes[] =
{
    //{ 0x0000, L"Undefined",     L"device" },
    { 0x0001, L"VGA",           L"video" },
    { 0x0081, L"Disk Volume",   L"volume" },

    { 0x0100, L"SCSI",          L"scsi" },
    { 0x0101, L"IDE",           L"ide" },
    { 0x0102, L"Floppy",        L"floppy" },
    { 0x0103, L"IPI",           L"ipi" },
    { 0x0104, L"RAID",          L"raid" },
    { 0x0180, L"Other",         L"drive" },

    { 0x0200, L"Ethernet",      L"ethernet" },
    { 0x0201, L"Token Ring",    L"token" },
    { 0x0202, L"FDDI",          L"fddi" },
    { 0x0203, L"ATM",           L"atm" },
    { 0x0204, L"ISDN",          L"isdn" },
    { 0x0280, L"Other",         L"network" },

    { 0x0300, L"VGA",           L"video" },
    { 0x0300, L"VGA+8514",      L"video" },
    { 0x0301, L"XGA",           L"video" },
    { 0x0302, L"3D",            L"video" },
    { 0x0380, L"Other",         L"video" },

    { 0x0400, L"Video",         L"video" },
    { 0x0401, L"Audio",         L"audio" },
    { 0x0402, L"Telephony",     L"telephone" },
    { 0x0480, L"Other",         L"media" },

    { 0x0500, L"RAM",           L"ram" },
    { 0x0501, L"Flash",         L"flash" },
    { 0x0580, L"Other",         L"memory" },

    { 0x0600, L"PCI to HOST",   L"bridge" },
    { 0x0601, L"PCI to ISA",    L"bridge" },
    { 0x0602, L"PCI to EISA",   L"bridge" },
    { 0x0603, L"PCI to MCA",    L"bridge" },
    { 0x0604, L"PCI to PCI",    L"bridge" },
    { 0x0605, L"PCI to PCMCIA", L"bridge" },
    { 0x0606, L"PCI to NuBUS",  L"bridge" },
    { 0x0607, L"PCI to Cardbus", L"bridge" },
    { 0x0608, L"PCI to RACEway", L"bridge" },
    { 0x0609, L"PCI to PCI",    L"bridge" },
    { 0x060A, L"PCI to InfiBand", L"bridge" },
    { 0x0680, L"PCI to Other",  L"bridge"},

    { 0x0700, L"Serial",        L"serial" },
    { 0x0701, L"Parallel",      L"parallel" },
    { 0x0702, L"Multiport Serial", L"serial" },
    { 0x0703, L"Hayes Compatible Modem", L"modem" },
    { 0x0780, L"Other",         L"comm" },

    { 0x0800, L"PIC",           L"pic" },
    { 0x0801, L"DMA",           L"dma" },
    { 0x0802, L"Timer",         L"timer" },
    { 0x0803, L"RTC",           L"rtc" },
    { 0x0880, L"Other",         L"motherboard" },

    { 0x0900, L"Keyboard",      L"keyboard" },
    { 0x0901, L"Pen",           L"pen" },
    { 0x0902, L"Mouse",         L"mouse" },
    { 0x0903, L"Scanner",       L"scanner" },
    { 0x0904, L"Game Port",     L"game" },
    { 0x0980, L"Other",         L"input" },

    { 0x0a00, L"Generic",       L"device" },
    { 0x0a80, L"Other",         L"device" },

    { 0x0b00, L"386",           L"cpu" },
    { 0x0b01, L"486",           L"cpu" },
    { 0x0b02, L"Pentium",       L"cpu" },
    { 0x0b03, L"PentiumPro",    L"cpu" },
    { 0x0b10, L"DEC Alpha",     L"cpu" },
    { 0x0b20, L"PowerPC",       L"cpu" },
    { 0x0b30, L"MIPS",          L"cpu" },
    { 0x0b40, L"Coprocessor",   L"copro" },
    { 0x0b80, L"Other",         L"processor" },

    { 0x0c00, L"FireWire",      L"firewire" },
    { 0x0c01, L"Access.bus",    L"access" },
    { 0x0c02, L"SSA",           L"ssa" },
    { 0x0c03, L"USB",           L"usb" },
    { 0x0c04, L"Fiber",         L"fibre" },
    { 0x0c05, L"SMBus Controller", L"smbus" },
    { 0x0c06, L"InfiniBand",    L"infiniband" },
    { 0x0c80, L"Other",         L"bus" },

    { 0x0d00, L"iRDA",          L"irda" },
    { 0x0d01, L"Consumer IR",   L"ir" },
    { 0x0d10, L"RF",            L"rf" },
    { 0x0d80, L"Other",         L"wireless" },

    { 0x0e00, L"I2O",           L"i2o" },
    { 0x0e80, L"Other",         L"bus" },

    { 0x0f01, L"TV",            L"tv" },
    { 0x0f02, L"Audio",         L"audio" },
    { 0x0f03, L"Voice",         L"voice" },
    { 0x0f04, L"Data",          L"data" },
    { 0x0f80, L"Other",         L"media" },

    { 0x1000, L"Network",       L"network" },
    { 0x1010, L"Entertainment", L"entertainment" },
    { 0x1080, L"Other",         L"" },

    { 0x1100, L"DPIO Modules",  L"dpio" },
    { 0x1101, L"Performance Counters", L"counter" },
    { 0x1110, L"Comm Sync, Time+Frequency Measurement", L"meter" },
    { 0x1180, L"Other",         L"acquire" },
};

extern module_t mod_kernel;
extern thread_queue_t thr_finished;

typedef struct device_info_t device_info_t;
struct device_info_t
{
    device_info_t *next, *prev, *parent;
    const wchar_t *name;
    bool is_link;
    unsigned refs;

    union
    {
        struct
        {
            device_t *dev;
            device_config_t *cfg;
            device_info_t *child_first, *child_last;
        } info;

        struct
        {
            device_info_t *target;
        } link;
    } u;
};

irq_t *irq_first[16], *irq_last[16];
driver_t *drv_first, *drv_last;

extern device_info_t dev_root;

device_info_t dev_classes = 
{
    NULL, NULL,
    &dev_root,
    L"Classes",
    false,
    0
};

device_info_t dev_root =
{
    NULL, NULL,
    NULL,
    L"(root)",
    false,
    0,
    { { NULL, NULL, &dev_classes, &dev_classes } }
};

device_info_t *DfsParsePath(const wchar_t *path)
{
    device_info_t *parent, *info;
    const wchar_t *slash;

    parent = &dev_root;
    if (*path == '/')
        path++;

    while (*path != '\0')
    {
        slash = wcschr(path, '/');
        if (slash == NULL)
            slash = path + wcslen(path);

        for (info = parent->u.info.child_first; info != NULL; info = info->next)
            if (wcslen(info->name) == (size_t) (slash - path) &&
                _wcsnicmp(path, info->name, slash - path) == 0)
                break;

            if (info == NULL)
                return NULL;

            parent = info;
            if (*slash == '/')
                path = slash + 1;
            else
                path = slash;

            while (parent->is_link)
            {
                assert(parent->u.link.target != parent);
                parent = parent->u.link.target;
            }
    }

    return parent;
}

status_t DfsParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
{
    device_info_t *info, *parent;
    unsigned num_levels;
    size_t len, len2;
    wchar_t *link_to;

    if (node->id == VNODE_ROOT)
        info = &dev_root;
    else
        info = (device_info_t*) node->id;

    assert(!info->is_link);

    if (wcscmp(name, L"..") == 0)
    {
        if (info->parent == NULL)
            node->id = VNODE_ROOT;
        else
            node->id = (vnode_id_t) info->parent;

        return 0;
    }
    else
        for (info = info->u.info.child_first; info != NULL; info = info->next)
            if (_wcsicmp(info->name, name) == 0)
            {
                if (info->is_link)
                {
                    num_levels = 0;
                    len = wcslen(SYS_DEVICES) + 2;
                    for (parent = info->u.link.target; 
                        parent != &dev_root; 
                        parent = parent->parent)
                    {
                        wprintf(L"%s/", parent->name);
                        len += wcslen(parent->name) + 1;
                    }

                    //len *= 2;
                    wprintf(L" len = %u ", len);

                    *new_path = link_to = malloc(sizeof(wchar_t) * len);
                    wcscpy(link_to, SYS_DEVICES);
                    link_to += wcslen(SYS_DEVICES);
                    len = 1;
                    for (parent = info->u.link.target; 
                        parent != &dev_root; 
                        parent = parent->parent)
                    {
                        len2 = wcslen(parent->name) + 1;
                        memmove(link_to + len2, link_to, len * sizeof(wchar_t));

                        *link_to = '/';
                        //link_to++;
                        memcpy(link_to + 1, parent->name, sizeof(wchar_t) * (len2 - 1));

                        len += len2;
                        //link_to += len2;
                    }

                    len += wcslen(SYS_DEVICES) + 1;
                    wprintf(L" len = %u, new_path = %s\n", len, *new_path);
                    info = info->u.link.target;
                }

                node->id = (vnode_id_t) info;
                return 0;
            }

    return ENOTFOUND;
}

status_t DfsLookupFile(fsd_t *fsd, vnode_id_t id, uint32_t open_flags, void **cookie)
{
    /*device_info_t *info;

    info = DfsParsePath(path);
    if (info == NULL)
    {
        wprintf(L"DfsLookupFile(%s): not found\n", path);
        return ENOTFOUND;
    }
    else
    {
        assert(!info->is_link);
        *cookie = info;
        return 0;
    }*/

    if (id == VNODE_ROOT)
        *cookie = &dev_root;
    else
        *cookie = (device_info_t*) id;

    return 0;
}

status_t DfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    device_info_t *info;
    dirent_all_t *di;

    info = cookie;
    di = buf;
    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_DIRENT:
        wcsncpy(di->dirent.name, info->name, _countof(di->dirent.name) - 1);
        di->dirent.vnode = 0;
        return 0;

    case FILE_QUERY_STANDARD:
        di->standard.length = 0;
        di->standard.attributes = FILE_ATTR_DEVICE;
        if (info->u.info.child_first != NULL)
            di->standard.attributes |= FILE_ATTR_DIRECTORY;

        wcscpy(di->standard.mimetype, L"");
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
    dev = info->u.info.dev;
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
    ret = IoRequestSync(info->u.info.dev, &req.header);

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
    ret = IoRequestSync(info->u.info.dev, &req_dev->header);
    io->op.result = io->original->result = req_dev->header.result;

    memcpy(buf, &req_dev->params, length);

    FsNotifyCompletion(io, length, req_dev->header.result);
    free(req_dev);
    return ret;
}

status_t DfsOpenDir(fsd_t *fsd, vnode_id_t node, void **dir_cookie)
{
    device_info_t **ptr, *info;

    //info = DfsParsePath(path);
    //if (info == NULL)
        //return ENOTFOUND;

    if (node == VNODE_ROOT)
        info = &dev_root;
    else
        info = (device_info_t*) node;

    assert(!info->is_link);

    ptr = malloc(sizeof(device_info_t*));
    if (ptr == NULL)
        return errno;

    *ptr = info->u.info.child_first;
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

static const struct vtbl_fsd_t devfs_vtbl =
{
    NULL,           /* dismount */
    NULL,           /* get_fs_info */
    DfsParseElement,
    NULL,           /* create_file */
    DfsLookupFile,
    DfsGetFileInfo,
    NULL,           /* set_file_info */
    NULL,           /* free_cookie */
    DfsRead,
    DfsWrite,
    DfsIoCtl,
    DfsPassthrough,
    NULL,           /* mkdir */
    DfsOpenDir,
    DfsReadDir,
    DfsFreeDirCookie,
    //NULL,           /* mount */
    DfsFinishIo,
    NULL,           /* flush_cache */
};

static fsd_t dev_fsd =
{
    &devfs_vtbl,
};

fsd_t *DfsMountFs(driver_t *drv, const wchar_t *dest)
{
    return &dev_fsd;
}

static driver_t devfs_driver =
{
    &mod_kernel,
    NULL,
    NULL,
    NULL,
    NULL,
    DfsMountFs,
};

/*
* IoOpenDevice and IoCloseDevice are in device.c to save io.c knowing about
*    device_info_t.
*/

device_t *IoOpenDevice(const wchar_t *name)
{
    device_info_t *info;

    info = DfsParsePath(name);
    if (info == NULL)
    {
        //wprintf(L"IoOpenDevice(%s): not found\n", name);
        errno = ENOTFOUND;
        return NULL;
    }
    else
        return info->u.info.dev;
}

void IoCloseDevice(device_t *dev)
{
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
    thread_t *thr;

    if (req != NULL)
    {
        /*
         * It should be possible to pass a NULL request here, as long as the caller set 
         *  req->result.
         */
        req->result = SIOPENDING;
        req->original = NULL;
    }

    io = malloc(sizeof(asyncio_t));
    if (io == NULL)
        return NULL;

    io->pages = MemCopyPageArray(pages);
    if (io->pages == NULL)
    {
        free(io);
        return NULL;
    }

    thr = current();
    io->owner = thr;

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
    io->extra = NULL;

    /* xxx -- SpinAcquire(&thr->sem); */
    io->thread_next = NULL;
    io->thread_prev = thr->aio_last;
    if (thr->aio_last != NULL)
        thr->aio_last->thread_next = io;
    if (thr->aio_first == NULL)
        thr->aio_first = io;
    /* xxx -- SpinRelease(&thr->sem); */

    /* xxx -- SpinAcquire(&dev->sem); */
    LIST_ADD(dev->io, io);
    /* xxx -- SpinRelease(&dev->sem); */
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
        assert(io->owner->process == current()->process);
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

    free(io);
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
    thread_t *thr;

    MemDeletePageArray(io->pages);
    io->pages = NULL;

    /* xxx -- SpinAcquire(&dev->sem); */
    LIST_REMOVE(dev->io, io);
    /* xxx -- SpinRelease(&dev->sem); */

    thr = io->owner;
    /* xxx -- SpinAcquire(&thr->sem); */
    if (io->thread_next != NULL)
        io->thread_next->thread_prev = io->thread_prev;
    if (io->thread_prev != NULL)
        io->thread_prev->thread_next = io->thread_next;
    if (thr->aio_first == io)
        thr->aio_first = io->thread_next;
    if (thr->aio_last == io)
        thr->aio_last = io->thread_prev;
    io->thread_next = io->thread_prev = NULL;
    /* xxx -- SpinRelease(&thr->sem); */

    io->req->result = result;

    if (io->owner->process == current()->process)
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
 *  \brief  Cancels an I/O request
 *
 *  I/O requests can be cancelled if the originator is no longer interested in
 *  receiving the results of the operation. For example, when a thread exits,
 *  \p ThrExitThread calls \p DevCancelIo for each pending I/O request.
 *
 *  This function calls the device's \p cancelio routine if available; 
 *  otherwise, \p DevFinishIo is called with \p ECANCELLED status.
 *
 *  \param  io  Asynchronous request to cancel
 *  \return \p true if the request could be cancelled, \p false otherwise
 */
bool DevCancelIo(asyncio_t *io)
{
    device_t *dev;

    dev = io->dev;
    if (dev->vtbl->cancelio == NULL)
    {
        DevFinishIo(dev, io, ECANCELLED);
        return true;
    }
    else
        return dev->vtbl->cancelio(dev, io);
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
    device_resource_t *res;

    res = cfg->resources;
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
    device_resource_t *res;

    res = cfg->resources;
    for (i = j = 0; i < cfg->num_resources; i++)
        if (res[i].cls == resMemory)
        {
            if (j == n)
                return res + i;

            j++;
        }

        return NULL;
}

extern driver_t rd_driver, port_driver, vfs_driver;
#ifdef WIN32
extern driver_t win32fs_driver;
#endif

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
driver_t *DevInstallNewDriver(const wchar_t *name, const wchar_t *profile_key)
{
    if (_wcsicmp(name, L"ramfs") == 0)
        return &rd_driver;
    else if (_wcsicmp(name, L"portfs") == 0)
        return &port_driver;
    else if (_wcsicmp(name, L"devfs") == 0)
        return &devfs_driver;
    else if (_wcsicmp(name, L"vfs") == 0)
        return &vfs_driver;
#ifdef WIN32
    else if (_wcsicmp(name, L"win32fs") == 0)
        return &win32fs_driver;
#endif
    else
    {
        wchar_t temp[50];
        module_t *mod;
        driver_t *drv;
        bool (*DrvInit)(driver_t *drv);

        /*swprintf(temp, SYS_BOOT L"/%s.drv", name);*/
        swprintf(temp, L"%s.drv", name);
        //wprintf(L"DevInstallNewDriver: loading %s\n", temp);

        mod = PeLoad(&proc_idle, temp, 0);
        if (mod == NULL)
            return NULL;

        FOREACH(drv, drv)
            if (drv->mod == mod)
                return drv;

        drv = malloc(sizeof(driver_t));
        if (drv == NULL)
        {
            PeUnload(&proc_idle, mod);
            return NULL;
        }

        drv->mod = mod;
        drv->add_device = NULL;
        drv->mount_fs = NULL;
        drv->profile_key = _wcsdup(profile_key);

        DrvInit = (void*) mod->entry;
        //wprintf(L"DevInstallNewDriver: DrvInit = %p\n", DrvInit);
        if (DrvInit == NULL || !DrvInit(drv))
        {
            PeUnload(&proc_idle, mod);
            free(drv);
            return NULL;
        }

        //wprintf(L"DevInstallNewDriver: done\n");
        LIST_ADD(drv, drv);
        return drv;
    }
}

/*! 
 *  \brief  Adds a new device to the device manager's list
 *
 *  The new device will appear in the \p /System/Device directory, and it
 *  will also be available via \p IoOpenDevice. Call this function from a 
 *  driver's \p add_device routine for any devices found.
 *
 *  \param  dev     Pointer to the new device object
 *  \param  name    Name for the device
 *  \param  cfg     Device configuration list
 *  \return \p true if the device was added, or \p false otherwise
 */
bool DevAddDevice(device_t *dev, const wchar_t *name, device_config_t *cfg)
{
    device_info_t *info, *parent, *link;
    wchar_t link_name[20];
    device_class_t *class;
    unsigned i;

    /*wprintf(L"DevAddDevice: added %s\n", name);*/

    if (cfg == NULL || cfg->parent == NULL)
        parent = NULL;
    else
        parent = cfg->parent->info;

    if (parent == NULL)
        parent = &dev_root;

    class = NULL;
    if (cfg != NULL)
    {
        for (i = 0; i < _countof(classes); i++)
            if (classes[i].device_class == cfg->device_class)
            {
                class = classes + i;
                break;
            }

            //if (class == NULL)
                //class = classes + 0;
            //else
            if (class != NULL)
                for (i = 0; i < _countof(classes); i++)
                    if (classes + i == class ||
                        _wcsicmp(classes[i].base, class->base) == 0)
                    {
                        class = classes + i;
                        break;
                    }
    }

    info = malloc(sizeof(device_info_t));
    if (info == NULL)
        return false;

    /*wprintf(L"DevAddDevice: adding device %s to parent %p = %s\n",
        name, parent, parent->name);*/

    info->name = _wcsdup(name);
    info->is_link = false;
    info->parent = parent;
    info->u.info.dev = dev;
    info->u.info.cfg = cfg;
    info->u.info.child_first = info->u.info.child_last = NULL;
    dev->info = info;
    LIST_ADD(parent->u.info.child, info);

    if (class != NULL)
    {
        swprintf(link_name, L"%s%u", class->base, class->count);
        KeAtomicInc(&class->count);

        link = malloc(sizeof(device_info_t));
        if (link != NULL)
        {
            link->name = _wcsdup(link_name);
            link->is_link = true;
            link->parent = parent;
            link->u.link.target = info;
            LIST_ADD(dev_classes.u.info.child, link);
        }
    }

    return true;
}

/*!
 *  \brief  Initialises the standard fields in a \p device_t structure
 *
 *  Drivers can call this function to initialise their devices' fields to
 *  the correct values. The \p device_t structure must be allocated beforehand
 *  (e.g. using \p malloc ).
 *
 *  \param  dev     Device structure to be initialised
 *  \param  vtbl    Function table for this device
 *  \param  drv     Driver which handles this device
 *  \param  flags   Flags which define the device manager's interactions with 
 *      the device. This is a bitwise OR of some of the following values:
 *  - \p DEVICE_IO_PAGED: when set, requests that the device manager provides
 *      read and write buffers in the form of a \p page_array_t structure, 
 *      whose memory can be accessed using \p MemMapPageArray. This is set if
 *      \p DEVICE_IO_DIRECT is not set.
 *  - \p DEVICE_IO_DIRECT: when set, requests that the device manager provides
 *      an ordinary memory buffer for reads and writes.
 */
void DevInitDevice(device_t *dev, const device_vtbl_t *vtbl, driver_t *drv, 
                   uint32_t flags)
{
    dev->driver = drv;
    dev->cfg = NULL;    /* gets set by DevAddDevice */
    dev->io_first = dev->io_last = NULL;
    dev->info = NULL;   /* gets set by DevAddDevice */
    dev->flags = flags;
    dev->vtbl = vtbl;
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
 *  \brief  Installs a new device
 *
 *  This function will ask the specified driver for the given device;
 *  the driver is loaded if not already present.
 *
 *  \param  driver      Name of the device driver
 *  \param  name        Name of the new device
 *  \param  cfg         Configuration list to assign to the new device
 *  \param  profile_key Path to the device's profile key
 *  \return \p true if the driver could be loaded (or was already present),
 *      \p false otherwise
 */
bool DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
                      device_config_t *cfg, const wchar_t *profile_key)
{
    device_t *dev;
    driver_t *drv;

    if (driver == NULL)
        driver = name;

    dev = IoOpenDevice(name);
    if (dev != NULL)
        return true;

    drv = DevInstallNewDriver(driver, profile_key);
    if (drv != NULL)
    {
        if (drv->add_device != NULL)
            drv->add_device(drv, name, cfg);

        return true;
    }
    else
    {
        wprintf(L"%s.%s: driver not loaded\n", driver, name);
        return false;
    }
}

/*!
 *  \brief  Temporarily maps the user-mode buffer of an asynchronous request
 *
 *  Use this function in the interrupt routine of a driver when data need to be
 *  written to the user buffer.
 *
 *  Call \p DevUnmapBuffer when you have finished with the buffer returned.
 *
 *  \param  io  Asynchronous request structure
 *  \return A pointer to the start of the buffer
 */
void *DevMapBuffer(asyncio_t *io)
{
/*return MemMapTemp((addr_t*) (io + 1), io->length_pages, 
    PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);*/
    return MemMapPageArray(io->pages, PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
}
