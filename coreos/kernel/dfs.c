/*
 * $History: dfs.c $
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 7/03/04    Time: 15:01
 * Created in $/coreos/kernel
 */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>
#include <kernel/profile.h>

#include <kernel/debug.h>

#include <string.h>
#include <errno.h>
#include <wchar.h>

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
            dev_config_t *cfg;
            device_info_t *child_first, *child_last;
        } info;

        struct
        {
            device_info_t *target;
        } link;
    } u;
};

extern device_info_t dev_root;

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
    { 0x0001, L"VGA",           L"video" },
    { 0x0081, L"Disk Volume",   L"volume" },
    { 0x0082, L"CD-ROM Drive",    L"cdrom" },

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

extern struct module_t mod_kernel;


static device_info_t *DfsParsePath(const wchar_t *path)
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


static status_t DfsParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
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
                        len += wcslen(parent->name) + 1;

                    //len *= 2;

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
                    info = info->u.link.target;
                }

                node->id = (vnode_id_t) info;
                return 0;
            }

    return ENOTFOUND;
}


static status_t DfsLookupFile(fsd_t *fsd, vnode_id_t id, uint32_t open_flags, void **cookie)
{
    if (id == VNODE_ROOT)
        *cookie = &dev_root;
    else
        *cookie = (device_info_t*) id;

    return 0;
}


static status_t DfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
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

    case FILE_QUERY_DEVICE:
        if (info->u.info.dev->cfg == NULL)
        {
            memset(di->device.description, 0, sizeof(di->device.description));
            di->device.device_class = 0;
        }
        else
        {
            wcsncpy(di->device.description,
                ProGetString(info->u.info.dev->cfg->profile_key, L"Description", L""),
                _countof(di->device.description));

            di->device.device_class = info->u.info.dev->cfg->device_class;
        }

        return 0;
    }

    return ENOTIMPL;
}


static status_t DfsReadWrite(fsd_t *fsd, const fs_request_t *fsreq, size_t *bytes)
{
    device_t *dev;
    device_info_t *info;
    request_dev_t *req_dev;
    io_callback_t cb;
    status_t ret;

    req_dev = malloc(sizeof(request_dev_t));
    if (req_dev == NULL)
    {
        *bytes = 0;
        return errno;
    }

    info = fsreq->file->fsd_cookie;
    dev = info->u.info.dev;
    req_dev->header.code = fsreq->is_reading ? DEV_READ : DEV_WRITE;
    req_dev->header.param = fsreq->io;
    req_dev->params.dev_read.pages = fsreq->pages;
    req_dev->params.dev_read.length = fsreq->length;
    req_dev->params.dev_read.offset = fsreq->pos;

    cb.type = IO_CALLBACK_FSD;
    cb.u.fsd = fsd;
    ret = IoRequest(&cb, dev, &req_dev->header);

    if (ret > 0)
    {
        *bytes = req_dev->params.buffered.length;
        free(req_dev);
        return ret;
    }

    /*
     * DfsFinishIo *will* get called upon every successful request: 
     *  for async io: when the device finishes
     *   for sync io: by IoRequest
     * Therefore, since DfsFinishIo frees req_dev, we can't use it from now on.
     * Also, FsNotifyCompletion will free io, so we can't use that either.
     */

    return ret;
}


static void DfsFinishIo(fsd_t *fsd, request_t *req)
{
    request_dev_t *req_dev;
    fs_asyncio_t *io;

    req_dev = (request_dev_t*) req;
    TRACE2(L"DfsFinishIo: req = %p, result = %d\n", req, req_dev->header.result);
    io = req_dev->header.param;
    assert(io != NULL);

    FsNotifyCompletion(io, 
        req_dev->params.buffered.length, 
        req_dev->header.result);
    req_dev->header.code = 0;
    free(req_dev);
}


static bool DfsIoCtl(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                     size_t length, fs_asyncio_t *io)
{
    request_dev_t req;
    status_t ret;
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


static bool DfsPassthrough(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                           size_t length, fs_asyncio_t *io)
{
    request_dev_t *req_dev;
    status_t ret;
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
    io->op.result = io->original->result = ret;

    memcpy(buf, &req_dev->params, length);

    if (ret == 0)
        FsNotifyCompletion(io, length, ret);

    free(req_dev);
    return ret == 0;
}


static status_t DfsOpenDir(fsd_t *fsd, vnode_id_t node, void **dir_cookie)
{
    device_info_t **ptr, *info;

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


static status_t DfsReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
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


static void DfsFreeDirCookie(fsd_t *fsd, void *dir_cookie)
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
    DfsReadWrite,
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


static fsd_t *DfsMountFs(driver_t *drv, const wchar_t *dest)
{
    return &dev_fsd;
}


driver_t devfs_driver =
{
    &mod_kernel,
    NULL,
    NULL,
    NULL,
    DfsMountFs,
};


/*
* IoOpenDevice and IoCloseDevice are in here to save io.c knowing about
*    device_info_t.
*/

device_t *IoOpenDevice(const wchar_t *name)
{
    device_info_t *info;

    info = DfsParsePath(name);
    if (info == NULL)
    {
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
 *  \brief  Adds a new device to the device manager's list
 *
 *  The new device will appear in the \p /System/Device directory, and it
 *  will also be available via \p IoOpenDevice. Call this function from a 
 *  driver's \p add_device routine for any devices found.
 *
 *  \param  drv     Driver which handles this device
 *  \param  vtbl    Function table for this device
 *  \param  flags   Flags which define the device manager's interactions with 
 *      the device.
 *  \param  name    Name for the device
 *  \param  cfg     Device configuration list
 *  \return \p true if the device was added, or \p false otherwise
 */
device_t *DevAddDevice(driver_t *drv, const device_vtbl_t *vtbl, uint32_t flags,
                       const wchar_t *name, dev_config_t *cfg, void *cookie)
{
    device_t *dev;
    device_info_t *info, *parent, *link;
    wchar_t link_name[20];
    device_class_t *class;
    unsigned i;

    dev = malloc(sizeof(*dev));
    if (dev == NULL)
        return NULL;

    dev->driver = drv;
    dev->cookie = cookie;
    dev->cfg = cfg;
    dev->io_first = dev->io_last = NULL;
    dev->flags = flags;
    dev->vtbl = vtbl;

    info = malloc(sizeof(*info));
    if (info == NULL)
    {
        free(dev);
        return NULL;
    }

    dev->info = info;

    if (cfg == NULL || cfg->bus == NULL)
        parent = NULL;
    else
        parent = cfg->bus->info;

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

            if (class != NULL)
                for (i = 0; i < _countof(classes); i++)
                    if (classes + i == class ||
                        _wcsicmp(classes[i].base, class->base) == 0)
                    {
                        class = classes + i;
                        break;
                    }
    }

    info->name = _wcsdup(name);
    info->is_link = false;
    info->parent = parent;
    info->u.info.dev = dev;
    info->u.info.cfg = cfg;
    info->u.info.child_first = info->u.info.child_last = NULL;
    LIST_ADD(parent->u.info.child, info);

    if (class != NULL)
    {
        swprintf(link_name, L"%s%u", class->base, class->count);
        KeAtomicInc(&class->count);

        link = malloc(sizeof(*info));
        if (link != NULL)
        {
            link->name = _wcsdup(link_name);
            link->is_link = true;
            link->parent = parent;
            link->u.link.target = info;
            LIST_ADD(dev_classes.u.info.child, link);
        }
    }

    return dev;
}
