#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>

#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <ctype.h>

#include <os/blkdev.h>
#include <os/fs.h>
#include <os/defs.h>

#include "fat.h"

#define DEBUG
#include <kernel/debug.h>

/*! \ingroup fat */
/*@{ */

typedef struct fat_root_t fat_root_t;
struct fat_root_t
{
	device_t dev;
	device_t *disk;
	fat_bootsector_t boot_sector;
	uint8_t *fat;
	uint8_t fat_bits;
	uint32_t bytes_per_cluster;
	uint32_t data_start, root_start;
};

typedef struct fat_file_t fat_file_t;
struct fat_file_t
{
	file_t file;
	bool is_search;
	fat_dirent_t entry;
	uint64_t cached_pos;
	uint32_t cached_cluster;
};

typedef struct fat_search_t fat_search_t;
struct fat_search_t
{
	file_t file;
	bool is_search;
	fat_file_t *dir;
	wchar_t *spec;
};

#define FAT_AVAILABLE		0
#define FAT_RESERVED_START	0xfff0
#define FAT_RESERVED_END	0xfff6
#define FAT_BAD				0xfff7
#define FAT_EOC_START		0xfff8
#define FAT_EOC_END			0xffff

#define IS_EOC_CLUSTER(c)		((c) >= FAT_EOC_START && (c) <= FAT_EOC_END)
#define IS_RESERVED_CLUSTER(c)	((c) >= FAT_RESERVED_START && (c) <= FAT_RESERVED_END)

void dump(const uint8_t* buf, size_t size)
{
	int j;

	for (j = 0; j < size; j++)
		wprintf(L"%02x ", buf[j]);

	wprintf(L"\n");
}

uint32_t fatGetNextCluster(fat_root_t* root, uint32_t cluster)
{
	uint32_t FATOffset;
	uint16_t w;
	uint8_t* b;

	if (cluster >=
		root->boot_sector.sectors / root->boot_sector.sectors_per_cluster)
	{
		wprintf(L"Cluster 0x%x (%d) beyond than total clusters 0x%x (%d)\n",
			cluster, cluster,
			root->boot_sector.sectors / root->boot_sector.sectors_per_cluster,
			root->boot_sector.sectors / root->boot_sector.sectors_per_cluster);
		assert(false);
	}

	switch (root->fat_bits)
	{
	case 12:
		FATOffset = cluster + cluster / 2;
		break;
	case 16:
		FATOffset = cluster * 2;
		break;
	default:
		assert(root->fat_bits == 12 || root->fat_bits == 16);
		return -1;
	}

	b = root->fat + FATOffset;
	w = *(uint16_t*) b;
	/*dump(b - 2, 16); */

	if (root->fat_bits == 12)
	{
		if (cluster & 1)	/* cluster is odd */
			w >>= 4;
		else				/* cluster is even */
			w &= 0xfff;
		
		if (w >= 0xff0)
			w |= 0xf000;
	}
	
	return w;
}

uint32_t fatFindCluster(fat_file_t* file, uint64_t pos)
{
	fat_root_t *root = (fat_root_t*) file->file.fsd;
	uint64_t ptr;
	uint32_t cluster;

	cluster = (uint32_t) file->entry.first_cluster;

	if (cluster == 0)
		return (uint32_t) pos / root->bytes_per_cluster;
	else
	{
		ptr = root->bytes_per_cluster;
		while (ptr <= pos)
		{
			if (IS_EOC_CLUSTER(cluster) ||
				IS_RESERVED_CLUSTER(cluster) ||
				cluster == FAT_BAD)
				return -1;

			ptr += root->bytes_per_cluster;
			cluster = fatGetNextCluster(root, cluster);
		}

		return cluster;
	}
}

