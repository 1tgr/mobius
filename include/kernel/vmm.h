/* $Id: vmm.h,v 1.6 2002/08/04 17:22:39 pavlovskii Exp $ */
#ifndef __KERNEL_VMM_H
#define __KERNEL_VMM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>
#include <kernel/fs.h>

/*!
 *	\ingroup	kernel
 *	\defgroup	vmm	Virtual Memory
 *	@{
 */
typedef struct vm_area_t vm_area_t;

#define VM_AREA_EMPTY           0
#define VM_AREA_NORMAL	        1
#define VM_AREA_MAP		2
#define VM_AREA_SHARED	        3
#define VM_AREA_FILE	        4
#define VM_AREA_IMAGE           5

#define MEM_READ		1
#define MEM_WRITE		2
#define MEM_ZERO		4
#define MEM_COMMIT		8
#define MEM_LITERAL		16

struct page_array_t;

/*! \brief Describes an area of memory allocated within the address space of a 
 *	particular process. */
struct vm_area_t
{
    vm_area_t *prev, *next;
    vm_area_t *shared_prev, *shared_next;
    process_t *owner;

    addr_t start;
    unsigned num_pages;
    uint32_t flags;
    semaphore_t mtx_allocate;
    fileop_t pagingop;
    struct page_array_t *pages, *read_pages;
    wchar_t *name;
    
    unsigned type;

    union
    {
	addr_t phys_map;
	vm_area_t* shared_from;
	handle_t file;
        module_t *mod;
    } dest;
};

/* vmmAlloc flags are in os/os.h */

/* create a vm_area_t */
void*       VmmMap(size_t pages, addr_t virt, void *dest, unsigned type, 
                   uint32_t flags);

void*       VmmAlloc(size_t pages, addr_t start, uint32_t flags);
bool        VmmShare(void *base, const wchar_t *name);

void        VmmFree(vm_area_t* area);
bool        VmmPageFault(vm_area_t* area, addr_t start, bool is_writing);
void        VmmUncommit(vm_area_t* area);
void        VmmInvalidate(vm_area_t* area, addr_t start, size_t pages);
vm_area_t*  VmmArea(process_t* proc, const void* ptr);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif