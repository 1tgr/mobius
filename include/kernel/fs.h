/* $Id: fs.h,v 1.12 2002/08/29 14:03:47 pavlovskii Exp $ */
#ifndef __KERNEL_FS_H
#define __KERNEL_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/* Normal FS functions are defined in <os/syscall.h> */
#include <os/syscall.h>
#include <os/comdef.h>

#include <kernel/driver.h>
#include <kernel/handle.h>

/*!
 *	\ingroup	kernel
 *	\defgroup	fs	File System
 *	@{
 */

struct cache_t;
struct fsd_t;
struct thread_t;
struct process_t;

typedef struct file_t file_t;
/*!
 *	\brief File object structure
 *
 *	All file objects created by file system drivers start with this header
 */
struct file_t
{
    /*! File system device that maintains this file */
    struct fsd_t *fsd;
    /*! Offset in the file for the next read or write operations */
    uint64_t pos;
    /*! Flags used to open this file */
    uint32_t flags;
    /*wchar_t *name;*/
    struct cache_t *cache;
    void *fsd_cookie;
};

#define FS_INFO_CACHE_BLOCK_SIZE    1
#define FS_INFO_SPACE_TOTAL         2
#define FS_INFO_SPACE_FREE          4

typedef struct fs_info_t fs_info_t;
struct fs_info_t
{
    uint32_t flags;
    uint64_t cache_block_size;
    uint64_t space_total;
    uint64_t space_free;
};

typedef struct fs_asyncio_t fs_asyncio_t;
struct fs_asyncio_t
{
    fileop_t *original;
    fileop_t op;
    struct thread_t *owner;
    file_t *file;
    handle_t file_handle;
};

typedef addr_t vnode_id_t;

typedef struct vnode_t vnode_t;
struct vnode_t
{
    struct fsd_t *fsd;
    vnode_id_t id;
};

#define VNODE_NONE  0
#define VNODE_ROOT  1

#undef __interface_name
#define __interface_name        fsd_t
INTERFACE(fsd_t)
{
    METHOD(void, dismount)(THIS) PURE;
    METHOD(void, get_fs_info)(THIS_ fs_info_t *info) PURE;

    METHOD(status_t, parse_element)(THIS_ const wchar_t *name, 
        wchar_t **new_path, vnode_t *node) PURE;
    METHOD(status_t, create_file)(THIS_ vnode_id_t dir, const wchar_t *name, 
        void **cookie) PURE;
    METHOD(status_t, lookup_file)(THIS_ vnode_id_t node, void **cookie) PURE;
    METHOD(status_t, get_file_info)(THIS_ void *cookie, uint32_t type, 
        void *buf) PURE;
    METHOD(status_t, set_file_info)(THIS_ void *cookie, uint32_t type, 
        const void *buf) PURE;
    METHOD(void, free_cookie)(THIS_ void *cookie) PURE;

    METHOD(bool, read_file)(THIS_ file_t *file, page_array_t *pages, 
        size_t length, fs_asyncio_t *io) PURE;
    METHOD(bool, write_file)(THIS_ file_t *file, page_array_t *pages, 
        size_t length, fs_asyncio_t *io) PURE;
    METHOD(bool, ioctl_file)(THIS_ file_t *file, uint32_t code, void *buf, 
        size_t length, fs_asyncio_t *io) PURE;
    METHOD(bool, passthrough)(THIS_ file_t *file, uint32_t code, void *buf, 
        size_t length, fs_asyncio_t *io) PURE;

    METHOD(status_t, mkdir)(THIS_ vnode_id_t dir, const wchar_t *name, 
        void **dir_cookie) PURE;
    METHOD(status_t, opendir)(THIS_ vnode_id_t node, void **dir_cookie) PURE;
    METHOD(status_t, readdir)(THIS_ void *dir_cookie, dirent_t *buf) PURE;
    METHOD(void, free_dir_cookie)(THIS_ void *dir_cookie) PURE;

    METHOD(void, finishio)(THIS_ request_t *req) PURE;
    METHOD(void, flush_cache)(THIS_ file_t *fd) PURE;
};

typedef union dirent_all_t dirent_all_t;
union dirent_all_t
{
    dirent_t dirent;
    dirent_standard_t standard;
    dirent_device_t device;
};

handle_t    FsCreate(const wchar_t*, uint32_t);
handle_t    FsOpen(const wchar_t*, uint32_t);
bool    FsClose(handle_t);
bool    FsRead(handle_t, void*, size_t, struct fileop_t*);
bool    FsWrite(handle_t, const void*, size_t, struct fileop_t*);
off_t   FsSeek(handle_t, off_t, unsigned);

bool    FsReadSync(handle_t file, void *buf, size_t bytes, size_t *bytes_read);
bool    FsWriteSync(handle_t file, const void *buf, size_t bytes, 
                    size_t *bytes_written);
bool    FsReadPhysical(handle_t file, page_array_t *pages, size_t bytes, 
                       struct fileop_t *op);
bool    FsWritePhysical(handle_t file, page_array_t *pages, size_t bytes, 
                        struct fileop_t *op);
bool	FsMount(const wchar_t *path, const wchar_t *filesys, const wchar_t *dest);
//bool	FsCreateVirtualDir(const wchar_t *path);

handle_t FsCreateFileHandle(struct process_t *proc, fsd_t *fsd, void *fsd_cookie, 
                            const wchar_t *name, uint32_t flags);
void    FsNotifyCompletion(fs_asyncio_t *io, size_t bytes, status_t result);
bool    FsGuessMimeType(const wchar_t *ext, wchar_t *mimetype, size_t length);
addr_t  RdGetFilePhysicalAddress(const wchar_t *name);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif