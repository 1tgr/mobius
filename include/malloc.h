#ifndef __MALLOC_H
#define __MALLOC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
size_t msize(void* ptr);

union _BlockHeader
{
	struct
	{
		union _BlockHeader *ptr;
		size_t size;
	} s;
	long x;
};

typedef union _BlockHeader BlockHeader;

#ifdef __cplusplus
}
#endif

#endif