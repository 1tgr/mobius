#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <os/com.h>
#include <kernel/obj.h>

/*!
 *	\defgroup	proc	Process Services
 *	\ingroup	kernel
 *	@{
 */

struct IModule;
struct IPager;
struct vm_area_t;

typedef struct process_t process_t;
struct process_info_t;
struct module_info_t;

struct process_t
{
	dword* page_dir;
	struct IModule *modules[10];
	struct IPager* stack_pager;
	addr_t stack_end, vmm_end;
	byte level;
	int num_modules;
	struct vm_area_t *first_vm_area, *last_vm_area;
	struct process_info_t* info;
	struct module_info_t* module_last;
	marshal_map_t* marshal_map;
	marshal_t last_marshal;
	unsigned id;
};

process_t* procLoad(byte level, const wchar_t* file, const wchar_t* cmdline);
void procDelete(process_t* proc);
void procTerminate(process_t* proc);
process_t* procCurrent();

#undef INTERFACE
#define INTERFACE IModule

//! Implements an executable module in the kernel.
/*!
 *	Modules, such as executables (EXEs) and dynamic libraries (DLLs) are 
 *		treated transparently by the kernel by handling them as IModule 
 *		interfaces (IPager is also used, to allow modules to load themselves
 *		on demand).
 *
 *	Modules are loaded using the modLoadXXX() functions, which, given the name
 *		of a file, are expected to return an IModule interface to a module 
 *		object if successful. This will then be used later by the kernel to 
 *		import and export functions and, in the case of executables, to run
 *		processes.
 *
 *	Currently only the PE (Portable Executable) format is supported. 
 *		Theoretically any module format could be supported; to be used as a
 *		dynamically-linked library format, a module must be capable of 
 *		exporting named functions.
 *
 *	\implement	If you are adding a new module format to the kernel.
 *	\use	If you are accessing module attributes (such as exported function 
 *		or module base addresses) from the kernel.
 */
DECLARE_INTERFACE(IModule)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	//! Retrieves the main entry point for the module.
	/*!
	 *	For executables this is the address where execution of the main thread
	 *		will begin.
	 *
	 *	For DLLs this is the address of the DLLMain function, which will be
	 *		called when the module is loaded.
	 *	\return	The entry point of the module in the process's address space.
	 */
	STDMETHOD_(const void*, GetEntryPoint)(THIS) PURE;

	//! Retrieves the base address of the module.
	/*!
	 *	This may be different to the base originally specified if the module 
	 *		has been relocated.
	 *	\return	The base address of the module in the process's address space.
	 */
	STDMETHOD_(const void*, GetBase)(THIS) PURE;

	//! Retrieves the address of a named function exported by a module.
	/*!
	 *	In order to support dynamic linking, modules may support the ability to
	 *		name functions that are accessible by other modules. This function
	 *		allows the kernel to import functions by name.
	 *	\param	proc	The name of the function, in 8-bit character format
	 *	\return	The address of the function, or NULL if the function was not
	 *		found.
	 */
	STDMETHOD_(const void*, GetProcAddress)(THIS_ const char* proc) PURE;

	//! Retrieves the name of the module.
	/*!
	 *	The module name is used to identify the module within a particular
	 *		process.
	 *	\param	name	Points to a buffer which will receive the module's name
	 *	\param	max		Specifies the size of the buffer, in characters
	 *	\return	S_OK if successful, or a failure code.
	 */
	STDMETHOD(GetName)(THIS_ wchar_t* name, size_t max) PURE;
};

IModule* modLoadPe(process_t* proc, const wchar_t* file, dword base);
IModule* modLoadPeMemory(process_t* proc, const wchar_t* file, const void* ptr, dword base);

// {816B7D52-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IModule, 
0x816b7d52, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

#include <os/pe.h>

//@}

#ifdef __cplusplus
}
#endif

#endif