status_t fatRead(fat_root_t *root, fat_file_t *file, void *buffer, size_t *length)
{
	uint32_t cluster;
	uint64_t cluster_pos;
	size_t incr, this_cluster, user_length;
	bool isRoot;

	if (file->file.pos != file->cached_pos)
		file->cached_cluster = fatFindCluster(file, file->file.pos);
	else if (IS_EOC_CLUSTER(file->cached_cluster))
	{
		*length = 0;
		return 0;
	}

	if (file->cached_cluster == -1)
	{
		wprintf(L"fatReadFile: failed to find cluster at %d for file at 0x%x\n",
			(uint32_t) file->file.pos, file->entry.first_cluster);
		return ENOTFOUND;
	}

	isRoot = file->entry.first_cluster == 0;
	cluster = file->cached_cluster;

	user_length = *length;
	*length = 0;

	this_cluster = (uint32_t) file->file.pos % root->bytes_per_cluster;

	TRACE1("[%x] ", cluster);
	while (*length < user_length)
	{
		if (isRoot)
			/* root directory or FAT */
			cluster_pos = 
				root->root_start * root->boot_sector.bytes_per_sector 
				+ cluster * root->bytes_per_cluster;
		else
			/* normal file or directory */
			cluster_pos = 
				root->data_start * root->boot_sector.bytes_per_sector 
				+ (cluster - 2) * root->bytes_per_cluster;
		
		cluster_pos += this_cluster;

		TRACE2("(%lu = %lu) ", 
			(unsigned long) file->file.pos,
			(unsigned long) cluster_pos);
		incr = min(user_length - *length, root->bytes_per_cluster);
		
		if (DevRead(root->disk, 
			cluster_pos,
			(uint8_t*) buffer + *length,
			incr) != incr)
		{
			wprintf(L"fatReadFile: disk read failed at %u\n",
				(unsigned long) cluster_pos);
			file->cached_pos = file->file.pos;
			file->cached_cluster = cluster;
			return EHARDWARE;
		}

		*length += incr;
		file->file.pos += incr;
		
		this_cluster += incr;
		if (this_cluster >= root->bytes_per_cluster)
		{
			if (isRoot)
				cluster++;
			else
				cluster = fatGetNextCluster(root, cluster);
			
			this_cluster -= root->bytes_per_cluster;
			TRACE1("next cluster = %x\n", cluster);
		}

		if (IS_EOC_CLUSTER(cluster))
			break;
	}

	file->cached_pos = file->file.pos;
	file->cached_cluster = cluster;
	return 0;
}

bool fatReadDir(fat_root_t* root, fat_file_t *dir, wchar_t *name, 
				size_t name_max, fat_dirent_t* entry)
{
	union
	{
		fat_dirent_t di;
		fat_lfnslot_t lfn;
	} u;
	wchar_t wtemp[256], *nameptr;
	char temp[14];
	size_t length;
	int i, k, slot;
	status_t hr;
	/*bool gotEntry; */
	
	/*gotEntry = false; */
	while (true)
	{
		/*if (!gotEntry) */
		{
			length = sizeof(u);
			hr = fatRead(root, dir, &u, &length);
			if (hr != 0)
			{
				TRACE0("fatReadDir: end of directory\n");
				return hr;
			}

			/*gotEntry = true; */
		}

		if (u.di.name[0] == 0)
		{
			TRACE0("fatReadDir: end of entries\n");
			return ENOTFOUND;
		}

		if (u.di.name[0] == 0xe5)
		{
			/*gotEntry = false; */
			continue;
		}

		name[0] = '\0';
		if (u.di.attribs != ATTR_LONG_NAME)
		{
			memset(temp, 0, sizeof(temp));

			for (i = 0; i < 8; i++)
			{
				if (u.di.name[i] == ' ')
				{
					temp[i] = 0;
					break;
				}
				else
					temp[i] = u.di.name[i];
			}

			if (u.di.extension[0] != ' ')
			{
				strcat(temp, ".");

				k = strlen(temp);
				for (i = 0; i < 3; i++)
				{
					if (u.di.extension[i] == ' ')
					{
						temp[i + k] = 0;
						break;
					}
					else
						temp[i + k] = u.di.extension[i];
				}
			}

			/* xxx -- need to convert from OEM to UCS */
			mbstowcs(name, temp, name_max);
			/*gotEntry = false; */
		}
		else
		{
			memset(wtemp, 0, sizeof(wtemp));
			assert((u.lfn.slot & 0x40) == 0x40);
			
			while (u.di.attribs == ATTR_LONG_NAME)
			{
				slot = (u.lfn.slot & ~0x40) - 1;
				/*wprintf(L"slot %d ", slot); */
				nameptr = wtemp + (slot * 13);

				/*
				 * Check whether the end of this slot's name is still inside 
				 *	the buffer
				 */
				assert(nameptr + 13 < wtemp + _countof(wtemp));

				for (i = 0; i < _countof(u.lfn.name0_4); i++)
					*nameptr++ = u.lfn.name0_4[i];
				
				for (i = 0; i < _countof(u.lfn.name5_10); i++)
					*nameptr++ = u.lfn.name5_10[i];
				
				for (i = 0; i < _countof(u.lfn.name11_12); i++)
					*nameptr++ = u.lfn.name11_12[i];

				length = sizeof(u);
				hr = fatRead(root, dir, &u, &length);
				if (hr != 0)
				{
					TRACE0("fatReadDir: end of directory\n");
					return hr;
				}
			}

			/*
			 * The entry immediately following the LFN slots is the alias 
			 *	itself, so we need to skip it. u.di contains the alias at 
			 *	this point.
			 */
			
			if (u.di.name[0] != 0xe5)
				wcsncpy(name, wtemp, name_max);
			/*gotEntry = true; */
			/*wprintf(L"%s\t\t%x\n", name, u.di.first_cluster); */
		}

		if (name[0])
		{
			*entry = u.di;
			return 0;
		}
	}
}

status_t fatLookupEntry(fat_root_t *root, fat_file_t *dir, 
						const wchar_t *filename, fat_dirent_t *entry)
{
	wchar_t buf[MAX_PATH];
	status_t hr;

	TRACE2("fatLookupEntry: searching for %s in directory at %x\n",
		filename, dir->entry.first_cluster);
	dir->file.pos = 0;

	while ((hr = fatReadDir(root, dir, buf, _countof(buf), entry)) == 0)
		if (_wcsicmp(buf, filename) == 0)
		{
			TRACE2("fatLookupEntry: found %s at %x\n", 
				filename, entry->first_cluster);
			return 0;
		}
	
	return hr;
}

#define FsIsWildcard(path)	false

bool fatOpenFile(fat_root_t* root, request_fs_t* req)
{
	wchar_t *ch, component[MAX_PATH];
	const wchar_t* path;
	fat_file_t *fd, dir;
	fat_dirent_t entry;

	memset(&entry, 0, sizeof(entry));
	entry.attribs = ATTR_DIRECTORY;
	entry.first_cluster = 0;
	entry.file_length = 
		root->boot_sector.num_root_entries * sizeof(fat_dirent_t);

	path = req->params.fs_open.name + 1;
	dir.entry = entry;
	dir.cached_cluster = dir.entry.first_cluster;
	dir.file.pos = dir.cached_pos = 0;
	
	while ((ch = wcschr(path, '/')))
	{
		wcsncpy(component, path, ch - path);
		path += wcslen(component) + 1;
		
		req->header.result = fatLookupEntry(root, &dir, component, &entry);
		if (req->header.result != 0)
			return false;
		
		dir.file.pos = dir.cached_pos = 0;
		dir.entry = entry;
		dir.cached_cluster = dir.entry.first_cluster;
	}

	/*
	 * At this point, entry and cluster refer to the directory entry and 
	 *	cluster for the next-to-last component of the file spec respectively.
	 */

	if (FsIsWildcard(path))
	{
		fat_search_t *search;

		if ((entry.attribs & ATTR_DIRECTORY) == 0)
		{
			req->header.result = EINVALID;
			return false;
		}

		req->params.fs_open.file = HndAlloc(NULL, sizeof(fat_search_t), 'file');
		search = HndLock(NULL, req->params.fs_open.file, 'file');
		search->file.fsd = &root->dev;
		search->file.pos = 0;
		search->is_search = true;
		search->spec = _wcsdup(path);

		/* fd is the file descriptor for the directory being searched */
		search->dir = malloc(sizeof(fat_file_t));
		fd->file.fsd = &root->dev;
		fd->file.pos = 0;
		fd->is_search = false;
		fd->entry = entry;
		fd->cached_pos = 0;
		fd->cached_cluster = entry.first_cluster;

		HndUnlock(NULL, req->params.fs_open.file, 'file');
		TRACE2("fatOpenFile: opened search object %s for dir at %x\n",
			search->spec, fd->entry.first_cluster);
	}
	else
	{
		req->header.result = fatLookupEntry(root, &dir, path, &entry);
		if (req->header.result != 0)
			return false;
		
		req->params.fs_open.file = HndAlloc(NULL, sizeof(fat_file_t), 'file');
		fd = HndLock(NULL, req->params.fs_open.file, 'file');
		fd->file.fsd = &root->dev;
		fd->file.pos = 0;
		fd->is_search = false;
		fd->entry = entry;
		fd->cached_pos = 0;
		fd->cached_cluster = entry.first_cluster;

		HndUnlock(NULL, req->params.fs_open.file, 'file');
		TRACE2("fatOpenFile: opened file %s at %x\n",
			path, fd->entry.first_cluster);
	}

	return true;
}

bool fatReadFile(fat_root_t* root, fat_file_t *file, request_fs_t* req)
{
	req->header.result = fatRead(root, file, 
		(void*) req->params.fs_read.buffer, 
		&req->params.fs_read.length);
	return req->header.result == 0;
}

bool fatReadSearch(fat_root_t* root, fat_search_t *search, request_fs_t* req)
{
	dir_entry_t *entry;
	fat_dirent_t dirent;
	wchar_t name[MAX_PATH];
	status_t hr;
	size_t user_length;

	TRACE1("fatReadSearch(%s)\n", search->spec);
	
	if (req->params.fs_read.length % sizeof(*entry) != 0)
	{
		req->header.result = EBUFFER;
		return false;
	}

	user_length = req->params.fs_read.length;
	req->params.fs_read.length = 0;
	entry = (dir_entry_t*) req->params.fs_read.buffer;

	while (req->params.fs_read.length < user_length)
	{
		hr = fatReadDir(root, search->dir, name, _countof(name), &dirent);
		if (hr != 0)
		{
			req->header.result = hr;
			return false;
		}
		
		TRACE1("%s ", name);
		if (_wcsmatch(search->spec, name) == 0)
		{
			wcscpy(entry->name, name);
			entry->attribs = dirent.attribs;
			entry->length = dirent.file_length;

			TRACE0("ok\n");
			entry++;
			req->params.fs_read.length += sizeof(*entry);
		}
	}

	TRACE0("finished\n");
	return true;
}

bool fatRequest(device_t* dev, request_t* req)
{
	request_fs_t *req_fs = (request_fs_t*) req;
	fat_root_t *root = (fat_root_t*) dev;
	fat_file_t *file;
	fat_search_t *search;
	bool ret;
		
	switch (req->code)
	{
	case FS_CLOSE:
		file = HndLock(NULL, req_fs->params.fs_close.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return true;
		}

		if (file->is_search)
		{
			search = (fat_search_t*) file;
			free(search->spec);
			free(search->dir);
		}

		HndFree(NULL, req_fs->params.fs_close.file, 'file');
		return true;

	case FS_OPEN:
		return fatOpenFile(root, (request_fs_t*) req);

	case FS_READ:
		file = HndLock(NULL, req_fs->params.fs_read.file, 'file');
		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		if (file->is_search)
			ret = fatReadSearch(root, (fat_search_t*) file, 
				(request_fs_t*) req);
		else
			ret = fatReadFile(root, file, (request_fs_t*) req);

		HndUnlock(NULL, req_fs->params.fs_close.file, 'file');
		return ret;

	case FS_GETLENGTH:
		file = HndLock(NULL, req_fs->params.fs_getlength.file, 'file');

		if (file == NULL)
		{
			req->result = EHANDLE;
			return false;
		}

		if (file->is_search)
			/* xxx - return a proper length here? */
			req_fs->params.fs_getlength.length = 0;
		else
			req_fs->params.fs_getlength.length = file->entry.file_length;

		HndUnlock(NULL, req_fs->params.fs_getlength.file, 'file');
		return true;
	}

	req->code = ENOTIMPL;
	return false;
}

