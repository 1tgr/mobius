/* $Id: defs.h,v 1.8 2002/04/04 00:08:42 pavlovskii Exp $ */
#ifndef __OS_DEFS_H
#define __OS_DEFS_H

#include <sys/types.h>

/*!
 *    \ingroup	  libsys
 *    \defgroup    osdefs    Möbius Types and Definitions
 *    @{
 */

/*!    Path to the system boot directory (ramdisk) */
#define SYS_BOOT    L"/System/Boot"
/*!    Path to the system ports directory */
#define SYS_PORTS    L"/System/Ports"
/*!    Path to the system devices directory */
#define SYS_DEVICES    L"/System/Devices"

/*!    \brief	 Maximum length of a path specification, in characters */
#define MAX_PATH    256

#ifndef PAGE_SIZE
/*!    \brief	 Size of one page on the target architecture */
#define PAGE_SIZE    4096
#endif

#ifndef __KERNEL_MEMORY_H
/*!    \brief	 Rounds an address down to a page boundary */
#define PAGE_ALIGN(addr)    ((addr) & -PAGE_SIZE)
/*!    \brief	 Rounds an address up to a page boundary */
#define PAGE_ALIGN_UP(addr)    (((addr) + PAGE_SIZE - 1) & -PAGE_SIZE)
#endif

#ifndef __KERNEL_VMM_H
#define MEM_READ	1
#define MEM_WRITE	 2
#define MEM_ZERO	4
#define MEM_COMMIT	  8
#define MEM_LITERAL	   16
#endif

/* Flags for FsCreate() and FsOpen() */
/*! Allows a file to be read from */
#define FILE_READ    1
/*! Allows a file to be written to */
#define FILE_WRITE    2

/* Flags for FsQueryFile() */
/*! Tests for the file's presence; does not return any other information */
#define FILE_QUERY_NONE        0
/*! Returns \p dirent_t information for the file */
#define FILE_QUERY_STANDARD    1
/*! Returns \p dirent_device_t information for the device */
#define FILE_QUERY_DEVICE    2

/* Values for seek origin */
#define FILE_SEEK_SET	 0
#define FILE_SEEK_CUR	 1
#define FILE_SEEK_END	 2

typedef struct process_info_t process_info_t;
/*!    \brief	 User-mode process information structure */
struct process_info_t
{
    unsigned id;
    addr_t base;
    handle_t std_in, std_out;
    wchar_t cwd[256];
    wchar_t cmdline[256];
};

typedef struct thread_info_t thread_info_t;
/*!
 *    \brief	User-mode thead information structure
 *
 *    On the x86 the \p FS segment contains this structure.
 */
struct thread_info_t
{
    /*!
     *	  \brief DS-relative pointer to this structure
     *
     *	  Use this pointer to avoid FS-relative memory accesses
     */
    thread_info_t *info;
    /*!    \brief    ID of this thread */
    uint32_t id;
    /*!    \brief    Pointer to this process's information structure */
    process_info_t *process;
    /*! \brief	  Result code from the last syscall */
    int status;
    /*! \brief    Parameter passed to \p ThrCreateThread */
    void *param;
    handle_t msgqueue_event;
};

typedef struct fileop_t fileop_t;
/*!
 *    \brief	Contains status information for user-mode asynchronous I/O 
 *    operations
 */
struct fileop_t
{
    /*!
     *	  \brief    Result of the operation
     *	  Only valid just after calling \p FsRead or \p FsWrite and after
     *	  having called \p ThrWaitHandle.
     *
     *	  Valid error codes are found in \p <errno.h>.
     */
    status_t result;
    /*!
     *	  \brief    Handle of the event to be signalled upon I/O completion
     */
    handle_t event;
    /*!
     *	  \brief    Number of bytes transferred after the operation completed
     */
    size_t bytes;
};

#define FILE_ATTR_READ_ONLY	0x01
#define FILE_ATTR_HIDDEN	0x02
#define FILE_ATTR_SYSTEM	0x04
#define FILE_ATTR_VOLUME_ID	0x08
#define FILE_ATTR_DIRECTORY	0x10
#define FILE_ATTR_ARCHIVE	 0x20
#define FILE_ATTR_DEVICE	0x1000
#define FILE_ATTR_LINK		  0x2000

typedef struct dirent_t dirent_t;
/*! \brief    Contains information on an entry in a directory */
struct dirent_t
{
    wchar_t name[256];
    uint64_t length;
    uint64_t standard_attributes;
};

typedef struct dirent_device_t dirent_device_t;
struct dirent_device_t
{
    wchar_t description[256];
    uint32_t device_class;
};

/* Resources */
#define RT_CURSOR	    1
#define RT_BITMAP	    2
#define RT_ICON 	    3
#define RT_MENU 	    4
#define RT_DIALOG	    5
#define RT_STRING	    6
#define RT_FONTDIR	    7
#define RT_FONT 	    8
#define RT_ACCELERATOR	    9
#define RT_RCDATA	    10
#define RT_MESSAGETABLE     11

typedef struct sysinfo_t sysinfo_t;
/*! \brief    Information structure for \p SysGetInfo() */
struct sysinfo_t
{
    size_t page_size;
    size_t pages_total;
    size_t pages_free;
    size_t pages_physical;
    size_t pages_kernel;
};

typedef struct systimes_t systimes_t;
/*! \brief    Information structure for \p SysGetTimes() */
struct systimes_t
{
    unsigned quantum;
    unsigned uptime;
    unsigned current_cputime;
};

#ifdef i386
typedef uint32_t FARPTR;

#define MK_FP(seg, off)    ((FARPTR) (((uint32_t) (seg) << 16) | (uint16_t) (off)))
#define FP_SEG(fp)	  (((FARPTR) fp) >> 16)
#define FP_OFF(fp)	  (((FARPTR) fp) & 0xffff)
#define FP_TO_LINEAR(seg, off)	  ((void*) ((((uint16_t) (seg)) << 4) + ((uint16_t) (off))))

#ifndef __CONTEXT_DEFINED
#define __CONTEXT_DEFINED
typedef struct pusha_t pusha_t;
struct pusha_t
{
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

/*! \brief The layout of the stack frame generated by the assembly-language 
 *	  interrupt handler.
 *
 *    The isr_and_switch routine (in start.asm), along with the individual 
 *	  interrupt handlers and the processor, build this structure on the stack
 *	  before isr() is called.
 */
typedef struct context_t context_t;
struct context_t
{
    uint32_t kernel_esp;
    context_t *ctx_prev;
    pusha_t regs;
    uint32_t gs, fs, es, ds;
    uint32_t intr, error;
    uint32_t eip, cs, eflags, esp, ss;
};

typedef struct context_v86_t context_v86_t;
struct context_v86_t
{
    uint32_t kernel_esp;
    context_t *ctx_prev;
    pusha_t regs;
    uint32_t gs, fs, es, ds;
    uint32_t intr, error;
    uint32_t eip, cs, eflags, esp, ss;

    /* extra fields for V86 mode */
    uint32_t v86_es, v86_ds, v86_fs, v86_gs;
};
#endif

#endif

/* \brief Flags for \p SysShutdown */

#define SHUTDOWN_HALT       0
#define SHUTDOWN_REBOOT     1
#define SHUTDOWN_POWEROFF   2
#define SHUTDOWN_FLUSH      3

/*! @} */

#endif