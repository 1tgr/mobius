#ifndef __IO_H
#define __IO_H

#ifdef __cplusplus
extern "C"
{
#endif

int _open(const char *filename, int oflag, ...);
int _read(int handle, void *buffer, unsigned int count);
int _close(int handle);

#define access(p, m)	1

#define R_OK	0

#ifdef __cplusplus
}
#endif

#endif