#include <kernel/kernel.h>
#include <kernel/ramdisk.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/vmm.h>
#include <kernel/memory.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>

extern process_t proc_idle;

ramdisk_t* ramdisk_header;
ramfile_t* ramdisk_files;

typedef struct ramfd_t ramfd_t;
struct ramfd_t
{
	file_t file;
	ramfile_t *ram;
};

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

bool ramFsRequest(device_t* dev, request_t* req)
{
	ramfd_t *fd;
	ramfile_t *file;
	unsigned i;
	wchar_t name[16];
	byte *ptr;

	switch (req->code)
	{
	case FS_OPEN:
		assert(req->params.fs_open.name[0] == '/');

		//wprintf(L"ramFsRequest: open %s: ", req->params.fs_open.name + 1);
		file = NULL;
		for (i = 0; i < ramdisk_header->num_files; i++)
		{
			mbstowcs(name, ramdisk_files[i].name, countof(name));
			if (wcsicmp(name, req->params.fs_open.name + 1) == 0)
			{
				file = ramdisk_files + i;
				break;
			}
		}

		if (file == NULL)
		{
			req->result = ENOTFOUND;
			req->params.fs_open.fd = NULL;
			wprintf(L"not found\n");
			return false;
		}

		fd = hndAlloc(sizeof(ramfd_t), NULL);
		assert(fd != NULL);
		fd->file.fsd = dev;
		fd->file.pos = 0;
		fd->ram = file;
		
		/*wprintf(L"ramFsRequest: FS_OPEN(%s), file = %p = %x, %d bytes\n", 
			name, 
			file, 
			*(dword*) ((byte*) ramdisk_header + fd->ram->offset),
			fd->ram->length);*/
		
		req->params.fs_open.fd = &fd->file;
		hndSignal(req->event, true);
		return true;

	case FS_CLOSE:
		fd = (ramfd_t*) req->params.fs_close.fd;
		assert(fd != NULL);

		hndFree(fd);
		hndSignal(req->event, true);
		return true;

	case FS_READ:
		fd = (ramfd_t*) req->params.fs_read.fd;
		assert(fd != NULL);
	
		if (fd->file.pos + req->params.fs_read.length >= fd->ram->length)
			req->params.fs_read.length = fd->ram->length - fd->file.pos;

		ptr = (byte*) ramdisk_header + fd->ram->offset + (dword) fd->file.pos;
		/*wprintf(L"ramRequest: read %x (%S) at %x => %x = %08x\n",
			fd->ram->offset,
			fd->ram->name,
			(dword) fd->file.pos, 
			(addr_t) ptr,
			*(dword*) ptr);*/

		memcpy((void*) req->params.fs_read.buffer, 
			ptr,
			req->params.fs_read.length);
		fd->file.pos += req->params.fs_read.length;

		hndSignal(req->event, true);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t* ramMountFs(driver_t* driver, const wchar_t* path, device_t* dev)
{
	device_t *ram = hndAlloc(sizeof(device_t), NULL);
	ram->driver = driver;
	ram->request = ramFsRequest;
	return ram;
}

bool ramPageFault(addr_t virt)
{
	addr_t phys;
	//vm_area_t *area;
	
	if ((virt >= 0xd0000000) && (virt < 0xe0000000))
	{
		virt &= -PAGE_SIZE;
		/*if (vmmMap(&proc_idle, 1, virt, 
			_sysinfo.ramdisk_phys + virt - 0xd0000000,
			0 | MEM_READ | MEM_COMMIT) == NULL)
			return false;
		else
			return true;*/

		return memMap(proc_idle.page_dir, virt, 
			_sysinfo.ramdisk_phys + virt - 0xd0000000,
			1, 0 | PRIV_RD | PRIV_PRES);
	}
	else if (virt >= 0xc0000000)
	{
		/*area = vmmArea(&proc_idle, (const void*) virt);
		if (area == NULL)
			return false;
		else
		{
			vmmShare(&proc_idle, 
		}*/
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
	ramdisk_header = (ramdisk_t*) 0xd0000000;
	ramdisk_files = (ramfile_t*) (ramdisk_header + 1);

	if (ramdisk_header->signature != RAMDISK_SIGNATURE_1 &&
		ramdisk_header->signature != RAMDISK_SIGNATURE_2)
	{
		wprintf(L"ramdisk: invalid signature\n");
		return false;
	}

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
	wchar_t temp[16];
	int i;

	assert(ramdisk_header != NULL);
	assert(ramdisk_files != NULL);

	for (i = 0; i < ramdisk_header->num_files; i++)
	{
		mbstowcs(temp, ramdisk_files[i].name, countof(ramdisk_files[i].name));
		//wprintf(L"%s\t%x\n", temp, files[i].offset);
		if (wcsicmp(name, temp) == 0)
		{
			assert(ramdisk_files[i].offset < _sysinfo.ramdisk_size);
			return (byte*) ramdisk_header + ramdisk_files[i].offset;
		}
	}

	return NULL;
}

size_t ramFileLength(const wchar_t* name)
{
	wchar_t temp[16];
	int i;

	assert(ramdisk_header != NULL);
	assert(ramdisk_files != NULL);

	for (i = 0; i < ramdisk_header->num_files; i++)
	{
		mbstowcs(temp, ramdisk_files[i].name, countof(ramdisk_files[i].name));
		//wprintf(L"%s\t%x\n", temp, files[i].offset);
		if (wcsicmp(name, temp) == 0)
			return ramdisk_files[i].length;
	}

	return -1;
}
