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
#include "fat.h"

//#define DEBUG
#include <kernel/debug.h>

//! \ingroup fat
//@{

typedef struct fat_root_t fat_root_t;
struct fat_root_t
{
	device_t dev;
	device_t *disk;
	fat_bootsector_t boot_sector;
	byte *fat;
	byte fat_bits;
	dword bytes_per_cluster;
	dword data_start, root_start;
};

typedef struct fat_file_t fat_file_t;
struct fat_file_t
{
	file_t file;
	fat_dirent_t entry;
	qword cached_pos;
	dword cached_cluster;
};

#define FAT_AVAILABLE		0
#define FAT_RESERVED_START	0xfff0
#define FAT_RESERVED_END	0xfff6
#define FAT_BAD				0xfff7
#define FAT_EOC_START		0xfff8
#define FAT_EOC_END			0xffff

#define IS_EOC_CLUSTER(c)	((c) >= FAT_EOC_START && (c) <= FAT_EOC_END)

void dump(const byte* buf, size_t size)
{
	int j;

	for (j = 0; j < size; j++)
		wprintf(L"%02x ", buf[j]);

	_cputws(L"\n");
}

dword fatGetNextCluster(fat_root_t* root, dword cluster)
{
	dword FATOffset;
	word w;
	byte* b;

	assert(cluster < 
		root->boot_sector.sectors / root->boot_sector.sectors_per_cluster);
	switch (root->fat_bits)
	{
	case 12:
		FATOffset = cluster + cluster / 2;
		break;
	case 16:
		FATOffset = cluster * 2;
		break;
	}

	b = root->fat + FATOffset;
	w = *(word*) b;
	//dump(b - 2, 16);

	if (root->fat_bits == 12)
	{
		if (cluster & 1)	// cluster is odd
			w >>= 4;
		else				// cluster is even
			w &= 0xfff;
		
		if (w >= 0xff0)
			w |= 0xf000;
	}
	
	/*wprintf(L"FATOffset = 0x%x = 0x%x, w = 0x%04x = 0x%02x%02x\n", 
		FATOffset, 
		FATOffset + root->boot_sector.reserved_sectors * 
			root->boot_sector.bytes_per_sector, 
		w,
		b[1], b[0]);*/
	return w;
}

bool fatLookupEntry(fat_root_t* root, dword cluster, 
					const wchar_t* filename, bool is_root, fat_dirent_t* entry)
{
	union
	{
		fat_dirent_t di;
		fat_lfnslot_t lfn;
	} u[512 / sizeof(fat_dirent_t)];
	wchar_t name[MAX_PATH], temp[14], *nameptr;
	size_t length;
	int i, j, k;
	qword pos;
	
	if (is_root)
		cluster = 0;

	while (true)
	{
		if (is_root)
			pos = (root->root_start + cluster) * root->boot_sector.bytes_per_sector;
		else
			pos = root->data_start * root->boot_sector.bytes_per_sector + 
				(cluster - 2) * root->bytes_per_cluster;

		//wprintf(L"fatLookupEntry%s: cluster = %d pos = 0x%x\n",
			//is_root ? L"(root)" :  L"", cluster, (dword) pos);
		length = sizeof(u);
		if (devReadSync(root->disk, pos, u, &length))
		{
			wprintf(L"fatLookupEntry: disk read failed\n");
			return false;
		}

		for (j = 0; j < countof(u); j++)
		{
			if (u[j].di.name[0] == 0)
			{
				wprintf(L"fatLookupEntry: end of directory\n");
				return false;
			}
			if (u[j].di.name[0] != 0xe5)
			{
				memset(name, 0, sizeof(name));

				if ((u[j].di.attribs & ATTR_LONG_NAME) == 0)
				{
					for (i = 0; i < 8; i++)
					{
						if (u[j].di.name[i] == ' ')
						{
							name[i] = 0;
							break;
						}
						else if (iswupper(u[j].di.name[i]))
							name[i] = towlower(u[j].di.name[i]);
						else
							name[i] = u[j].di.name[i];
					}

					if (u[j].di.extension[0] != ' ')
					{
						wcscat(name, L".");

						k = wcslen(name);
						for (i = 0; i < 3; i++)
						{
							if (u[j].di.extension[i] == ' ')
							{
								name[i + k] = 0;
								break;
							}
							else if (iswupper(u[j].di.extension[i]))
								name[i + k] = towlower(u[j].di.extension[i]);
							else
								name[i + k] = u[j].di.extension[i];
						}
					}
				}
				else
				{
					//int lfn_start = j;
					while ((u[j].di.attribs & ATTR_LONG_NAME) == ATTR_LONG_NAME)
					{
						//wprintf(L"<lfn seq=%2x> ", u[j].di.name[0]);
						if (u[j].di.name[0] != 0xe5)
						{
							memset(temp, 0, sizeof(temp));
							nameptr = temp;

							for (i = 0; i < 5; i++)
							{
								*nameptr = u[j].lfn.name0_4[i];
								nameptr++;
							}

							for (i = 0; i < 6; i++)
							{
								*nameptr = u[j].lfn.name5_10[i];
								nameptr++;
							}

							for (i = 0; i < 2; i++)
							{
								*nameptr = u[j].lfn.name11_12[i];
								nameptr++;
							}

							i = wcslen(temp);
							memmove(name + i, name, wcslen(name) * sizeof(wchar_t));
							memcpy(name, temp, i * sizeof(wchar_t));
						}

						j++;
					}

					//j = lfn_start;
				}

				if (name[0])
				{
					TRACE2("%s\t\t%x\n", name, u[j].di.first_cluster);
					if (wcsicmp(name, filename) == 0)
					{
						*entry = u[j].di;
						TRACE2("%s: found at cluster %x\n", 
							name, u[j].di.first_cluster);
						return true;
					}
				}
			}
		}

		if (is_root)
		{
			cluster++;

			if (cluster >= 
				(root->boot_sector.num_root_entries * 32) / root->boot_sector.bytes_per_sector)
			{
				wprintf(L"fatLookupEntry(root): end of chain\n");
				return false;
			}
		}
		else
		{
			cluster = fatGetNextCluster(root, cluster);
			if (IS_EOC_CLUSTER(cluster))
			{
				wprintf(L"fatLookupEntry: end of chain\n");
				return false;
			}
		}
	}

	return false;
}

