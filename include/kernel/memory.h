#ifndef __MEMORY_H
#define __MEMORY_H

#ifdef __cplusplus
extern "C"
{
#endif

bool memInit();
addr_t memAlloc();
void memFree(addr_t block);
addr_t memAllocLow();
void memFreeLow(addr_t block);
bool memMap(dword *PageDir, dword Virt, dword Phys, dword pages, byte Privilege);
dword memTranslate(const dword* pPageDir, const void* pAddress);

#if 0

#undef INTERFACE
#define INTERFACE IPager

//!	Implements a method to allow memory to be automatically paged in.
/*!
 *	\implement	If you are writing code which causes data to be loaded by
 *		being paged in, as opposed to manual loading. Examples include sections
 *		from executable files, thread stack pages, and the ramdisk.
 *	\use	This interface is only used from within the kernel's page fault 
 *		handler.
 */
DECLARE_INTERFACE(IPager)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	//! Called by the page fault handler when a memory page was not found,
	//!		allowing the handler to load the page and make it available.
	/*!
	 *	\param	virt	The virtual address which caused the page fault. Note that
	 *		this is not necessarily on a page boundary.
	 *	\return	S_OK if the address is now available, and program execution 
	 *		should continue; a failure code if this object does not handle this 
	 *		page, or if the page could not be made available.
	 */
	STDMETHOD(ValidatePage)(THIS_ const void* virt) PURE;
};

// {816B7D51-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IPager, 
0x816b7d51, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

#endif

#ifdef __cplusplus
}
#endif

#endif