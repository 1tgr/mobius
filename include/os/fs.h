#ifndef __OS_FS_H
#define __OS_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/*!
 *	\ingroup	kernelu
 *	\defgroup	fs	File System
 *	@{
 */

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_DEVICE         0x1000
#define ATTR_LINK			0x2000

typedef struct dir_entry_t dir_entry_t;
struct dir_entry_t
{
	uint32_t attribs;
	uint64_t length;
	wchar_t name[260];
};

/*#ifndef KERNEL

struct request_t;

bool	FsFullPath(const wchar_t* src, wchar_t* dst);
bool	FsRequest(marshal_t file, struct request_t* req, size_t size);
handle_t	FsOpen(const wchar_t* path);
bool	FsClose(marshal_t file);
bool	FsRead(marshal_t file, void* buffer, size_t* length);
bool	FsWrite(marshal_t file, const void* buffer, size_t* length);
bool	FsIoCtl(marshal_t file, dword code, void* params, size_t length);
bool	FsSeek(marshal_t file, qword pos);
uint64_t	FsGetPosition(handle_t file);
uint64_t	FsGetLength(handle_t file);

#endif*/
#include <os/syscall.h>

/*@}*/

#ifdef __cplusplus
}
#endif

#endif