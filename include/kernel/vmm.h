#ifndef __KERNEL_VMM_H
#define __KERNEL_VMM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>

typedef struct vm_area_t vm_area_t;

//! Describes an area of memory allocated within the address space of a 
//!		particular process.
struct vm_area_t
{
	//! Points to the previous vm_area_t in the owning process
	vm_area_t *prev, 
	//! Points to the next vm_area_t in the owning process
		*next;
	//! Indicates the start of the area in the owning process's address space
	void* start;
	//!	Specifies the number of pages spanned by the area
	size_t pages;
	//!	Indicates the start of the area in physical memory, or NULL if it has 
	//!		not been allocated
	//addr_t phys;
	//! Specifies the MEM_xxx flags associated with the area
	dword flags;
	bool is_committed;
};

/* vmmAlloc flags are in os/os.h */

void* vmmAlloc(process_t* proc, size_t pages, addr_t start, dword flags);
void vmmFree(process_t* proc, vm_area_t* area);
bool vmmCommit(process_t* proc, vm_area_t* area);
void vmmUncommit(process_t* proc, vm_area_t* area);
void vmmInvalidate(vm_area_t* area);
vm_area_t* vmmArea(process_t* proc, const void* ptr);

#ifdef __cplusplus
}
#endif

#endif