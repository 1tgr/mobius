/* $Id: vmm.h,v 1.2 2003/06/22 15:50:13 pavlovskii Exp $ */
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

#define VM_AREA_NORMAL	        1
#define VM_AREA_MAP		2
#define VM_AREA_FILE	        3
#define VM_AREA_IMAGE           4
#define VM_AREA_CALLBACK        5

/*! \brief Allow access only from kernel mode */
#define VM_MEM_KERNEL           0x00000000
/*! \brief Allow access from user mode and kernel mode */
#define VM_MEM_USER             0x00000003
/*! \brief Mask for privilege level bits */
#define VM_MEM_PL_MASK          0x00000003
/*! \brief Allow memory to be read */
#define VM_MEM_READ		0x00000004
/*! \brief Allow memory to be written */
#define VM_MEM_WRITE		0x00000008
/*! \brief Zero memory before it is used */
#define VM_MEM_ZERO		0x00000010
/*! \brief Disable write caching on memory */
#define VM_MEM_CACHE_WT         0x00000020
/*! \brief Disable all caching on memory */
#define VM_MEM_CACHE_NONE       0x00000040
/*! \brief Allow mapping of VM_AREA_MAP areas to address 0 */
#define VM_MEM_LITERAL		0x00000080
/*! \brief Reserve a region of address space (nodes only) */
#define VM_MEM_RESERVED         0x00000100

/*! \brief Flags that are valid for nodes (as opposed to areas) */
#define VM_MEM_NODE_MASK       (VM_MEM_PL_MASK | \
                                VM_MEM_READ | \
                                VM_MEM_WRITE | \
                                VM_MEM_CACHE_WT | \
                                VM_MEM_CACHE_NONE | \
                                VM_MEM_RESERVED)

struct page_array_t;

typedef struct vm_node_t vm_node_t;
typedef bool (*VMM_CALLBACK)(void *cookie, vm_node_t *node, addr_t start, 
                             bool is_writing);

typedef struct vm_desc_t vm_desc_t;
/*!
 *  \brief  Represents an area of memory, possibly shared between more than 
 *      one process
 */
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
    uint32_t flags;

    unsigned type;

    union
    {
        addr_t phys_map;
        vm_desc_t* shared_from;
        file_handle_t *file;
        module_t *mod;

        struct
        {
            VMM_CALLBACK handler;
            void *cookie;
        } callback;
    } dest;
};

/*!
 *  \brief  Represents an area of memory in one process
 */
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
void*       VmmAllocCallback(size_t pages, addr_t start, uint32_t flags, 
                             VMM_CALLBACK handler,
                             void *cookie);

void        VmmFreeNode(vm_node_t *node);
bool        VmmPageFault(process_t *proc, addr_t start, bool is_writing);
vm_node_t*  VmmLookupNode(int level, vm_node_t *parent, const void* ptr);

void *      VmmAlloc(size_t pages, addr_t base, uint32_t flags);
bool        VmmFree(void *base);
void *      VmmMapSharedArea(vm_desc_t *desc, addr_t base, uint32_t flags);
void *      VmmMapFile(file_handle_t *fh, addr_t base, size_t pages, uint32_t flags);
void *      VmmReserveArea(size_t pages, addr_t base);
vm_desc_t * VmmShare(void *base, const wchar_t *name);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif
