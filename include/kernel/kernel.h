/* $Id: kernel.h,v 1.8 2002/08/14 16:30:53 pavlovskii Exp $ */
#ifndef __KERNEL_KERNEL_H
#define __KERNEL_KERNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <os/defs.h>

/*!
 *	\defgroup	kernel
 *	@{
 */

typedef struct kernel_startup_t kernel_startup_t;
struct kernel_startup_t
{
    uint32_t memory_size;
    uint32_t kernel_size;
    uint32_t kernel_phys;
    /*uint32_t ramdisk_size;
    uint32_t ramdisk_phys;*/
    struct multiboot_info *multiboot_info;
    uint32_t kernel_data;
    uint32_t num_cpus;
};

struct thread_t;

typedef struct semaphore_t semaphore_t;
struct semaphore_t
{
    uint32_t locks;
    struct thread_t *owner;
    uint32_t old_psw;
};

extern kernel_startup_t kernel_startup;

void	SemInit(semaphore_t *sem);
void	SemAcquire(semaphore_t *sem);
void	SemRelease(semaphore_t *sem);
void	MtxAcquire(semaphore_t *sem);
void	MtxRelease(semaphore_t *sem);
int     _cputs(const char *str, size_t count);
int     _cputws(const wchar_t *str, size_t count);

#define ___CASSERT(a, b) a##b
#define __CASSERT(a, b) ___CASSERT(a, b)
#define CASSERT(exp)    extern char __CASSERT(__ERR, __LINE__)[(exp) ? 0 : -1]
#define PHYSICAL(addr)  ((void*) ((char*) PHYSMEM + (addr_t) (addr)))

#define LIST_ADD(list, item) \
    if (list##_last != NULL) \
        list##_last->next = item; \
    item->prev = list##_last; \
    item->next = NULL; \
    list##_last = item; \
    if (list##_first == NULL) \
            list##_first = item;

#define LIST_REMOVE(list, item) \
    if (item->next != NULL) \
        item->next->prev = item->prev; \
    if (item->prev != NULL) \
        item->prev->next = item->next; \
    if (list##_first == item) \
        list##_first = item->next; \
    if (list##_last == item) \
        list##_last = item->prev; \
    item->next = item->prev = NULL;

#define FOREACH(item, list) \
    for (item = list##_first; item != NULL; item = item->next)

typedef struct kernel_hooks_t kernel_hooks_t;
struct kernel_hooks_t
{
    uint64_t mask;
    int (*cputs)(const char *, size_t);
    int (*cputws)(const wchar_t *, size_t);
};

#define KERNEL_HOOK_CPUTS   0x01
#define KERNEL_HOOK_CPUTWS  0x02

void KeInstallHooks(const kernel_hooks_t *hooks);
void KeAtomicInc(unsigned *n);
void KeAtomicDec(unsigned *n);
void *KeGetSysCall(unsigned id);
void *KeSetSysCall(unsigned id, void *ptr);

void *sbrk_virtual(size_t diff);

#ifdef __SMP__
#define MAX_CPU     4
#else
#define MAX_CPU     1
#endif

#ifdef _MSC_VER
#define __initcode
#define __initdata
#define KeYield()
#else
#define __initcode __attribute__((section (".dtext")))
#define __initdata __attribute__((section (".ddata")))
#define KeYield() __asm__("int $0x30" : : "a" (0x108), "c" (0), "d" (0))
#endif

/*! @} */

#ifdef __cplusplus
}
#endif

#endif