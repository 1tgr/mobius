#ifndef __KERNEL_VMM_H
#define __KERNEL_VMM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>
#include <kernel/fs.h>

typedef struct vm_area_t vm_area_t;

#define VM_AREA_NORMAL	0
#define VM_AREA_MAP		1
#define VM_AREA_SHARED	2
#define VM_AREA_FILE	3

//! Describes an area of memory allocated within the address space of a 
//!		particular process.
struct vm_area_t
{
	//! Points to the previous vm_area_t in the owning process
	vm_area_t *prev;
	//! Points to the next vm_area_t in the owning process
	vm_area_t *next;
	process_t *owner;

	//! Indicates the start of the area in the owning process's address space
	void* start;
	//!	Specifies the number of pages spanned by the area
	size_t pages;
	//! Specifies the MEM_xxx flags associated with the area
	dword flags;
	//bool is_committed;

	//! Specifies the type of the memory area
	unsigned type;

	union
	{
		addr_t phys_map;
		vm_area_t* shared_from;
		file_t *file;
	} dest;
};

/* vmmAlloc flags are in os/os.h */

/* create a vm_area_t */
void*		vmmMap(process_t* proc, size_t pages, addr_t virt, void *dest, 
				   unsigned type, dword flags);

/* create a normal vm area */
void*		vmmAlloc(process_t* proc, size_t pages, addr_t start, dword flags);
/* create a shared vm area */
void*		vmmShare(vm_area_t* area, process_t* proc, addr_t start, dword flags);
/* create a mapped file vm area */
void*		vmmMapFile(process_t *proc, addr_t start, size_t pages, file_t *file, 
					   dword flags);

void		vmmFree(vm_area_t* area);
bool		vmmCommit(vm_area_t* area, addr_t start, size_t pages);
void		vmmUncommit(vm_area_t* area);
void		vmmInvalidate(vm_area_t* area, addr_t start, size_t pages);
vm_area_t*	vmmArea(process_t* proc, const void* ptr);

#ifdef __cplusplus
}
#endif

#endif