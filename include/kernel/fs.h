/* $Id: fs.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */
#ifndef __KERNEL_FS_H
#define __KERNEL_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
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

typedef addr_t vnode_id_t;

typedef struct vnode_t vnode_t;
struct vnode_t
{
    struct fsd_t *fsd;
    vnode_id_t id;
};

typedef struct file_t file_t;
/*!
 *	\brief File object structure
 *
 *	All file objects created by file system drivers start with this header
 */
struct file_t
{
    union
    {
        struct
        {
            file_t *hash_next;
            unsigned refs;
        } open;

        struct
        {
            file_t *prev, *next;
        } closing;
    } u;

    vnode_t vnode;
    struct cache_t *cache;
    void *fsd_cookie;
};

typedef struct file_dir_t file_dir_t;
struct file_dir_t
{
    handle_hdr_t hdr;
    struct fsd_t *fsd;
    void *fsd_cookie;
};

typedef struct file_handle_t file_handle_t;
struct file_handle_t
{
    handle_hdr_t hdr;
    uint32_t flags;
    file_t *file;
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

typedef struct fs_request_t fs_request_t;
struct fs_request_t
{
    file_t *file;
    page_array_t *pages;
    uint64_t pos;
    size_t length;
    fs_asyncio_t *io;
    bool is_reading;
};

struct fs_asyncio_t
{
    fileop_t *original;
    fileop_t op;
    struct thread_t *owner;
    fs_request_t request;
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
    METHOD(status_t, lookup_file)(THIS_ vnode_id_t node, uint32_t open_flags, 
        void **cookie) PURE;
    METHOD(status_t, get_file_info)(THIS_ void *cookie, uint32_t type, 
        void *buf) PURE;
    METHOD(status_t, set_file_info)(THIS_ void *cookie, uint32_t type, 
        const void *buf) PURE;
    METHOD(void, free_cookie)(THIS_ void *cookie) PURE;

    METHOD(status_t, read_write_file)(THIS_ const fs_request_t *req, size_t *bytes) PURE;
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

typedef struct pipe_t pipe_t;

status_t FsPathToVnode(const vnode_t *start, const wchar_t *path, 
					   vnode_t *vn, bool do_create, const wchar_t **out);
file_handle_t *FsCreate(const vnode_t *start, const wchar_t*, uint32_t);
file_handle_t *FsOpen(const vnode_t *start, const wchar_t*, uint32_t);
status_t FsReadAsync(file_handle_t *fh, void*, uint64_t, size_t, struct fileop_t*);
status_t FsWriteAsync(file_handle_t *fh, const void*, uint64_t, size_t, struct fileop_t*);

bool    FsRead(file_handle_t *fh, void *buf, uint64_t offset, size_t bytes, size_t *bytes_read);
bool    FsWrite(file_handle_t *fh, const void *buf, uint64_t offset, size_t bytes, size_t *bytes_written);
status_t FsReadPhysicalAsync(file_handle_t *fh, page_array_t *pages, uint64_t offset, size_t bytes, struct fileop_t *op);
status_t FsWritePhysicalAsync(file_handle_t *fh, page_array_t *pages, uint64_t offset, size_t bytes, struct fileop_t *op);

file_t *    FsReferenceFile(file_t *file);
void    FsReleaseFile(file_t *fd);
file_t *    FsFindOpenFile(const vnode_t *node);
file_t *    FsCreateFileObject(fsd_t *fsd, vnode_id_t id, void *fsd_cookie);
file_handle_t *FsCreateFileHandle(file_t *file, uint32_t flags);
void    FsNotifyCompletion(fs_asyncio_t *io, size_t bytes, status_t result);
bool    FsGuessMimeType(const wchar_t *ext, wchar_t *mimetype, size_t length);
addr_t  RdGetFilePhysicalAddress(const wchar_t *name, size_t *length);
bool    FsCreatePipeInternal(pipe_t **pipes);
bool    FsLookupMountPoint(vnode_t *out, const vnode_t *in);

file_dir_t *FsOpenDir(const wchar_t *name);
bool    FsQueryFile(const wchar_t *name, uint32_t code, void *buffer, size_t size);
bool    FsRequest(file_handle_t *fh, uint32_t code, void *buffer, size_t bytes, struct fileop_t* op);
bool    FsIoCtl(file_handle_t *fh, uint32_t, void *, size_t, struct fileop_t*);
bool    FsReadDir(file_dir_t *fh, struct dirent_t *, size_t);
bool    FsQueryHandle(file_handle_t *fh, uint32_t, void *, size_t);
bool    FsCreateDir(const wchar_t*);
bool    FsChangeDir(const wchar_t*);
bool    FsMount(const wchar_t *, const wchar_t *, const wchar_t *);
bool    FsDismount(const wchar_t *);
bool    FsCreatePipe(file_handle_t **);
bool	FsFullPath(const wchar_t* src, wchar_t* dst);

extern vnode_t fs_root;

/*! @} */

#ifdef __cplusplus
}
#endif

#endif