dword fatFindCluster(fat_file_t* file, qword pos)
{
	fat_root_t *root = (fat_root_t*) file->file.fsd;
	qword ptr;
	dword cluster;

	cluster = (dword) file->entry.first_cluster;
	//wprintf(L"fatFindCluster: first_cluster = 0x%x\n", file->entry.first_cluster);
	ptr = root->bytes_per_cluster;
	while (ptr <= pos)
	{
		if (IS_EOC_CLUSTER(cluster))
			return -1;

		ptr += root->bytes_per_cluster;
		cluster = fatGetNextCluster(root, cluster);
	}

	return cluster;
}

bool fatOpenFile(fat_root_t* root, request_t* req)
{
	wchar_t *ch, component[MAX_PATH];
	const wchar_t* path;
	dword cluster;
	fat_file_t *fd;
	bool is_root;
	fat_dirent_t entry;

	path = req->params.fs_open.name + 1;
	cluster = 0;
	is_root = true;

	while ((ch = wcschr(path, '/')))
	{
		wcsncpy(component, path, ch - path);
		path += wcslen(component) + 1;
		
		//wprintf(L"fatLookupEntry: %s @ %d\n", component, cluster);
		if (!fatLookupEntry(root, cluster, component, is_root, &entry))
		{
			req->result = ENOTFOUND;
			return false;
		}

		is_root = false;
		cluster = entry.first_cluster;
	}

	if (!fatLookupEntry(root, cluster, path, is_root, &entry))
	{
		req->result = ENOTFOUND;
		return false;
	}

	fd = hndAlloc(sizeof(fat_file_t), NULL);
	fd->file.fsd = &root->dev;
	fd->file.pos = 0;
	fd->entry = entry;
	fd->cached_pos = 0;
	fd->cached_cluster = entry.first_cluster;

	req->params.fs_open.fd = &fd->file;
	hndSignal(req->event, true);
	return true;
}

bool fatReadFile(fat_root_t* root, request_t* req)
{
	fat_file_t* file;
	dword cluster, diff;
	qword cluster_pos, user_pos;
	size_t length, this_cluster;
	status_t hr;

	file = (fat_file_t*) req->params.fs_read.fd;

	if (file->file.pos != file->cached_pos)
		file->cached_cluster = fatFindCluster(file, file->file.pos);

	cluster = file->cached_cluster;
	req->user_length = req->params.fs_read.length;
	req->params.fs_read.length = 0;
	this_cluster = 0;

	TRACE1("[%x] ", cluster);
	while (req->params.fs_read.length < req->user_length)
	{
		user_pos = file->file.pos + req->params.fs_read.length;
		user_pos &= -root->bytes_per_cluster;
		diff = file->file.pos + req->params.fs_read.length - user_pos;

		cluster_pos = root->data_start * root->boot_sector.bytes_per_sector + 
			(cluster - 2) * root->bytes_per_cluster + this_cluster +
			diff;
		TRACE1("(%lu) ", (unsigned long) cluster_pos);
		length = min(req->user_length - req->params.fs_read.length,
			root->bytes_per_cluster);
		/*if (length < root->boot_sector.bytes_per_sector)
			length = root->boot_sector.bytes_per_sector;*/

		//wprintf(L"fatReadFile: pos = %d cluster = 0x%x length = %d\n", 
			//(dword) file->file.pos,
			//cluster,
			//length);

		hr = devReadSync(root->disk, 
			cluster_pos,
			(byte*) req->params.fs_read.buffer + req->params.fs_read.length,
			&length);
		
		if (hr || length == 0)
		{
			wprintf(L"fatReadFile: disk read failed at %u\n",
				(unsigned long) cluster_pos);
			req->result = hr;
			file->cached_pos = file->file.pos;
			file->cached_cluster = cluster;
			return false;
		}

		req->params.fs_read.length += length;
		file->file.pos += length;
		
		if (IS_EOC_CLUSTER(cluster))
			break;

		this_cluster += length;
		if (this_cluster >= root->bytes_per_cluster)
		{
			cluster = fatGetNextCluster(root, cluster);
			this_cluster -= root->bytes_per_cluster;
		}

		TRACE2("read %d bytes; next cluster = %x\n", length, cluster);
	}

	//wprintf(L"fat: finished read\n");
	file->cached_pos = file->file.pos;
	file->cached_cluster = cluster;
	hndSignal(req->event, true);
	return true;
}

bool fatRequest(device_t* dev, request_t* req)
{
	fat_root_t* root = (fat_root_t*) dev;
		
	switch (req->code)
	{
	case FS_CLOSE:
		hndFree(req->params.fs_close.fd);

	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case FS_OPEN:
		return fatOpenFile(root, req);

	case FS_READ:
		return fatReadFile(root, req);

	case FS_GETLENGTH:
		{
			fat_file_t *fd = (fat_file_t*) req->params.fs_getlength.fd;
			req->params.fs_getlength.length = fd->entry.file_length;
			hndSignal(req->event, true);
			return true;
		}
	}

	req->code = ENOTIMPL;
	return false;
}

device_t* fatMountFs(driver_t* driver, const wchar_t* path, device_t* dev)
{
	fat_root_t *root;
	size_t length;
	dword RootDirSectors, FatSectors;
	//byte* temp;
	block_size_t size;
	request_t req;

	root = hndAlloc(sizeof(fat_root_t), NULL);
	root->dev.driver = driver;
	root->dev.request = fatRequest;
	root->disk = dev;

	size.total_blocks = 0;
	req.code = BLK_GETSIZE;
	req.params.buffered.buffer = (addr_t) &size;
	req.params.buffered.length = sizeof(size);
	if (devRequestSync(root->disk, &req) != 0 ||
		size.total_blocks > 20740)
	{
		TRACE1("Total blocks = %d, using FAT16\n", size.total_blocks);
		root->fat_bits = 16;
	}
	else
	{
		TRACE1("Total blocks = %d, using FAT12\n", size.total_blocks);
		root->fat_bits = 12;
	}
	
	length = sizeof(fat_bootsector_t);
	if (devReadSync(root->disk, 0, &root->boot_sector, &length) ||
		length < sizeof(fat_bootsector_t))
	{
		hndFree(root);
		return NULL;
	}

	assert(root->boot_sector.sectors_per_fat != 0);
	assert(root->boot_sector.bytes_per_sector != 0);

	root->bytes_per_cluster = root->boot_sector.bytes_per_sector * 
		root->boot_sector.sectors_per_cluster;
	
	root->fat = malloc(root->boot_sector.sectors_per_fat * 
		root->boot_sector.bytes_per_sector);
	assert(root->fat != NULL);

	TRACE2("FAT starts at sector %d = 0x%x\n",
		root->boot_sector.reserved_sectors,
		root->boot_sector.reserved_sectors * 
			root->boot_sector.bytes_per_sector);

	length = root->boot_sector.sectors_per_fat * 
		root->boot_sector.bytes_per_sector;
	if (devReadSync(root->disk, 
		root->boot_sector.reserved_sectors * 
			root->boot_sector.bytes_per_sector,
		root->fat,
		&length))
	{
		free(root->fat);
		hndFree(root);
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

	/*length = root->boot_sector.num_root_entries * 32;
	temp = malloc(length);
	devReadSync(root->disk, root->root_start * root->boot_sector.bytes_per_sector, 
		temp, &length);
	free(temp);*/

	return &root->dev;
}

bool STDCALL drvInit(driver_t* drv)
{
	drv->add_device = NULL;
	drv->mount_fs = fatMountFs;
	return true;
}

//@}