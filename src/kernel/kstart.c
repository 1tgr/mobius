#include <kernel/kernel.h>

extern char scode[], stack_end[];

//! Adjusts the value of a pointer so that it can be used before the 
//!	initial kernel GDT is set up.
#define MANGLE_PTR(type, ptr)	((type) ((char*) ptr - scode))

#ifdef _MSC_VER
void i386_lidt(idtr_t* p)
{
	__asm lidt [p]
}

void i386_lgdt(gdtr_t* p)
{
	__asm lgdt [p]
}

void sysStart();

#else
#define i386_lidt(ptr)	asm("lidt (%0)" : : "r" (ptr));
#define i386_lgdt(ptr)	asm("lgdt (%0)" : : "r" (ptr));
#endif

//! GDT register used by the kernel
gdtr_t _gdtr;
//! GDT used by the kernel
descriptor_t _gdt[9];
//! Global system information block
sysinfo_t _sysinfo;

//! The first kernel function called by the startup code
/*!
 *	\param	ramdisk_phys	Location of the ramdisk in physical memory
 *	\param	ramdisk_size	The length of the ramdisk
 *	\param	phys	Location of the kernel in physical memory; the physical
 *		address of the start() function
 *	\param	kernel_size		The length of the kernel code and data, not 
 *		including the bss
 *	\param	mem_top	The address of the top of physical memory
 */
int INIT_CODE _main(addr_t ramdisk_phys, 
					size_t ramdisk_size, 
					addr_t phys, 
					size_t kernel_size, 
					addr_t mem_top)
{
	gdtr_t* gdtr = MANGLE_PTR(gdtr_t*, &_gdtr);
	descriptor_t* gdt = MANGLE_PTR(descriptor_t*, _gdt);
	sysinfo_t* sysinfo = MANGLE_PTR(sysinfo_t*, &_sysinfo);
	idtr_t idtr = { 0, 0 };

	i386_lidt(&idtr);
	
	if (mem_top > 128 * 1048576)
		mem_top = 128 * 1048576;

	sysinfo->kernel_phys = phys;
	sysinfo->kernel_size = kernel_size;
	sysinfo->memory_top = mem_top;
	sysinfo->ramdisk_phys = ramdisk_phys;
	sysinfo->ramdisk_size = ramdisk_size;

	// null descriptor = 0
	i386_set_descriptor(gdt + 0, 0, 0, 0, 0);
	// kernel code = 0x8
	i386_set_descriptor(gdt + 1, phys - (addr_t) scode, 0xfffff, 
		ACS_CODE | ACS_DPL_0, ATTR_DEFAULT | ATTR_GRANULARITY);
	// kernel data = 0x10
	i386_set_descriptor(gdt + 2, phys - (addr_t) scode, 0xfffff, 
		ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
	// flat kernel code = 0x18
	i386_set_descriptor(gdt + 3, 0, 0xfffff, 
		ACS_CODE | ACS_DPL_0, ATTR_DEFAULT | ATTR_GRANULARITY);
	// flat kernel data = 0x20
	i386_set_descriptor(gdt + 4, 0, 0xfffff, 
		ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
	// flat user code = 0x28
	i386_set_descriptor(gdt + 5, 0, 0xfffff, 
		ACS_CODE | ACS_DPL_3, ATTR_DEFAULT | ATTR_GRANULARITY);
	// flat user data = 0x30
	i386_set_descriptor(gdt + 6, 0, 0xfffff, 
		ACS_DATA | ACS_DPL_3, ATTR_BIG | ATTR_GRANULARITY);
	// TSS = 0x38 (set up in main)
	i386_set_descriptor(gdt + 7, 0, 0, 0, 0);
	// user-mode thread data = 0x40 (base set on context switch)
	i386_set_descriptor(gdt + 8, 0, 1, 
		ACS_DATA | ACS_DPL_3, ATTR_BIG | ATTR_GRANULARITY);

	gdtr->base = (addr_t) gdt + phys;
	gdtr->limit = sizeof(_gdt) - 1;
	i386_lgdt(gdtr);
	
#ifdef _MSC_VER
	sysStart();
#else
	asm("mov $0x10,%%eax ; "
		"mov %%eax,%%ds ;"
		"mov %%eax,%%ss ;": : : "eax");
	asm("mov $0x20,%%eax ; "
		"mov %%eax,%%es ;"
		"mov %%eax,%%gs ;"
		"mov %%eax,%%fs ;": : : "eax");
	asm("mov $_stack_end,%%esp" : : : "esp");
	asm("ljmp $8,$_main");
#endif

	return 0;
}
