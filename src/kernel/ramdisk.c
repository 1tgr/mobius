/* $Id: ramdisk.c,v 1.2 2001/11/05 22:41:06 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/fs.h>
#include <kernel/ramdisk.h>

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

bool RdFsRequest(device_t* dev, request_t* req)
{
	request_fs_t *req_fs = (request_fs_t*) req;

	ramfd_t *fd;
	ramfile_t *file;
	unsigned i;
	wchar_t name[16];
	uint8_t *ptr;
	size_t len;

	switch (req->code)
	{
	case FS_OPEN:
		assert(req_fs->params.fs_open.name[0] == '/');

		if (req_fs->params.fs_open.flags & FILE_WRITE)
		{
			req->result = EACCESS;
			return false;
		}

		/*wprintf(L"RdFsRequest: open %s: ", req_fs->params.fs_open.name + 1);*/
		file = NULL;
		for (i = 0; i < ramdisk_header->num_files; i++)
		{
			len = mbstowcs(name, ramdisk_files[i].name, _countof(name));
			name[len] = '\0';
			if (_wcsicmp(name, req_fs->params.fs_open.name + 1) == 0)
			{
				file = ramdisk_files + i;
				break;
			}
		}

		if (file == NULL)
		{
			req_fs->header.result = ENOTFOUND;
			req_fs->params.fs_open.file = NULL;
			wprintf(L"%s: not found on ram disk\n", 
				req_fs->params.fs_open.name);
			return false;
		}

		req_fs->params.fs_open.file = HndAlloc(NULL, sizeof(ramfd_t), 'file');
		fd = HndLock(NULL, req_fs->params.fs_open.file, 'file');
		assert(fd != NULL);
		fd->file.fsd = dev;
		fd->file.pos = 0;
		fd->file.flags = req_fs->params.fs_open.flags;
		fd->ram = file;
		HndUnlock(NULL, req_fs->params.fs_open.file, 'file');
		
		/*wprintf(L"RdFsRequest: FS_OPEN(%s), file = %p = %x, %d bytes\n", 
			name, 
			file, 
			*(uint32_t*) ((uint8_t*) ramdisk_header + fd->ram->offset),
			fd->ram->length);*/
		
		return true;

	case FS_CLOSE:
		HndFree(NULL, req_fs->params.fs_close.file, 'file');
		return true;

	case FS_READ:
		fd = HndLock(NULL, req_fs->params.fs_read.file, 'file');
		assert(fd != NULL);
	
		if (fd->file.pos + req_fs->params.fs_read.length >= fd->ram->length)
			req_fs->params.fs_read.length = fd->ram->length - fd->file.pos;

		ptr = (uint8_t*) ramdisk_header + fd->ram->offset + (uint32_t) fd->file.pos;
		/*wprintf(L"RdFsRequest: read %x (%S) at %x => %x = %08x\n",
			fd->ram->offset,
			fd->ram->name,
			(uint32_t) fd->file.pos, 
			(addr_t) ptr,
			*(uint32_t*) ptr);*/

		memcpy((void*) req_fs->params.fs_read.buffer, 
			ptr,
			req_fs->params.fs_read.length);
		fd->file.pos += req_fs->params.fs_read.length;

		HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

/*
bool RdPageFault(addr_t virt)
{
	addr_t phys;
	
	if ((virt >= 0xd0000000) && (virt < 0xe0000000))
	{
		virt &= -PAGE_SIZE;
		return MemMap(virt, kernel_startup.ramdisk_phys + virt - 0xd0000000,
			virt + PAGE_SIZE, PRIV_KERN | PRIV_RD | PRIV_PRES);
	}
	else if (virt >= 0xc0000000)
	{
		phys = MemTranslate((void*) virt);
		if (phys)
		{
			wprintf(L"Mapping kernel page %x => %x\n", virt, phys);
			MemMap(virt, phys & -PAGE_SIZE, virt + PAGE_SIZE, phys & (PAGE_SIZE - 1));
			return true;
		}
	}
	
	return false;
}
*/

/*!	\brief Initialises the ramdisk during kernel startup.
 *
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
bool RdInit()
{
	/*ramdisk_header = (ramdisk_t*) 0xd0000000;*/
	ramdisk_header = PHYSICAL(kernel_startup.ramdisk_phys);
	ramdisk_files = (ramfile_t*) (ramdisk_header + 1);

	if (ramdisk_header->signature != RAMDISK_SIGNATURE_1 &&
		ramdisk_header->signature != RAMDISK_SIGNATURE_2)
	{
		wprintf(L"ramdisk: invalid signature\n");
		return false;
	}

	return true;
}

/*!	\brief Retrieves a pointer to a file in the ramdisk.
 *	This function should only be used to access files before device drivers 
 *		(and, hence, the ramdisk driver) are started.
 *
 *	\param	name	The name of the file to open. File names in the ramdisk are
 *		limited to 16 ASCII bytes.
 *	\return	A pointer to the start of the file data.
 */
void* RdOpen(const wchar_t* name)
{
	wchar_t temp[16];
	int i;
	size_t len;

	assert(ramdisk_header != NULL);
	assert(ramdisk_files != NULL);

	for (i = 0; i < ramdisk_header->num_files; i++)
	{
		len = 
			mbstowcs(temp, ramdisk_files[i].name, _countof(ramdisk_files[i].name));
		temp[len] = '\0';
		/* wprintf(L"%s\t%x\n", temp, ramdisk_files[i].offset); */
		if (_wcsicmp(name, temp) == 0)
		{
			assert(ramdisk_files[i].offset < kernel_startup.ramdisk_size);
			return (uint8_t*) ramdisk_header + ramdisk_files[i].offset;
		}
	}

	return NULL;
}

size_t RdFileLength(const wchar_t* name)
{
	wchar_t temp[16];
	int i;

	assert(ramdisk_header != NULL);
	assert(ramdisk_files != NULL);

	for (i = 0; i < ramdisk_header->num_files; i++)
	{
		mbstowcs(temp, ramdisk_files[i].name, _countof(ramdisk_files[i].name));
		/*wprintf(L"%s\t%x\n", temp, files[i].offset);*/
		if (_wcsicmp(name, temp) == 0)
			return ramdisk_files[i].length;
	}

	return -1;
}

device_t* RdMountFs(driver_t* driver, const wchar_t* path, device_t *dev)
{
	device_t *ram = malloc(sizeof(device_t));
	ram->driver = driver;
	ram->request = RdFsRequest;
	return ram;
}

driver_t rd_driver =
{
	NULL,
	NULL,
	NULL,
	NULL,
	RdMountFs
};