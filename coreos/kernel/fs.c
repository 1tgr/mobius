/*
 * $History: fs.c $
 * 
 * *****************  Version 5  *****************
 * User: Tim          Date: 13/03/04   Time: 22:18
 * Updated in $/coreos/kernel
 * Don't unload failed drivers on mount for now
 * 
 * *****************  Version 4  *****************
 * User: Tim          Date: 8/03/04    Time: 20:31
 * Updated in $/coreos/kernel
 * Moved lots of things into their own files
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 7/03/04    Time: 13:04
 * Updated in $/coreos/kernel
 * Fixed bug in FsWrite
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:07
 * Updated in $/coreos/kernel
 * Improved comments
 * Added a check in FsCleanupDir so that uninitialised directory handles
 * don't cause crashes
 * Put fs_asyncio_t creation/deletion code into separate functions
 * Added support for multiple sync I/O events, for recursive
 * FsRead/FsWrite per thread
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/slab.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/init.h>

#include <kernel/debug.h>

#include <string.h>
#include <errno.h>
#include <wchar.h>

vnode_t fs_root;

/*
 * Structure representing a file system mounted on top of another
 */
typedef struct fs_mount_t fs_mount_t;
struct fs_mount_t
{
    fs_mount_t *prev, *next;
    vnode_t node;
    vnode_t root;
};

// List of mount points
static spinlock_t sem_mounts;
fs_mount_t *fs_mount_first, *fs_mount_last;

static slab_t slab_file_handle;
static slab_t slab_mount;

/*
 * Cleanup callback for file_handle_t objects
 */
static void FsCleanupFileHandle(void *hdr)
{
    file_handle_t *fh;
    fh = hdr;
    FsReleaseFile(fh->file);
    SlabFree(&slab_file_handle, fh);
}


static void FsConstructFileHandle(void *ptr)
{
    static handle_class_t file_class = 
        HANDLE_CLASS('file', FsCleanupFileHandle, NULL, NULL, NULL);
    file_handle_t *fh;
    fh = ptr;
    HndInit(&fh->hdr, &file_class);
}


/*
 * Create a file_handle_t object
 */
file_handle_t *FsCreateFileHandle(file_t *file, uint32_t flags)
{
    file_handle_t *fh;

    fh = SlabAlloc(&slab_file_handle);
    if (fh == NULL)
        return 0;

    fh->flags = flags;
    fh->file = file;

    return fh;
}


/*
 * Attempt to find a mounted file system on top of the given vnode
 */
bool FsLookupMountPoint(vnode_t *out, const vnode_t *in)
{
    fs_mount_t *mount;

    // Search through all the mounted file systems for a match

    SpinAcquire(&sem_mounts);

    for (mount = fs_mount_first; mount != NULL; mount = mount->next)
    {
        if (mount->node.fsd == in->fsd &&
            mount->node.id == in->id)
        {
            *out = mount->root;
            SpinRelease(&sem_mounts);
            return true;
        }
    }

    SpinRelease(&sem_mounts);

    if (out != in)
        *out = *in;
    return false;
}


bool FsQueryFile(const wchar_t *name, uint32_t query_class, void *buffer, size_t buffer_size)
{
    void *cookie;
    status_t ret;
    vnode_t node;

    if (!MemVerifyBuffer(buffer, buffer_size))
    {
        errno = EBUFFER;
        return false;
    }

    ret = FsPathToVnode(&fs_root, name, &node, false, NULL);
    if (ret == 0)
    {
        if (node.fsd->vtbl->lookup_file == NULL)
            ret = ENOTIMPL;
        else
        {
            ret = node.fsd->vtbl->lookup_file(node.fsd, node.id, 0, &cookie);
        }
    }

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    if (node.fsd->vtbl->get_file_info == NULL)
        ret = ENOTIMPL;
    else
        ret = node.fsd->vtbl->get_file_info(node.fsd, cookie, query_class, buffer);

    if (node.fsd->vtbl->free_cookie != NULL)
        node.fsd->vtbl->free_cookie(node.fsd, cookie);

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    return true;
}


bool FsQueryHandle(file_handle_t *fh, uint32_t query_class, void *buffer, size_t buffer_size)
{
    status_t ret;
    file_t *fd;

    if (!MemVerifyBuffer(buffer, buffer_size))
    {
        errno = EBUFFER;
        return false;
    }

    fd = fh->file;

    if (fd->vnode.fsd->vtbl->get_file_info == NULL)
        ret = ENOTIMPL;
    else
        ret = fd->vnode.fsd->vtbl->get_file_info(fd->vnode.fsd, fd->fsd_cookie, query_class, buffer);

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    return true;
}


static const struct
{
    const wchar_t *ext;
    const wchar_t *mimetype;
} mimetypes[] =
{
    { L"dll", L"application/x-pe-library" },
    { L"drv", L"application/x-pe-driver" },
    { L"exe", L"application/x-pe-program" },
    { L"gz", L"application/x-gzip" },
    { L"hqx", L"application/x-binhex40" },
    { L"lha", L"application/x-lharc" },
    { L"pcl", L"application/x-pcl" },
    { L"pdf", L"application/pdf" },
    { L"ps", L"application/postscript" },
    { L"sit", L"application/x-stuff-it" },
    { L"tar", L"application/x-tar" },
    { L"tgz", L"application/x-gzip" },
    { L"uue", L"application/x-uuencode" },
    { L"z", L"application/x-compress" },
    { L"zip", L"application/zip" },
    { L"zoo", L"application/x-zoo" },

    { L"aif", L"audio/x-aiff" },
    { L"aiff", L"audio/x-aiff" },
    { L"au", L"audio/basic" },
    { L"mid", L"audio/x-midi" },
    { L"midi", L"audio/x-midi" },
    { L"mod", L"audio/mod" },
    { L"ra", L"audio/x-real-audio" },
    { L"wav", L"audio/x-wav" },

    { L"bmp", L"image/x-bmp" },
    { L"fax", L"image/g3fax" },
    { L"gif", L"image/gif" },
    { L"iff", L"image/x-iff" },
    { L"jpg", L"image/jpeg" },
    { L"jpeg", L"image/jpeg" },
    { L"pbm", L"image/x-portable-bitmap" },
    { L"pcx", L"image/x-pcx" },
    { L"pgm", L"image/x-portable-graymap" },
    { L"png", L"image/png" },
    { L"ppm", L"image/x-portable-pixmap" },
    { L"rgb", L"image/x-rgb" },
    { L"tga", L"image/x-targa" },
    { L"tif", L"image/tiff" },
    { L"tiff", L"image/tiff" },
    { L"wmf", L"image/x-wmf" },
    { L"xbm", L"image/x-xbitmap" },

    { L"txt", L"text/plain" },
    { L"doc", L"text/plain" },
    { L"htm", L"text/html" },
    { L"html", L"text/html" },
    { L"rtf", L"text/rtf" },
    { L"asm", L"text/x-source-code" },
    { L"c", L"text/x-source-code" },
    { L"cc", L"text/x-source-code" },
    { L"c++", L"text/x-source-code" },
    { L"h", L"text/x-source-code" },
    { L"hh", L"text/x-source-code" },
    { L"cxx", L"text/x-source-code" },
    { L"cpp", L"text/x-source-code" },
    { L"S", L"text/x-source-code" },
    { L"java", L"text/x-source-code" },

    { L"avi", L"video/x-msvideo" },
    { L"mov", L"video/quicktime" },
    { L"mpg", L"video/mpeg" },
    { L"mpeg", L"video/mpeg" },

    { 0, 0 }
};

bool FsGuessMimeType(const wchar_t *ext, wchar_t *mimetype, size_t length)
{
    unsigned i;

    memset(mimetype, 0, sizeof(wchar_t) * length);
    if (ext != NULL)
    {
        if (ext[0] == '.')
            ext++;

        for (i = 0; mimetypes[i].ext != NULL; i++)
            if (_wcsicmp(ext, mimetypes[i].ext) == 0)
            {
                wcsncpy(mimetype, mimetypes[i].mimetype, length);
                return true;
            }
    }

    return false;
}


static bool FsDismountVnode(const vnode_t *node)
{
    fs_mount_t *mount;

    SpinAcquire(&sem_mounts);
    mount = fs_mount_first;
    while (mount != NULL)
    {
        if (mount->node.fsd == node->fsd)
        {
            vnode_t root;

            root = mount->root;
            wprintf(L"FsDismountVnode: dismounting %p/%u\n",
                root.fsd, root.id);
            SpinRelease(&sem_mounts);

            FsDismountVnode(&root);

            SpinAcquire(&sem_mounts);
            LIST_REMOVE(fs_mount, mount);
            SlabFree(&slab_mount, mount);
            mount = fs_mount_first;
        }
        else
            mount = mount->next;
    }
    SpinRelease(&sem_mounts);

    if (node->fsd->vtbl->dismount != NULL)
        node->fsd->vtbl->dismount(node->fsd);

    return true;
}


bool FsMountDevice(const wchar_t *path, fsd_t *newfsd)
{
    status_t ret;
    vnode_t node;
    fs_mount_t *mount;

    if (wcscmp(path, L"/") == 0)
    {
        if (fs_root.fsd != NULL && 
            !FsDismountVnode(&fs_root))
            return false;

        fs_root.fsd = newfsd;
        fs_root.id = VNODE_ROOT;
        return true;
    }
    else
    {
        ret = FsPathToVnode(&fs_root, path, &node, false, NULL);
        if (ret != 0)
        {
            errno = ret;
            return false;
        }

        if (node.id == VNODE_ROOT &&
            !FsDismountVnode(&node))
            return false;

        mount = SlabAlloc(&slab_mount);
        if (mount == NULL)
            return false;

        mount->node = node;
        mount->root.fsd = newfsd;
        mount->root.id = VNODE_ROOT;
        SpinAcquire(&sem_mounts);
        LIST_ADD(fs_mount, mount);
        SpinRelease(&sem_mounts);

        return true;
    }
}


/*!
 *    \brief    Mounts a file system in a directory
 *
 *    The target directory must support the \p FS_MOUNT requests. Virtual
 *    folders (e.g. \p / and \p /System) support this.
 *
 *    \param    path    Full path specification of the new mount point
 *    \param    filesys    Name of the file system driver to use
 *    \param    dev    Device for the file system to use
 *    \return    \p true if the file system was mounted
 */
bool FsMount(const wchar_t *path, const wchar_t *filesys, const wchar_t *dest)
{
    driver_t *driver;
    fsd_t *fsd;

    //wprintf(L"FsMount: %s at %s using %s\n",
        //dest, path, filesys);

    driver = DevInstallNewDriver(filesys);
    if (driver == NULL)
    {
        wprintf(L"%s: unknown driver\n", filesys);
        return false;
    }

    if (driver->mount_fs == NULL)
    {
        //DevUnloadDriver(driver);
        return false;
    }

    fsd = driver->mount_fs(driver, dest);
    if (fsd == NULL)
    {
        //DevUnloadDriver(driver);
        return false;
    }

    assert(fsd->vtbl != NULL);
    if (!FsMountDevice(path, fsd))
    {
        if (fsd->vtbl->dismount != NULL)
            fsd->vtbl->dismount(fsd);
        return false;
    }

    return true;
}


bool FsDismount(const wchar_t *path)
{
    vnode_t node;
    status_t ret;

    ret = FsPathToVnode(&fs_root, path, &node, false, NULL);
    if (ret > 0)
    {
        errno = ret;
        return false;
    }

    return FsDismountVnode(&node);
}


bool FsChangeDir(const wchar_t *path)
{
    status_t ret;
    vnode_t node;
    process_t *proc;

    proc = current()->process;
    if (!FsFullPath(path, proc->info->cwd))
        return false;

    TRACE2("FsChangeDir: changing to %s = %s\n", path, proc->info->cwd);
    ret = FsPathToVnode(&fs_root, proc->info->cwd, &node, false, NULL);
    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    FsLookupMountPoint(&node, &node);
    proc->cur_dir = node;
    return true;
}


/*!
 *    \brief    Initializes the filing system
 *
 *    This function is called to mount any system and boot file systems. It 
 *    will mount:
 *    - The ramdisk on \p /System/Boot
 *    - The port file system on \p /System/Ports
 *    - The device file system on \p /System/Devices
 *    - (\p /System/Devices/ide0a on \p /hd using \p fat )
 *    - \p /System/Devices/fdc0 on \p /hd using \p fat
 *
 *    \return    \p true if all devices were mounted correctly
 */
bool FsInit(void)
{
    bool b;

    if (!FsInitFile())
        return false;

    SlabInit(&slab_file_handle, sizeof(file_handle_t), FsConstructFileHandle, HndCleanup);
    SlabInit(&slab_mount, sizeof(fs_mount_t), NULL, NULL);

    b = FsMount(L"/", L"vfs", NULL);
    assert(b || "Failed to mount VFS root");
    FsCreateDir(L"/System");
    FsCreateDir(SYS_BOOT);
    FsCreateDir(SYS_DEVICES);
    FsCreateDir(SYS_PORTS);
    b = FsMount(SYS_BOOT, L"ramfs", NULL);
    assert(b || "Failed to mount ramdisk");
    b = FsMount(SYS_PORTS, L"portfs", NULL);
    assert(b || "Failed to mount ports");
    FsMount(SYS_DEVICES, L"devfs", NULL);
    assert(b || "Failed to mount devices");

    return true;
}
