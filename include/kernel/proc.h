#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

#ifdef __cplusplus
extern "C"
{
#endif

//#include <os/com.h>
#include <kernel/obj.h>

/*!
 *	\defgroup	proc	Process Services
 *	\ingroup	kernel
 *	@{
 */

struct vm_area_t;

typedef struct process_t process_t;
struct process_info_t;
struct module_info_t;
struct file_t;

typedef struct module_t module_t;
struct module_t
{
	module_t *prev, *next;
	wchar_t *name;
	addr_t base;
	size_t length;
	addr_t entry;
	//void *raw_data;
	struct file_t *file;
	size_t sizeof_headers;
	bool imported;
};

struct process_t
{
	dword* page_dir;
	module_t *mod_first, *mod_last;
	addr_t stack_end, vmm_end;
	byte level;
	struct vm_area_t *first_vm_area, *last_vm_area;
	struct process_info_t* info;
	struct module_info_t* module_last;
	marshal_map_t* marshal_map;
	marshal_t last_marshal;
	unsigned id;

	semaphore_t sem_vmm;
};

process_t* procLoad(byte level, const wchar_t* file, const wchar_t* cmdline,
					unsigned priority, struct file_t* input, struct file_t* output);
void	procDelete(process_t* proc);
void	procTerminate(process_t* proc);
process_t*	procCurrent();
bool	procPageFault(process_t* proc, addr_t virt);

module_t*	peLoad(process_t* proc, const wchar_t* file, dword base);
module_t*	peLoadMemory(process_t* proc, const wchar_t* file, 
						 struct file_t *fd, dword base);
bool	pePageFault(process_t* proc, module_t* mod, addr_t addr);
addr_t	peGetExport(module_t* mod, const char* name);
void	peUnload(process_t* proc, module_t* mod);

#include <os/pe.h>

//@}

#ifdef __cplusplus
}
#endif

#endif