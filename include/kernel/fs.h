/* $Id: fs.h,v 1.3 2002/01/05 21:37:45 pavlovskii Exp $ */
#ifndef __KERNEL_FS_H
#define __KERNEL_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/driver.h>
#include <kernel/handle.h>

typedef struct file_t file_t;
struct file_t
{
	device_t *fsd;
	uint64_t pos;
	uint32_t flags;
};

bool	FsInit(void);
handle_t
	FsCreate(const wchar_t *path, uint32_t flags);
handle_t
	FsOpen(const wchar_t *path, uint32_t flags);
bool	FsClose(handle_t file);
size_t	FsRead(handle_t file, void *buf, size_t bytes);
size_t	FsWrite(handle_t file, const void *buf, size_t bytes);
addr_t	FsSeek(handle_t file, addr_t ofs);
bool	FsRequestSync(handle_t file, request_t *req);
bool	FsMount(const wchar_t *path, const wchar_t *filesys, device_t *dev);
bool	FsCreateVirtualDir(const wchar_t *path);

#ifdef __cplusplus
}
#endif

#endif