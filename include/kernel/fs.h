/* $Id: fs.h,v 1.10 2002/08/04 17:22:39 pavlovskii Exp $ */
#ifndef __KERNEL_FS_H
#define __KERNEL_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/* Normal FS functions are defined in <os/syscall.h> */
#include <os/syscall.h>

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

#ifdef __cplusplus

struct __attribute__((com_interface)) fsd_t
{
    virtual void dismount() = 0;
    virtual void get_fs_info(fs_info_t *info) = 0;

    virtual status_t create_file(const wchar_t *path, 
        fsd_t **redirect, void **cookie) = 0;
    virtual status_t lookup_file(const wchar_t *path, 
        fsd_t **redirect, void **cookie) = 0;
    virtual status_t get_file_info(void *cookie, uint32_t type, void *buf) = 0;
    virtual status_t set_file_info(void *cookie, uint32_t type, const void *buf) = 0;
    virtual void free_cookie(void *cookie) = 0;

    virtual bool read_file(file_t *file, page_array_t *pages, 
        size_t length, fs_asyncio_t *io) = 0;
    virtual bool write_file(file_t *file, page_array_t *pages, 
        size_t length, fs_asyncio_t *io) = 0;
    virtual bool ioctl_file(file_t *file, uint32_t code, void *buf, 
        size_t length, fs_asyncio_t *io) = 0;
    virtual bool passthrough(file_t *file, uint32_t code, void *buf, 
        size_t length, fs_asyncio_t *io) = 0;

    virtual status_t opendir(const wchar_t *path, fsd_t **redirect, void **dir_cookie) = 0;
    virtual status_t readdir(void *dir_cookie, dirent_t *buf) = 0;
    virtual void free_dir_cookie(void *dir_cookie) = 0;

    virtual status_t mount(const wchar_t *path, fsd_t *newfsd) = 0;

    virtual void finishio(request_t *req) = 0;
    virtual void flush_cache(file_t *fd) = 0;
};

#else

typedef struct fsd_t fsd_t;
struct fsd_t
{
    const struct fsd_vtbl_t *vtbl;
};

typedef struct fsd_vtbl_t fsd_vtbl_t;
struct fsd_vtbl_t
{
    void (*dismount)(fsd_t *fsd);
    void (*get_fs_info)(fsd_t *fsd, fs_info_t *info);

    status_t (*create_file)(fsd_t *fsd, const wchar_t *path, 
        fsd_t **redirect, void **cookie);
    status_t (*lookup_file)(fsd_t *fsd, const wchar_t *path, 
        fsd_t **redirect, void **cookie);
    status_t (*get_file_info)(fsd_t *fsd, void *cookie, uint32_t type, void *buf);
    status_t (*set_file_info)(fsd_t *fsd, void *cookie, uint32_t type, const void *buf);
    void (*free_cookie)(fsd_t *fsd, void *cookie);

    bool (*read_file)(fsd_t *fsd, file_t *file, page_array_t *pages, 
        size_t length, fs_asyncio_t *io);
    bool (*write_file)(fsd_t *fsd, file_t *file, page_array_t *pages, 
        size_t length, fs_asyncio_t *io);
    bool (*ioctl_file)(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
        size_t length, fs_asyncio_t *io);
    bool (*passthrough)(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
        size_t length, fs_asyncio_t *io);

    status_t (*opendir)(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **dir_cookie);
    status_t (*readdir)(fsd_t *fsd, void *dir_cookie, dirent_t *buf);
    void (*free_dir_cookie)(fsd_t *fsd, void *dir_cookie);

    status_t (*mount)(fsd_t *fsd, const wchar_t *path, fsd_t *newfsd);
    void (*finishio)(fsd_t *fsd, request_t *req);
    void (*flush_cache)(fsd_t *fsd, file_t *fd);
};

#endif

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
bool	FsCreateVirtualDir(const wchar_t *path);

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