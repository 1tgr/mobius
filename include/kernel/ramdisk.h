/* $Id: ramdisk.h,v 1.1.1.1 2002/12/31 01:26:21 pavlovskii Exp $ */
#ifndef __KERNEL_RAMDISK_H
#define __KERNEL_RAMDISK_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *	\ingroup	kernel
 *	\defgroup	rd	Ramdisk
 *	@{
 */

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
	uint32_t signature;
	uint32_t num_files;
};

typedef struct ramfile_t ramfile_t;
struct ramfile_t
{
	uint32_t offset;
	uint32_t length;
	char name[16];
};

#ifdef KERNEL
/*void*	RdOpen(const wchar_t* name);
size_t	RdFileLength(const wchar_t* name);
bool	RdPageFault(addr_t virt);*/
#endif

/*! @} */

#ifdef __cplusplus
}
#endif

#endif