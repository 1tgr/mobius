#ifndef __OS_FS_H
#define __OS_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/*!
 *	\defgroup	fs	File System
 *	\ingroup	kernelu
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

#ifndef KERNEL

struct request_t;

bool fsFullPath(const wchar_t* src, wchar_t* dst);
bool fsRequest(addr_t fd, struct request_t* req, size_t size);
addr_t fsOpen(const wchar_t* path);
bool fsClose(addr_t fd);
bool fsRead(addr_t fd, void* buffer, size_t* length);
bool fsWrite(addr_t fd, const void* buffer, size_t* length);

#endif

//@}

#ifdef __cplusplus
}
#endif

#endif