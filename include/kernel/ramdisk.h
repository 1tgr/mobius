#ifndef __RAMDISK_H
#define __RAMDISK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/memory.h>

typedef struct ramdisk_t ramdisk_t;
struct ramdisk_t
{
	dword signature;
	dword num_files;
};

typedef struct ramfile_t ramfile_t;
struct ramfile_t
{
	dword offset;
	dword length;
	char name[16];
};

bool ramInit();
void* ramOpen(const wchar_t* name);
size_t ramFileLength(const wchar_t* name);
bool ramPageFault(addr_t virt);

#ifdef __cplusplus
}
#endif

#endif