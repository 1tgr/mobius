#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>
#include <os/fs.h>

#define DEBUG
#include <kernel/debug.h>

#include <errno.h>
#include <wchar.h>
#include <ctype.h>

#include "fat.h"

typedef struct fat_dir_t fat_dir_t;
struct fat_dir_t
{
	file_t file;
	fat_dirent_t di;
	unsigned num_entries;
	/* fat_dirent_t entries[]; */
};

typedef struct fat_file_t fat_file_t;
struct fat_file_t
{
	file_t file;
	fat_dirent_t di;
	unsigned num_clusters;
	/* uint32_t clusters[]; */
};

typedef struct fat_root_t fat_root_t;
struct fat_root_t
{
	device_t dev;
	device_t *device;
	fat_bootsector_t bpb;
	fat_dir_t *root;
	unsigned fat_bits;
	unsigned bytes_per_cluster;
	unsigned root_start;
	unsigned data_start;
	unsigned cluster_shift;
	uint8_t *fat;
};

#define SECTOR_SIZE			512

CASSERT(sizeof(fat_bootsector_t) == SECTOR_SIZE);
CASSERT(sizeof(fat_dirent_t) == 16);
CASSERT(sizeof(fat_dirent_t) == sizeof(fat_lfnslot_t));

#define FAT_MAX_NAME		256

#define FAT_CLUSTER_ROOT			0

#define FAT_CLUSTER_AVAILABLE		0
#define FAT_CLUSTER_RESERVED_START	0xfff0
#define FAT_CLUSTER_RESERVED_END	0xfff6
#define FAT_CLUSTER_BAD				0xfff7
#define FAT_CLUSTER_EOC_START		0xfff8
#define FAT_CLUSTER_EOC_END			0xffff

#define IS_EOC_CLUSTER(c)		((c) >= FAT_CLUSTER_EOC_START && (c) <= FAT_CLUSTER_EOC_END)
#define IS_RESERVED_CLUSTER(c)	((c) >= FAT_CLUSTER_RESERVED_START && (c) <= FAT_CLUSTER_RESERVED_END)

fat_dir_t *FatAllocDir(fat_root_t *root, const fat_dirent_t *di)
{
	fat_dir_t *dir;
	size_t size;

	size = (di->file_length + SECTOR_SIZE - 1) & -SECTOR_SIZE;
	dir = malloc(sizeof(fat_dir_t) + size);
	if (dir == NULL)
		return NULL;

	dir->file.fsd = &root->dev;
	dir->file.pos = 0;
	dir->di = *di;
	dir->num_entries = size / sizeof(fat_dirent_t);

	if (di->first_cluster == FAT_CLUSTER_ROOT)
		DevRead(root->device, root->root_start * SECTOR_SIZE, dir + 1, size);
	else
		assert(false);

	return dir;
}

uint32_t FatGetNextCluster(fat_root_t* root, uint32_t cluster)
{
	uint32_t FATOffset;
	uint16_t w;
	uint8_t* b;

	if (cluster >=
		root->bpb.sectors / root->bpb.sectors_per_cluster)
	{
		wprintf(L"Cluster 0x%x (%d) beyond than total clusters 0x%x (%d)\n",
			cluster, cluster,
			root->bpb.sectors / root->bpb.sectors_per_cluster,
			root->bpb.sectors / root->bpb.sectors_per_cluster);
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

handle_t FatAllocFile(fat_root_t *root, const fat_dirent_t *di, uint32_t flags)
{
	fat_file_t *file;
	handle_t fd;
	unsigned num_clusters, i, cluster;
	uint32_t *ptr;

	num_clusters = 
		(di->file_length + root->bytes_per_cluster - 1) & -root->bytes_per_cluster;
	num_clusters /= root->bytes_per_cluster;
	fd = HndAlloc(NULL, sizeof(fat_dir_t) + num_clusters * sizeof(uint32_t), 'file');
	file = HndLock(NULL, fd, 'file');
	if (file == NULL)
	{
		HndFree(NULL, fd, 'file');
		return NULL;
	}

	file->file.fsd = &root->dev;
	file->file.pos = 0;
	file->file.flags = flags;
	file->di = *di;
	file->num_clusters = num_clusters;

	ptr = (uint32_t*) (file + 1);
	cluster = di->first_cluster;
	for (i = 0; i < num_clusters; i++)
	{
		*ptr++ = cluster;
		cluster = FatGetNextCluster(root, cluster);
	}

	assert(IS_EOC_CLUSTER(cluster));
	HndUnlock(NULL, fd, 'file');
	return fd;
}

unsigned FatAssembleName(fat_dirent_t *entries, wchar_t *name)
{
	fat_lfnslot_t *lfn;
	lfn = (fat_lfnslot_t*) entries;

	if ((entries[0].attribs & ATTR_LONG_NAME) == ATTR_LONG_NAME)
	{
		assert(false);
		return 1;
	}
	else
	{
		unsigned i;
		wchar_t *ptr;

		memset(name, 0, sizeof(*name) * FAT_MAX_NAME);
		ptr = name;
		for (i = 0; i < _countof(entries[0].name); i++)
		{
			if (entries[0].name[i] == ' ')
				break;
			
			*ptr++ = tolower(entries[0].name[i]);
		}

		if (entries[0].extension[0] != ' ')
		{
			*ptr++ = '.';

			for (i = 0; i < _countof(entries[0].extension); i++)
			{
				if (entries[0].extension[i] == ' ')
				{
					*ptr = '\0';
					break;
				}

				*ptr++ = tolower(entries[0].extension[i]);
			}
		}

		*ptr = '\0';
		return 1;
	}
}

bool FatLookupFile(fat_root_t *root, fat_dir_t *dir, 
				   wchar_t *path, fat_dirent_t *di)
{
	wchar_t *slash, name[FAT_MAX_NAME];
	unsigned i, count;
	fat_dirent_t *entries;
	bool find_dir;

	slash = wcschr(path, '/');
	if (slash)
	{
		*slash = '\0';
		find_dir = true;
	}
	else
		find_dir = false;
	
	i = 0;
	entries = ((fat_dirent_t*) (dir + 1));
	while (i < dir->num_entries && entries[i].name[0] != '\0')
	{
		if (entries[i].name[0] == 0xe5 ||
			entries[i].attribs & ATTR_LONG_NAME)
		{
			i++;
			continue;
		}

		count = FatAssembleName(entries + i, name);
		/*TRACE2("%s %s", path, name);*/

		if (_wcsicmp(name, path) == 0)
		{
			TRACE2("%s found at %u", name, entries[i].first_cluster);

			if (find_dir)
			{
				TRACE0("\n");
				dir = FatAllocDir(root, entries + i);
				if (FatLookupFile(root, dir, slash + 1, di))
				{
					free(dir);
					return true;
				}
				else
				{
					free(dir);
					return false;
				}
			}
			else
			{
				TRACE0(" finished\n");
				*di = entries[i];
				return true;
			}
		}

		i += count;
	}

	TRACE1("fat: nothing found for %s\n", path);
	return false;
}

uint32_t FatClusterToOffset(fat_root_t *root, uint32_t cluster)
{
	return root->data_start * root->bpb.bytes_per_sector 
		+ (cluster - 2) * root->bytes_per_cluster;
}

bool FatRead(fat_root_t *root, request_fs_t *req_fs)
{
	fat_file_t *file;
	unsigned cluster_index, user_length, bytes, mod;
	uint32_t *ptr;
	uint8_t *buf;
	bool ret;
	
	file = HndLock(NULL, req_fs->params.fs_read.file, 'file');
	if (file == NULL)
		return false;

	cluster_index = file->file.pos >> root->cluster_shift;
	if (cluster_index >= file->num_clusters)
		return false;

	/* xxx - change this to 64-bit modulo */
	mod = (uint32_t) file->file.pos % root->bytes_per_cluster;

	ptr = (uint32_t*) (file + 1);
	user_length = req_fs->params.fs_read.length;
	req_fs->params.fs_read.length = 0;
	ret = true;
	buf = (uint8_t*) req_fs->params.fs_read.buffer;

	wprintf(L"fat: reading %u bytes\n", user_length);
	while (req_fs->params.fs_read.length < user_length)
	{
		if (file->file.pos >= file->di.file_length)
		{
			/* attempt to read past end of file */
			wprintf(L"fat: attempt to read past end of file\n");
			ret = false;
			break;
		}

		bytes = user_length - req_fs->params.fs_read.length;
		if (bytes > root->bytes_per_cluster)
			bytes = root->bytes_per_cluster;
		wprintf(L"fat: pos = %u cluster = %u = %x bytes = %u\n", 
			(uint32_t) file->file.pos, cluster_index, 
			ptr[cluster_index], bytes);

		bytes = DevRead(root->device, 
			FatClusterToOffset(root, ptr[cluster_index]) + mod,
			buf,
			bytes);
		if (bytes == 0)
		{
			/* device failure */
			wprintf(L"fat: device failure\n");
			ret = false;
			break;
		}

		mod += bytes;
		if (mod >= root->bytes_per_cluster)
		{
			mod -= root->bytes_per_cluster;

			cluster_index++;
			if (cluster_index >= file->num_clusters)
			{
				/* last cluster reached: invalid chain? */
				wprintf(L"fat: last cluster reached\n");
				ret = false;
				break;
			}
		}

		file->file.pos += bytes;
		buf += bytes;
		req_fs->params.fs_read.length += bytes;
	}

	HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
	return ret;
}

bool FatRequest(device_t *dev, request_t *req)
{
	request_fs_t *req_fs;
	wchar_t *path;
	fat_root_t *root;
	fat_dirent_t di;
	
	req_fs = (request_fs_t*) req;
	root = (fat_root_t*) dev;
	switch (req->code)
	{
	case FS_OPEN:
		path = _wcsdup(req_fs->params.fs_open.name);
		if (!FatLookupFile(root, root->root, path + 1, &di))
		{
			req->code = ENOTFOUND;
			return false;
		}

		TRACE2("fat: opening %s at cluster %u\n", path, di.first_cluster);
		free(path);

		req_fs->params.fs_open.file = FatAllocFile(root, &di, 
			req_fs->params.fs_open.flags);
		return req_fs->params.fs_open.file == NULL ? false : true;

	case FS_READ:
		return FatRead(root, req_fs);
	}

	req->code = ENOTIMPL;
	return false;
}

device_t *FatMountFs(driver_t *drv, const wchar_t *path, device_t *dev)
{
	fat_root_t *root;
	fat_dirent_t root_entry;
	unsigned length, temp;
	uint32_t RootDirSectors, FatSectors;

	root = malloc(sizeof(fat_root_t));
	if (root == NULL)
		return NULL;

	memset(root, 0, sizeof(fat_root_t));
	root->dev.driver = drv;
	root->dev.request = FatRequest;
	root->device = dev;

	if (DevRead(dev, 0, &root->bpb, sizeof(root->bpb)) < sizeof(root->bpb))
	{
		free(root);
		return NULL;
	}

	root->fat_bits = 12;
	root->bytes_per_cluster = 
		root->bpb.bytes_per_sector * root->bpb.sectors_per_cluster;
	temp = root->bytes_per_cluster;
	for (root->cluster_shift = 0; (temp & 1) == 0; root->cluster_shift++)
		temp >>= 1;
	
	TRACE1("\tAllocating %u bytes for FAT\n",
		root->bpb.sectors_per_fat * 
		root->bpb.bytes_per_sector);
	root->fat = malloc(root->bpb.sectors_per_fat * root->bpb.bytes_per_sector);
	assert(root->fat != NULL);

	TRACE2("\tFAT starts at sector %d = 0x%x\n",
		root->bpb.reserved_sectors,
		root->bpb.reserved_sectors * 
			root->bpb.bytes_per_sector);

	length = root->bpb.sectors_per_fat * 
		root->bpb.bytes_per_sector;
	TRACE0("\tReading FAT\n");
	if (DevRead(root->device, 
		root->bpb.reserved_sectors * 
			root->bpb.bytes_per_sector,
		root->fat,
		length) != length)
	{
		free(root->fat);
		free(root);
		return NULL;
	}

	RootDirSectors = (root->bpb.num_root_entries * 32) /
		root->bpb.bytes_per_sector;
	FatSectors = root->bpb.num_fats * root->bpb.sectors_per_fat;
	root->data_start = root->bpb.reserved_sectors + 
		FatSectors + 
		RootDirSectors;

	root->root_start = root->bpb.reserved_sectors + 
		root->bpb.hidden_sectors + 
		root->bpb.sectors_per_fat * root->bpb.num_fats;

	memset(&root_entry, 0, sizeof(root_entry));
	root_entry.file_length = root->bpb.num_root_entries * sizeof(fat_dirent_t);
	root_entry.first_cluster = FAT_CLUSTER_ROOT;
	root->root = FatAllocDir(root, &root_entry);
	if (root->root == NULL)
	{
		free(root);
		return NULL;
	}
	
	return &root->dev;
}

bool DrvInit(driver_t *drv)
{
	drv->add_device = NULL;
	drv->mount_fs = FatMountFs;
	return true;
}