device_t* FatMountFs(driver_t* driver, const wchar_t* path, device_t* dev)
{
	fat_root_t *root;
	size_t length;
	uint32_t RootDirSectors, FatSectors;
	block_size_t size;
	request_dev_t req;

	TRACE0("FatMountFs:\n");

	root = malloc(sizeof(fat_root_t));
	memset(root, 0, sizeof(fat_root_t));
	root->dev.driver = driver;
	root->dev.request = fatRequest;
	root->disk = dev;

	size.total_blocks = 0;
	req.header.code = BLK_GETSIZE;
	req.params.buffered.buffer = (addr_t) &size;
	req.params.buffered.length = sizeof(size);
	if (!DevRequestSync(root->disk, &req.header) ||
		size.total_blocks > 20740)
	{
		TRACE1("\tTotal blocks = %d, using FAT16\n", size.total_blocks);
		root->fat_bits = 16;
	}
	else
	{
		TRACE1("\tTotal blocks = %d, using FAT12\n", size.total_blocks);
		root->fat_bits = 12;
	}
	
	TRACE1("\tReading boot sector: disk = %p\n", root->disk);
	if (DevRead(root->disk, 0, &root->boot_sector, sizeof(fat_bootsector_t)) 
			!= sizeof(fat_bootsector_t))
	{
		free(root);
		return NULL;
	}

	assert(root->boot_sector.sectors_per_fat != 0);
	assert(root->boot_sector.bytes_per_sector != 0);

	root->bytes_per_cluster = root->boot_sector.bytes_per_sector * 
		root->boot_sector.sectors_per_cluster;
	
	TRACE1("\tAllocating %u bytes for FAT\n",
		root->boot_sector.sectors_per_fat * 
		root->boot_sector.bytes_per_sector);
	root->fat = malloc(root->boot_sector.sectors_per_fat * 
		root->boot_sector.bytes_per_sector);
	assert(root->fat != NULL);

	TRACE2("\tFAT starts at sector %d = 0x%x\n",
		root->boot_sector.reserved_sectors,
		root->boot_sector.reserved_sectors * 
			root->boot_sector.bytes_per_sector);

	length = root->boot_sector.sectors_per_fat * 
		root->boot_sector.bytes_per_sector;
	TRACE0("\tReading FAT\n");
	if (DevRead(root->disk, 
		root->boot_sector.reserved_sectors * 
			root->boot_sector.bytes_per_sector,
		root->fat,
		length) != length)
	{
		free(root->fat);
		free(root);
		return NULL;
	}

	RootDirSectors = (root->boot_sector.num_root_entries * 32) /
		root->boot_sector.bytes_per_sector;
	FatSectors = root->boot_sector.num_fats * root->boot_sector.sectors_per_fat;
	root->data_start = root->boot_sector.reserved_sectors + 
		FatSectors + 
		RootDirSectors;

	root->root_start = root->boot_sector.reserved_sectors + 
				root->boot_sector.hidden_sectors + 
				root->boot_sector.sectors_per_fat * 
					root->boot_sector.num_fats;

	return &root->dev;
}

bool DrvInit(driver_t* drv)
{
	drv->add_device = NULL;
	drv->mount_fs = FatMountFs;
	return true;
}

/*@} */