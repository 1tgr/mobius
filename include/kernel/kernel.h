/* $Id: kernel.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */
#ifndef __KERNEL_KERNEL_H
#define __KERNEL_KERNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
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
    struct multiboot_info *multiboot_info;
    uint32_t kernel_data;
    uint32_t num_cpus;
	uint32_t debug_port;
	uint32_t debug_speed;
	wchar_t cmdline[256];
};

struct thread_t;

typedef struct spinlock_t spinlock_t;
struct spinlock_t
{
    uint32_t locks;
    struct thread_t *owner;
    uint32_t old_psw;
};

extern kernel_startup_t kernel_startup;

void	SpinInit(spinlock_t *sem);
void	SpinAcquire(spinlock_t *sem);
void	SpinRelease(spinlock_t *sem);
void	MtxAcquire(spinlock_t *sem);
void	MtxRelease(spinlock_t  *sem);

#define ___S(a, b) a##b
#define __S(a, b) ___S(a, b)
#define CASSERT(exp)    extern char __S(__ERR, __LINE__)[(exp) ? 0 : -1]

#define PHYSICAL(addr)  ((void*) ((char*) ADDR_TEMP_VIRT + (addr_t) (addr)))

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

#define ENCLOSING_STRUCT(ptr, struc, field) ((struc*) ((char*) (ptr) - (char*) &((struc*) 0)->field))

unsigned KeAtomicInc(unsigned *n);
unsigned KeAtomicDec(unsigned *n);
void *KeGetSysCall(unsigned id);
void *KeSetSysCall(unsigned id, void *ptr);

typedef struct systab_t systab_t;
struct systab_t
{
    uint32_t code;
    const char *name;
    void *routine;
    uint32_t argbytes;
};

void KeInstallSysCallGroup(uint8_t prefix, systab_t **group, unsigned count);

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
