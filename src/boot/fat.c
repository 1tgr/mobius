/* $Id: fat.c,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

/*///////////////////////////////////////////////////////////////////////////
	FAT12/16 FILESYSTEM
////////////////////////////////////////////////////////////////////////////*/
/* NULL, memset, memcmp, memcpy, strchr, strlen, strcpy, strupr, strchr */
#include <string.h>
#include "mobel.h"

#define	min(a,b)	(((a) < (b)) ? (a) : (b))
#define BPS			512

/* bytes per FAT directory entry */
#define	FAT_DIRENT_LEN		32
/* FAT entries 0 and 1 are reserved: */
#define	MAGIC_FAT_CLUSTER	0
#define	MAGIC_ROOT_CLUSTER	1

typedef enum
{
	FAT12, FAT16, FAT32
} fat_type_t;

typedef struct
{
	unsigned short max_cluster;
	unsigned char sectors_per_cluster;
	sector_t fat_start, root_start, data_start;
	fat_type_t fat_type;
} fat_t;
/*****************************************************************************
e.g. "foo.i" -> "FOO     I  "
'dst' must be >=12 bytes
*****************************************************************************/
static int fat_convert_name_to_fat(char *dst, char *src)
{
	unsigned len;
	char *dot;

/* put src in FAT format */
	memset(dst, ' ', 11);
	dst[11] = '\0';
	dot = strchr(src, '.');

/* xxx - why? trouble with Turbo C 2.0 installation? */
#define NULL 0

/* there is an extension */
	if(dot != NULL)
	{
/* long filenames not supported */
		len = dot - src;
		if(len > 8)
			return ERR_FILESYSTEM;
/* copy filename */
		memcpy(dst, src, len);
		dst[len] = '\0';
/* long extension not supported */
		len = strlen(dot) - 1;
		if(len > 3)
			return ERR_FILESYSTEM;
/* copy extension */
		memcpy(dst + 8, dot + 1, len);
	}
/* no extension */
	else
	{
/* long filenames not supported */
		len = strlen(src);
		if(len > 8)
			return ERR_FILESYSTEM;
/* copy filename */
		strcpy(dst, src);
	}
/* make it upper case */
	strupr(dst);
	return 0;
}
/*****************************************************************************
e.g. "README  TXT" -> "readme.txt"
'dst' must be >=12 bytes
*****************************************************************************/
static void fat_convert_name_from_fat(char *dst, char *src)
{
	unsigned char i;
	char *d = dst;

	for(i = 0; i < 8; i++)
	{
		if(src[i] == ' ')
			break;
		*d = src[i];
		d++;
	}
	if(src[8] != ' ')
	{
		*d = '.';
		d++;
		for(i = 8; i < 11; i++)
		{
			if(src[i] == ' ')
				break;
			*d = src[i];
			d++;
		}
	}
	*d = '\0';
/* make it lower case */
	strlwr(dst);
}
/*****************************************************************************
*****************************************************************************/
static int fat_open_root(file_t *root_dir, mount_t *mount)
{
	root_dir->pos = 0;
	root_dir->mount = mount;
	root_dir->inode = MAGIC_ROOT_CLUSTER;
	root_dir->file_size = -1uL; /* xxx - get root dir size */
	root_dir->is_dir = 1;
	strcpy(root_dir->name, "/");
	return 0;
}
/*****************************************************************************
FAT file attribute bits:
Bit(s)	Description	(Table 01401)
 0	read-only
 1	hidden
 2	system
 3	volume label or LFN
 4	directory
 5	archive
 6	?
 7	if set, file is shareable under Novell NetWare
*****************************************************************************/
static int fat_readdir(file_t *dir, file_t *file)
{
	char dirent[FAT_DIRENT_LEN];
	int err;

	while(1)
	{
/* read one 32-byte FAT directory entry */
		err = my_read(dir, dirent, FAT_DIRENT_LEN);
		if(err != FAT_DIRENT_LEN)
		{
/* short read means end of dir */
			if(err >= 0)
				return ERR_EOF;
			return err;
		}
/* "virgin" dir entry means end of dir, for both root dir and subdir */
		if(dirent[0] == 0)
			return ERR_EOF;
/* deleted file */
		if(dirent[0] == 5 || dirent[0] == '\xE5')
			continue;
/* volume label or LFN */
		if((dirent[11] & 0x08) == 0)
			break;
	}
/* found it! */
	file->pos = 0;
	file->mount = dir->mount;
	file->inode = read_le16(dirent + 26);
	file->file_size = read_le32(dirent + 28);
	if(dirent[11] & 0x10)
		file->is_dir = 1;
	else
		file->is_dir = 0;
	fat_convert_name_from_fat(file->name, dirent);
/* inode==0 is actually a pointer to the root directory */
	if(file->inode == 0)
	{
		if(file->is_dir)
			file->inode = MAGIC_ROOT_CLUSTER;
/* ...but only for directories */
		else
			return -1;
	}
/* xxx - corrupt filesystem; not software error */
	else if(file->inode < 2)
		return ERR_SW;
/* I guess FAT doesn't store the correct size of subdirectories
in the directory entry */
	if(file->file_size == 0)
	{
		if(file->is_dir)
			file->file_size = -1uL;
/* else it's a zero-length file, which is cool */
	}
	return 0;
}
/*****************************************************************************
convert 'cluster' to 'sector', then advance to next cluster in FAT chain
and store next cluster at 'cluster'
*****************************************************************************/
static int fat_walk(file_t *file, sector_t *sector, unsigned short *cluster)
{
	unsigned char buf[2];
	unsigned short entry;
	file_t the_fat;
	sector_t temp;
	fat_t *fat;
	int err;

/* xxx - init the_fat properly */
	fat = (fat_t *)(file->mount->fsinfo.info);
	the_fat.mount = file->mount;
	the_fat.inode = MAGIC_FAT_CLUSTER;
	the_fat.file_size = -1uL;
/* must be cluster within data area of disk */
	temp = (*cluster);
	if(temp < 2)
		return ERR_SW; /* can't (shouldn't) happen */
	temp -= 2;
/* convert cluster to sector */
	temp *= fat->sectors_per_cluster;
	temp += fat->data_start;
	(*sector) = temp;
/* now convert cluster to byte offset into FAT */
	temp = (*cluster);
	if(fat->fat_type == FAT12)
	{
/* 12 bits per FAT entry, so byte offset into FAT is 3/2 * temp */
		temp *= 3;
		the_fat.pos = temp >> 1;
/* read 2-byte entry */
		err = my_read(&the_fat, buf, 2);
		if(err < 0)
			return err;
		entry = read_le16(buf);
/* top 12 bits or bottom 12 bits? */
		if(temp & 1)
			entry >>= 4;
		else
			entry &= 0x0FFF;
	}
	else if(fat->fat_type == FAT16)
	{
/* 16 bits per FAT entry */
		temp <<= 1;
		the_fat.pos = temp;
/* read 2-byte entry */
		err = my_read(&the_fat, buf, 2);
		if(err < 0)
			return err;
		entry = read_le16(buf);
	}
	else
	{
		cprintf("fat_walk: FAT32 not yet supported\n\r");
		return ERR_SW;
	}
/* that's what we want! */
	(*cluster) = entry;
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int fat_read_sector(file_t *file, sector_t rel_sector,
		unsigned char **blk)
{
	unsigned short rel_cluster, abs_cluster;
	sector_t abs_sector;
	fsinfo_t *fsinfo;
	mount_t *mount;
	dev_t *dev;
	fat_t *fat;
	int err;

	mount = file->mount;
	dev = mount->dev;
	fsinfo = &mount->fsinfo;
	fat = (fat_t *)fsinfo->info;
	abs_cluster = file->inode;
/* starting cluster == 0: read from FAT */
	if(abs_cluster == MAGIC_FAT_CLUSTER)
	{
		abs_sector = fat->fat_start + rel_sector;
		if(abs_sector >= fat->root_start)
			return ERR_EOF;
	}
/* starting cluster == 1: read from root directory */
	else if(abs_cluster == MAGIC_ROOT_CLUSTER)
	{
		abs_sector = fat->root_start + rel_sector;
		if(abs_sector >= fat->data_start)
			return ERR_EOF;
	}
/* starting cluster >= 2: normal read from data area of disk */
	else
	{
/* cluster within file */
		rel_cluster = rel_sector / fat->sectors_per_cluster;
/* sector within cluster */
		rel_sector %= fat->sectors_per_cluster;
		if (rel_cluster == file->cur_inode &&
			file->cur_sector != -1)
			abs_sector = file->cur_sector;
		else
		{
/* chase clusters, so we go from relative cluster (cluster-within-file)... */
			for(;;)
			{
				if(abs_cluster > fat->max_cluster)
					return ERR_EOF;
				err = fat_walk(file, &abs_sector, &abs_cluster);
				if(err != 0)
					return err;
				if(rel_cluster == 0)
					break;
				rel_cluster--;
			}

			/* ...to absolute cluster (cluster-within-disk). fat_walk() also
converted cluster to sector, so add things together to get the
absolute sector number (finally!) */
			abs_sector += rel_sector;

			file->cur_inode = rel_cluster;
			file->cur_sector = abs_sector;
		}
	}
/* load it */
	return read_sector(dev, mount->partition_start + abs_sector, blk);
}
/*****************************************************************************
*****************************************************************************/
static int fat_read(file_t *file, unsigned char *buf, unsigned want)
{
	unsigned short byte_in_sector;
	sector_t rel_sector;
	unsigned got, count;
	unsigned char *blk;
	mount_t *mount;
	dev_t *dev;
	int err;

	if(want == 0)
		return 0;
	mount = file->mount;
	dev = mount->dev;
	count = 0;
	do
	{
		rel_sector = file->pos;
/* byte within sector */
		byte_in_sector = rel_sector % dev->bytes_per_sector;
/* sector within file */
		rel_sector /= dev->bytes_per_sector;
/* read the sector */
		err = fat_read_sector(file, rel_sector, &blk);
		if(err < 0)
			return err;
/* how many bytes can we read from this sector? */
		got = dev->bytes_per_sector - byte_in_sector;
/* nearing end of file? */
		if(file->pos + got > file->file_size)
			got = file->file_size - file->pos;
		if(got == 0)
			break;
/* how many will we actually read from it? */
		got = min(got, want);
/* read them */
		memcpy(buf, blk + byte_in_sector, got);
/* done with this sector
maybe I will need a function like this later?
		uncache_sector(file, sector, blk); */
/* advance pointers */
		file->pos += got;
		buf += got;
		want -= got;
		count += got;
/* done? */
	} while(want != 0);
	return count;
}
/*****************************************************************************
*****************************************************************************/
/* #include <stdlib.h> malloc() */

int fat_mount(mount_t *mount, dev_t *dev, unsigned char part)
{
	unsigned char *blk, *ptab_rec; /* partition table record */
	unsigned long total_sectors;
	fat_type_t fat_type;
	fsinfo_t *fsinfo;
	fat_t *fat;
	int err;

	mount->curr_dir = MAGIC_ROOT_CLUSTER;
	dev->bytes_per_sector = BPS;
/* floppy */
	if(dev->int13_dev < 0x80)
	{
		fat_type = FAT12;
/* read sector 0 of floppy (boot sector) */
		mount->partition_start = 0;
		err = read_sector(dev, mount->partition_start, &blk);
		if(err != 0)
			return err;
/* make sure it's a FAT disk */
		if((blk[0] != 0xEB && blk[0] != 0xE9) || /* JMP (SHORT) */
			read_le16(blk + 0x0B) != 512 || /* bytes/sector */
			blk[0x0D] != 1 ||		/* sectors/cluster */
			blk[0x15] != 0xF0 ||		/* media ID */
			blk[0x1A] != 2)			/* heads */
		{
NOT:		cprintf("Partition %u on drive 0x%02X is "
				"not a FAT12 or FAT16 partition\n\r",
				part, dev->int13_dev);
			cprintf("JMP = %02x\n"
				"Bytes per sector = %d\n"
				"Sectors per cluster = %d\n"
				"Media ID = %02x\n"
				"Heads = %d\n",
				blk[0], read_le16(blk + 0x0B),
				blk[0x0D], blk[0x15], blk[0x1A]);
			return -1;
		}
/* read disk geometry from BIOS Parameter Block (BPB) of FAT boot sector */
		dev->sects = read_le16(blk + 0x18);
		dev->heads = read_le16(blk + 0x1A);
	}
/* hard disk */
	else
	{
		if(part > 3)
		{
			cprintf("fat_mount: partition number must be 0-3\n\r");
			return -1;
		}
/* read sector 0 of hard disk (MBR; partition table) */
		err = read_sector(dev, 0, &blk);
		if(err != 0)
			return err;
/* xxx - finish initializing dev */
dev->sects = 63;
dev->heads = 255;
/* point to 16-byte partition table record */
		ptab_rec = blk + 446 + 16 * part;
		switch(ptab_rec[4])
		{
			case 1:
				fat_type = FAT12;
				break;
			case 4:
				fat_type = FAT16; /* up to 32 meg */
				break;
			case 6:
				fat_type = FAT16; /* DOS 3.31+, >32 meg */
				break;
			default:
				goto NOT;
		}
		mount->partition_start = read_le32(ptab_rec + 8);
/* read sector 0 of partition (boot sector) */
		err = read_sector(dev, mount->partition_start, &blk);
		if(err != 0)
			return err;
/* make sure it's a FAT disk */
		if((blk[0] != 0xEB && blk[0] != 0xE9) || /* JMP (SHORT) */
			read_le16(blk + 0x0B) != 512)	/* bytes/sector */
				goto NOT;
	}
/*	fat = malloc(sizeof(fat_t));
	if(fat == NULL)
		return ERR_MEM; */
	mount->dev = dev;
/* init mount->fsinfo */
	fsinfo = &mount->fsinfo;
fat = (fat_t *)fsinfo->info;

	fsinfo->open_root = fat_open_root;
	fsinfo->readdir = fat_readdir;
	fsinfo->read = fat_read;
/* init mount->fsinfo->info
	fsinfo->info = fat; */
memcpy(fsinfo->info, fat, sizeof(fat));
	fat = (fat_t *)fsinfo->info;
	fat->sectors_per_cluster = blk[13];
	fat->fat_start = read_le16(blk + 14);	/* reserved_sectors */
	fat->root_start = fat->fat_start +
		blk[16] *			/* num_fats */
		read_le16(blk + 22);		/* sectors_per_fat */
	fat->data_start = fat->root_start +
		(FAT_DIRENT_LEN *		/* bytes_per_dir_ent */
		read_le16(blk + 17)) /		/* num_root_dir_ents */
			BPS;			/* bytes_per_sector */
	total_sectors = read_le16(blk + 19);
	if(total_sectors == 0)
		total_sectors = read_le32(blk + 32);
	fat->max_cluster = total_sectors /
		fat->sectors_per_cluster -
		fat->data_start - 1;
	fat->fat_type = fat_type;
#if 0
cprintf("disk CHS=??:%u:%u\n\r", dev->heads, dev->sects);

cprintf("drive 0x%02X, partition %u: %s partition starting at sector %lu\n\r",
dev->int13_dev, part, fat->fat_type == FAT12 ? "FAT12" : "FAT16",
mount->partition_start);

cprintf("%u sector(s)/cluster, ", fat->sectors_per_cluster);
cprintf("%u FATs, ", blk[0x10]);
cprintf("root at sector %lu, ", fat->root_start);
cprintf("data at sector %lu\n\r", fat->data_start);

cprintf("FAT(s) at sector %lu, ", fat->fat_start);
cprintf("%u entries in root dir, ", read_le16(blk + 0x11));
cprintf("%lu total sectors ", total_sectors);
if(total_sectors >= 16384)
	cprintf("(%luM)\n\r\n\r", total_sectors / 2048);
else
	cprintf("(%luK)\n\r\n\r", total_sectors / 2);
#endif
	return 0;
}