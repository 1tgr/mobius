/* $Id: ramdisk_mb.c,v 1.3 2002/02/27 18:33:55 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/fs.h>
#include <kernel/init.h>
#include <kernel/multiboot.h>

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>

extern process_t proc_idle;

typedef struct ramfd_t ramfd_t;
struct ramfd_t
{
	file_t file;
	multiboot_module_t *mod;
};

bool RdFsRequest(device_t* dev, request_t* req)
{
	request_fs_t *req_fs = (request_fs_t*) req;

	ramfd_t *fd;
	unsigned i;
	wchar_t name[16];
	uint8_t *ptr;
	size_t len;
	dirent_t *buf;
	multiboot_module_t *mods, *mod;
	char *ch;

	mods = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
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
		mod = NULL;
		for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
		{
			ch = strrchr(PHYSICAL(mods[i].string), '/');
			if (ch == NULL)
				ch = PHYSICAL(mods[i].string);
			else
				ch++;

			len = mbstowcs(name, ch, _countof(name));
			name[len] = '\0';
			if (_wcsicmp(name, req_fs->params.fs_open.name + 1) == 0)
			{
				mod = mods + i;
				break;
			}
		}

		if (mod == NULL)
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
		fd->mod = mod;
		HndUnlock(NULL, req_fs->params.fs_open.file, 'file');
		
		/*wprintf(L"RdFsRequest: FS_OPEN(%s), file = %p = %x, %d bytes\n", 
			name, 
			file, 
			*(uint32_t*) ((uint8_t*) ramdisk_header + fd->ram->offset),
			fd->ram->length);*/
		return true;

	case FS_OPENSEARCH:
		assert(req_fs->params.fs_opensearch.name[0] == '/');
		req_fs->params.fs_opensearch.file = HndAlloc(NULL, sizeof(ramfd_t), 'file');
		fd = HndLock(NULL, req_fs->params.fs_opensearch.file, 'file');
		if (fd == NULL)
		{
			req->result = errno;
			return false;
		}

		fd->file.fsd = dev;
		fd->file.pos = 0;
		fd->file.flags = FILE_READ;
		fd->mod = NULL;
		HndUnlock(NULL, req_fs->params.fs_opensearch.file, 'file');
		return true;

	case FS_CLOSE:
		HndClose(NULL, req_fs->params.fs_close.file, 'file');
		return true;

	case FS_READ:
		fd = HndLock(NULL, req_fs->params.fs_read.file, 'file');
		assert(fd != NULL);
	
		if (fd->mod != NULL)
		{
			/* Normal file */
			if (fd->file.pos + req_fs->params.fs_read.length >= 
				fd->mod->mod_end - fd->mod->mod_start)
				req_fs->params.fs_read.length = 
					fd->mod->mod_end - fd->mod->mod_start - fd->file.pos;

			ptr = (uint8_t*) PHYSICAL(fd->mod->mod_start) + (uint32_t) fd->file.pos;
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
		}
		else
		{
			/* Search */
			if (fd->file.pos >= kernel_startup.multiboot_info->mods_count)
			{
				req->result = EEOF;
				HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
				return false;
			}
			
			mod = mods + fd->file.pos;
			len = req_fs->params.fs_read.length;

			req_fs->params.fs_read.length = 0;
			buf = req_fs->params.fs_read.buffer;
			while (req_fs->params.fs_read.length < len)
			{
				size_t temp;

				ch = strrchr(PHYSICAL(mod->string), '/');
				if (ch == NULL)
					ch = PHYSICAL(mod->string);
				else
					ch++;

				temp = mbstowcs(buf->name, ch, _countof(buf->name));
				if (temp == -1)
					wcscpy(buf->name, L"?");
				else
					buf->name[temp] = '\0';

				buf->length = mod->mod_end - mod->mod_start;
				buf->standard_attributes = FILE_ATTR_READ_ONLY;

				buf++;
				mod++;
				req_fs->params.fs_read.length += sizeof(dirent_t);

				fd->file.pos++;
				if (fd->file.pos >= kernel_startup.multiboot_info->mods_count)
					break;
			}
		}

		HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

/*!	\brief Initializes the ramdisk during kernel startup.
 *
 *	This routine is called by \p KernelMain to check the ramdisk (loaded by the
 *		second-stage boot routine) and map it into the kernel's address
 *		space. Because it is mapped into the kernel's address space,
 *		it will be mapped into subsequent address spaces as needed.
 *
 *	\note	All objects in the ramdisk (including the ramdisk header, file
 *		headers and the file data themselves) should be aligned on page 
 *		boundaries. This is done automatically by the boot loader.
 *
 *	\return	\p true if the ramdisk was correct
 */
bool RdInit(void)
{
	/*unsigned i;
	multiboot_module_t *mods;*/
	wprintf(L"ramdisk: number of modules = %u\n",
		kernel_startup.multiboot_info->mods_count);
	/*mods = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
	for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
		wprintf(L"module %u: %S: %x=>%x\n", i, PHYSICAL(mods[i].string), 
			mods[i].mod_start, mods[i].mod_end);*/
	/*halt(0);*/
	return true;
}

static const device_vtbl_t rdfs_vtbl =
{
	RdFsRequest,
	NULL
};

device_t* RdMountFs(driver_t* driver, const wchar_t* path, device_t *dev)
{
	device_t *ram = malloc(sizeof(device_t));
	ram->driver = driver;
	ram->vtbl = &rdfs_vtbl;
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
