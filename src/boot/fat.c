#include <bios.h>
#include <ctype.h>
#include <string.h>
#include "mobel_pe.h"

bool BootDiskReadSector(const device_t *dev, unsigned long sector, void *buf)
{
	unsigned short temp, tries, c, h, s;
	
	sector += dev->first_sector;
	if (sector == 0)
	{
		s = 1;
		h = 0;
		c = 0;
	}
	else
	{
		s = sector % dev->sects + 1;
		h = (sector / dev->sects) % dev->heads;
		c = (sector / dev->sects) / dev->heads;
	}

	for (tries = 3; tries != 0; tries--)
	{
		temp = biosdisk(DISK_READ, dev->id, h, c, s, 1, buf);
		if (temp == 0 || temp == 0x11)
			return true;
		biosdisk(DISK_RESET, dev->id, 0, 0, 0, 0, 0);
	}

	return false;
}

unsigned int BootFatReadCluster(const device_t *dev, unsigned long cluster, void *buf)
{
	unsigned long sector;
	unsigned int n;

	if (cluster <= 2)
		sector = dev->bpb.reserved_sectors + 
			dev->bpb.hidden_sectors + 
			dev->bpb.sectors_per_fat * dev->bpb.num_fats +
			cluster;
	else
		sector = dev->data_start + (cluster - 2) * dev->bpb.sectors_per_cluster;

	for (n = 0; n < dev->bpb.sectors_per_cluster; n++)
		if (!BootDiskReadSector(dev, sector + n, 
			(char*) buf + n * dev->bpb.bytes_per_sector))
			break;

	return n;
}

unsigned long BootFatGetNextCluster(const device_t *dev, unsigned long cluster)
{
	/*if (cluster == FAT_CLUSTER_ROOT)*/
		return cluster + 1;
}

void BootFatAssembleName(const fat_dirent_t *entries, char *name)
{
	unsigned i;
	char *ptr;

	memset(name, 0, FAT_MAX_NAME);
	ptr = name;
	for (i = 0; i < countof(entries->name); i++)
	{
		if (entries->name[i] == ' ')
			break;
		
		*ptr++ = tolower(entries->name[i]);
	}

	if (entries->extension[0] != ' ')
	{
		*ptr++ = '.';

		for (i = 0; i < countof(entries->extension); i++)
		{
			if (entries->extension[i] == ' ')
			{
				*ptr = '\0';
				break;
			}

			*ptr++ = tolower(entries->extension[i]);
		}
	}

	*ptr = '\0';
}

bool BootFatLookupFile(const device_t *dev, unsigned long dir, 
					   const char *name, fat_dirent_t *di)
{
	static fat_dirent_t entries[512 / sizeof(fat_dirent_t)];
	char filename[FAT_MAX_NAME];
	int i;

	while (!IS_EOC_CLUSTER(dir))
	{
		if (BootFatReadCluster(dev, dir, entries) == 0)
			return false;

		for (i = 0; i < countof(entries) && entries[i].name[0]; i++)
		{
			if ((entries[i].attribs & FILE_ATTR_LONG_NAME) ||
				entries[i].name[0] == 0xe5)
				continue;

			BootFatAssembleName(entries + i, filename);
			if (stricmp(filename, name) == 0)
			{
				memcpy(di, entries + i, sizeof(*di));
				return true;
			}
		}

		if (i < countof(entries) && entries[i].name[0] == '\0')
			break;

		dir = BootFatGetNextCluster(dev, dir);
	}

	return false;
}

bool BootFatInitDevice(unsigned char id, device_t *dev)
{
	unsigned int RootDirSectors, FatSectors;
	
	dev->id = id;
	if (dev->id < 0x80)
	{
		dev->type = fat12;
		dev->first_sector = 0;
	}
	else
	{
		/* xxx - need to account for fat32 */
		dev->type = fat16;
		dev->first_sector = 63;
		dev->sects = 63;
		dev->heads = 255;
	}

	if (!BootDiskReadSector(dev, 0, &dev->bpb))
		return false;

	dev->sects = dev->bpb.sectors_per_track;
	dev->heads = dev->bpb.num_heads;
	dev->bytes_per_cluster = dev->bpb.bytes_per_sector * dev->bpb.sectors_per_cluster;
	cputld(dev->sects);
	writechar('/');
	cputld(dev->heads);
	cputs("\n");
	
	RootDirSectors = (dev->bpb.num_root_entries * 32) /
		dev->bpb.bytes_per_sector;
	FatSectors = dev->bpb.num_fats * dev->bpb.sectors_per_fat;
	dev->data_start = dev->bpb.reserved_sectors + 
		FatSectors + 
		RootDirSectors;

	return true;
}
