/* $Id: proc.h,v 1.9 2002/08/17 23:09:01 pavlovskii Exp $ */
#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/handle.h>
#include <kernel/fs.h>

struct thread_t;

/*!
 *	\ingroup	kernel
 *	\defgroup	proc	Processes and Modules
 *	@{
 */

typedef struct module_t module_t;
struct module_t
{
	module_t *prev, *next;
	unsigned refs;
	wchar_t *name;
	addr_t base;
	size_t length;
	addr_t entry;
	handle_t file;
	size_t sizeof_headers;
	bool imported;
};

typedef struct process_t process_t;
struct process_t
{
	handle_hdr_t hdr;
	process_t *prev, *next;
	addr_t stack_end;
	addr_t page_dir_phys/*, *page_dir*/;
	void **handles;
	unsigned handle_count;
	unsigned handle_allocated;
	module_t *mod_first, *mod_last;
	struct vm_area_t *area_first, *area_last;
	/*addr_t vmm_end;*/
	spinlock_t sem_vmm;
	spinlock_t sem_lock;
	const wchar_t *exe;
	struct process_info_t *info;
	unsigned id;
	process_t *creator;
    int exit_code;
    vnode_t root, cur_dir;
};

extern process_t *proc_first, *proc_last;
extern process_t proc_idle;

process_t * ProcCreateProcess(const wchar_t *exe);
bool        ProcPageFault(process_t *proc, addr_t addr, bool is_writing);

module_t *  PeLoad(process_t* proc, const wchar_t* file, uint32_t base);
bool        PePageFault(process_t* proc, module_t* mod, addr_t addr);
void        PeUnload(process_t* proc, module_t* mod);
addr_t      PeGetExport(module_t* mod, const char* name, uint16_t hint);
bool        PeMapAddressToFile(module_t *mod, addr_t addr, uint64_t *off, 
                               size_t *bytes, uint32_t *flags);
void        PeProcessSection(module_t *mod, addr_t addr);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif