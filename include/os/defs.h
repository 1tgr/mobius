/* $Id: defs.h,v 1.4 2002/02/24 19:13:11 pavlovskii Exp $ */
#ifndef __OS_DEFS_H
#define __OS_DEFS_H

#include <sys/types.h>

/*!
 *	\ingroup	libsys
 *	\defgroup	osdefs	Möbius Types and Definitions
 *	@{
 */

/*!	Path to the system boot directory (ramdisk) */
#define SYS_BOOT	L"/System/Boot"
/*!	Path to the system ports directory */
#define SYS_PORTS	L"/System/Ports"
/*!	Path to the system devices directory */
#define SYS_DEVICES	L"/System/Devices"

/*!	\brief	Maximum length of a path specification, in characters */
#define MAX_PATH	256

#ifndef PAGE_SIZE
/*!	\brief	Size of one page on the target architecture */
#define PAGE_SIZE	4096
#endif

#ifndef __KERNEL_MEMORY_H
/*!	\brief	Rounds an address down to a page boundary */
#define PAGE_ALIGN(addr)	((addr) & -PAGE_SIZE)
/*!	\brief	Rounds an address up to a page boundary */
#define PAGE_ALIGN_UP(addr)	(((addr) + PAGE_SIZE - 1) & -PAGE_SIZE)
#endif

/* Flags for FsCreate() and FsOpen() */
/*! Allows a file to be read from */
#define FILE_READ	1
/*! Allows a file to be written to */
#define FILE_WRITE	2

typedef struct process_info_t process_info_t;
/*!	\brief	User-mode process information structure */
struct process_info_t
{
	wchar_t cwd[256];
	unsigned id;
	addr_t base;
	handle_t std_in, std_out;
};

typedef struct thread_info_t thread_info_t;
/*!
 *	\brief	User-mode thead information structure
 *
 *	On the x86 the \p FS segment contains this structure.
 */
struct thread_info_t
{
	/*!
	 *	\brief DS-relative pointer to this structure
	 *
	 *	Use this pointer to avoid FS-relative memory accesses
	 */
	thread_info_t *info;
	/*!	\brief	ID of this thread */
	uint32_t id;
	/*!	\brief	Pointer to this process's information structure */
	process_info_t *process;
	/*! \brief	Result code from the last syscall */
	int status;
};

typedef struct fileop_t fileop_t;
/*!
 *	\brief	Contains status information for user-mode asynchronous I/O 
 *	operations
 */
struct fileop_t
{
	/*!
	 *	\brief	Result of the operation
	 *	Only valid just after calling \p FsRead or \p FsWrite and after
	 *	having called \p ThrWaitHandle.
	 *
	 *	Valid error codes are found in \p <errno.h>.
	 */
	status_t result;
	/*!
	 *	\brief	Handle of the event to be signalled upon I/O completion
	 */
	handle_t event;
	/*!
	 *	\brief	Number of bytes transferred after the operation completed
	 */
	size_t bytes;
};

#define FILE_ATTR_READ_ONLY 	0x01
#define FILE_ATTR_HIDDEN		0x02
#define FILE_ATTR_SYSTEM		0x04
#define FILE_ATTR_VOLUME_ID 	0x08
#define FILE_ATTR_DIRECTORY 	0x10
#define FILE_ATTR_ARCHIVE		0x20
#define FILE_ATTR_DEVICE		0x1000
#define FILE_ATTR_LINK			0x2000

typedef struct dirent_t dirent_t;
/*!
 *	\brief	Contains information on an entry in a directory
 */
struct dirent_t
{
	wchar_t name[256];
	uint64_t length;
	uint64_t standard_attributes;
};

/* Resources */
#define RT_CURSOR           1
#define RT_BITMAP           2
#define RT_ICON             3
#define RT_MENU             4
#define RT_DIALOG           5
#define RT_STRING           6
#define RT_FONTDIR          7
#define RT_FONT             8
#define RT_ACCELERATOR      9
#define RT_RCDATA           10
#define RT_MESSAGETABLE     11

/*! @} */

#endif