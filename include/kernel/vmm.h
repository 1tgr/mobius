/* $Id: vmm.h,v 1.9 2002/09/08 20:47:03 pavlovskii Exp $ */
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

/*efine VM_AREA_EMPTY           0*/
#define VM_AREA_NORMAL	        1
#define VM_AREA_MAP		2
/*efine VM_AREA_SHARED	        3*/
#define VM_AREA_FILE	        4
#define VM_AREA_IMAGE           5
#define VM_AREA_CALLBACK        6

#define MEM_READ		1
#define MEM_WRITE		2
#define MEM_ZERO		4
#define MEM_COMMIT		8
#define MEM_LITERAL		16

struct page_array_t;

/*! \brief Describes an area of memory allocated within the address space of a 
 *	particular process. */
/*struct vm_area_t
{
    handle_hdr_t hdr;

    vm_area_t *prev, *next;
    vm_area_t *shared_prev, *shared_next;
    process_t *owner;

    addr_t start;
    unsigned num_pages;
    uint32_t flags;
    spinlock_t mtx_allocate;
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

        struct
        {
            bool (*handler)(void *, addr_t, bool);
            void *cookie;
        } callback;
    } dest;
};*/

typedef struct vm_node_t vm_node_t;
typedef bool (*VMM_CALLBACK)(void *cookie, vm_node_t *node, addr_t start, 
                             bool is_writing);

typedef struct vm_desc_t vm_desc_t;
struct vm_desc_t
{
    handle_hdr_t hdr;

    unsigned num_pages;
    vm_desc_t *shared_prev, *shared_next;
    spinlock_t mtx_allocate;
    spinlock_t spin;
    fileop_t pagingop;
    struct page_array_t *pages, *read_pages;
    wchar_t *name;

    unsigned type;

    union
    {
        addr_t phys_map;
        vm_desc_t* shared_from;
        handle_t file;
        module_t *mod;

        struct
        {
            VMM_CALLBACK handler;
            void *cookie;
        } callback;
    } dest;
};

struct vm_node_t
{
    vm_node_t *left, *right;
    addr_t base;
    uint32_t flags;

    union
    {
        vm_desc_t *desc;
        unsigned empty_pages;
    } u;
};

#define VMM_NODE_IS_EMPTY(n)    ((n)->flags == 0)

/* vmmAlloc flags are in os/os.h */

void*       VmmMap(size_t pages, addr_t start, void *dest1, void *dest2, 
                   unsigned type, uint32_t flags);
void*       VmmAlloc(size_t pages, addr_t start, uint32_t flags);
bool        VmmShare(void *base, const wchar_t *name);
void*       VmmAllocCallback(size_t pages, addr_t start, uint32_t flags, 
                             VMM_CALLBACK handler,
                             void *cookie);

void        VmmFree(const void *ptr);
bool        VmmPageFault(process_t *proc, addr_t start, bool is_writing);
vm_node_t*  VmmLookupNode(vm_node_t *parent, const void* ptr);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif