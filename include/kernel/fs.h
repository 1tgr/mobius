#ifndef __KERNEL_FS_H
#define __KERNEL_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/driver.h>

typedef struct file_t file_t;
struct file_t
{
	device_t *fsd;
	qword pos;
};

file_t*	fsOpen(const wchar_t* path);
bool	fsClose(file_t* fd);
size_t	fsRead(file_t* fd, void* buffer, size_t length);
void	fsSeek(file_t *fd, qword pos);
bool	fsMount(const wchar_t* name, const wchar_t* fsd, device_t* device);
qword	fsGetLength(file_t* fd);

#ifdef __cplusplus
}
#endif

#endif