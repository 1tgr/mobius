#ifndef __RAMDISK_H
#define __RAMDISK_H

#ifdef __cplusplus
extern "C"
{
#endif

/*#define RAMDISK_SIGNATURE_1	\
	((unsigned long) 'R' | \
	((unsigned long) 'D' << 8) | \
	((unsigned long) 'S' << 16) | \
	((unsigned long) 'K' << 24))
#define RAMDISK_SIGNATURE_2	\
	((unsigned long) 'K' | 
	((unsigned long) 'S' << 8) | 
	((unsigned long) 'D' << 16) | 
	((unsigned long) 'R' << 24))*/

#define RAMDISK_SIGNATURE_1		0x5244534BUL	/* RDSK */
#define RAMDISK_SIGNATURE_2		0x5244534BUL	/* KSDR */

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

#ifdef KERNEL
bool	ramInit();
void*	ramOpen(const wchar_t* name);
size_t	ramFileLength(const wchar_t* name);
bool	ramPageFault(addr_t virt);
#endif

#ifdef __cplusplus
}
#endif

#endif