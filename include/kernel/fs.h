/* $Id: fs.h,v 1.5 2002/03/04 18:56:07 pavlovskii Exp $ */
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

typedef struct file_t file_t;
/*!
 *	\brief File object structure
 *
 *	All file objects created by file system drivers start with this header
 */
struct file_t
{
	/*! File system device that maintains this file */
	device_t *fsd;
	/*! Offset in the file for the next read or write operations */
	uint64_t pos;
	/*! Flags used to open this file */
	uint32_t flags;
};

handle_t	FsCreate (const wchar_t*, uint32_t);
handle_t	FsOpen (const wchar_t*, uint32_t);
bool	FsClose (handle_t);
bool	FsRead (handle_t, void*, size_t, struct fileop_t*);
bool	FsWrite (handle_t, const void*, size_t, struct fileop_t*);
off_t	FsSeek (handle_t, off_t, unsigned);

size_t	FsReadSync(handle_t file, void *buf, size_t bytes);
size_t	FsWriteSync(handle_t file, const void *buf, size_t bytes);
bool	FsMount(const wchar_t *path, const wchar_t *filesys, device_t *dev);
bool	FsCreateVirtualDir(const wchar_t *path);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif