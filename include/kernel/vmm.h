#ifndef __KERNEL_VMM_H
#define __KERNEL_VMM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>

typedef struct vm_area_t vm_area_t;

#define VM_AREA_NORMAL	0
#define VM_AREA_MAP		1
#define VM_AREA_SHARED	2
#define VM_AREA_FILE	3

#define MEM_READ		1
#define MEM_WRITE		2
#define MEM_ZERO		4
#define MEM_COMMIT		8
#define MEM_LITERAL		16

/*! \brief Describes an area of memory allocated within the address space of a 
 *	particular process. */
struct vm_area_t
{
	vm_area_t *prev, *next;
	process_t *owner;

	void* start;
	size_t pages;
	uint32_t flags;
	
	unsigned type;

	union
	{
		addr_t phys_map;
		vm_area_t* shared_from;
		handle_t file;
	} dest;
};

/* vmmAlloc flags are in os/os.h */

/* create a vm_area_t */
void*		VmmMap(size_t pages, addr_t virt, void *dest, unsigned type, 
				   uint32_t flags);

/* create a normal vm area */
void*		VmmAlloc(size_t pages, addr_t start, uint32_t flags);
/* create a shared vm area */
/*void*		VmmShare(vm_area_t* area, process_t* proc, addr_t start, uint32_t flags);*/
/* create a mapped file vm area */
void*		VmmMapFile(addr_t start, size_t pages, handle_t file, uint32_t flags);

void		VmmFree(vm_area_t* area);
bool		VmmCommit(vm_area_t* area, addr_t start, size_t pages);
void		VmmUncommit(vm_area_t* area);
void		VmmInvalidate(vm_area_t* area, addr_t start, size_t pages);
vm_area_t*	VmmArea(process_t* proc, const void* ptr);

#ifdef __cplusplus
}
#endif

#endif