#include <stdlib.h>
#include <string.h>
#include <kernel/kernel.h>
#include <kernel/ramdisk.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

extern process_t proc_idle;

void dump(void* buf, size_t size)
{
	unsigned i, j;
	char c;

	for (i = 0; i < size; i += 16)
	{
		for (j = 0; j < 16 && i + j < size; j++)
			wprintf(L"%02x\t", ((byte*) buf)[i + j]);
		
		for (j = 0; j < 16 &&  i + j < size; j++)
		{
			if (((byte*) buf)[i + j] < 0x20)
				c = '.';
			else
				c = ((byte*) buf)[i + j];
			
			wprintf(L"%c", c);
		}

		//_cputws(L"\n");
	}
}

bool ramPageFault(addr_t virt)
{
	addr_t phys, page;
	
	if ((virt >= 0xd0000000) && (virt < 0xe0000000))
	{
		page = virt & -PAGE_SIZE;
		phys = _sysinfo.ramdisk_phys + page - 0xd0000000;
		//if (page > 0)
			//phys += PAGE_SIZE;
		//wprintf(L"CPagerRamdisk_ValidatePage: mapping page at %x to %x\n", page, phys);
		memMap(current->process->page_dir, page, phys, 1, 
			PRIV_KERN | PRIV_RD | PRIV_PRES);
		invalidate_page((void*) page);
		//wprintf(L" = %c%c\n", ((byte*) page)[0], ((byte*) page)[1]);
		//dump((void*) phys, 32);
		return true;
	}
	else if (virt >= 0xc0000000)
	{
		phys = memTranslate(proc_idle.page_dir, (void*) virt);
		if (phys)
		{
			wprintf(L"Mapping kernel page %x => %x\n", virt, phys);
			memMap(current->process->page_dir, virt, phys & -PAGE_SIZE, 1, phys & (PAGE_SIZE - 1));
			return true;
		}
	}
	
	return false;
}

//! Initialises the ramdisk during kernel startup.
/*!
 *	This routine is called by main() to check the ramdisk (loaded by the
 *		second-stage boot routine) and map it into the kernel's address
 *		space. Because it is mapped into the kernel's address space,
 *		it will be mapped into subsequent address spaces as needed.
 *
 *	\note	All objects in the ramdisk (including the ramdisk header, file
 *		headers and the file data themselves) should be aligned on page 
 *		boundaries. This is done automatically by the \p ramdisk program.
 *
 *	\return	A pointer to an IPager interface which will map ramdisk pages
 *		as needed.
 */
bool INIT_CODE ramInit()
{
	return true;
}

//! Retrieves a pointer to a file in the ramdisk.
/*!
 *	This function should only be used to access files before device drivers 
 *		(and, hence, the ramdisk driver) are started.
 *
 *	\param	name	The name of the file to open. File names in the ramdisk are
 *		limited to 16 ASCII bytes.
 *	\return	A pointer to the start of the file data.
 */
void* ramOpen(const wchar_t* name)
{
	ramdisk_t* header = (ramdisk_t*) 0xd0000000;
	ramfile_t* files = (ramfile_t*) (header + 1);
	wchar_t temp[16];
	int i;

	for (i = 0; i < header->num_files; i++)
	{
		mbstowcs(temp, files[i].name, countof(files[i].name));
		//wprintf(L"%s\t%x\n", temp, files[i].offset);
		if (wcsicmp(name, temp) == 0)
			return (byte*) header + files[i].offset;
	}

	return NULL;
}

size_t ramFileLength(const wchar_t* name)
{
	ramdisk_t* header = (ramdisk_t*) 0xd0000000;
	ramfile_t* files = (ramfile_t*) (header + 1);
	wchar_t temp[16];
	int i;

	for (i = 0; i < header->num_files; i++)
	{
		mbstowcs(temp, files[i].name, countof(files[i].name));
		//wprintf(L"%s\t%x\n", temp, files[i].offset);
		if (wcsicmp(name, temp) == 0)
			return files[i].length;
	}

	return -1;
}