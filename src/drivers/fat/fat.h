/* $Id: fat.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

#ifndef __FAT_H
#define __FAT_H

/*!
 *  \ingroup	drivers
 *  \defgroup	fat FAT file system
 *  @{
 */

#pragma pack(push, 1)

typedef struct fat_bootsector_t fat_bootsector_t;
struct fat_bootsector_t
{
	uint16_t jmp;
	uint8_t nop;
	uint8_t oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t num_fats;
	uint16_t num_root_entries;
	uint16_t sectors;
	uint8_t media_descriptor;
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors;
	uint8_t drive;
	uint8_t reserved2;
	uint8_t boot_sig;
	uint32_t serial_number;
	uint8_t volume[11];
	uint8_t system[8];
	uint8_t code[450];
};

typedef struct fat_dirent_t fat_dirent_t;
struct fat_dirent_t
{
	uint8_t name[8];
	uint8_t extension[3];
	uint8_t attribs;
	uint8_t reserved[10];
	uint16_t write_time;
	uint16_t write_date;
	uint16_t first_cluster;
	uint32_t file_length;
};

typedef struct fat_lfnslot_t fat_lfnslot_t;
struct fat_lfnslot_t
{
	uint8_t slot;			/* sequence number for slot */
	wchar_t name0_4[5]; 	/* first 5 characters in name */
	uint8_t attribs;		/* attribute byte */
	uint8_t reserved;		/* always 0 */
	uint8_t alias_checksum;	/* checksum for 8.3 alias */
	wchar_t name5_10[6];	/* 6 more characters in name */
	uint16_t first_cluster; /* starting cluster number */
	wchar_t name11_12[2];	/* last 2 characters in name */
};

#pragma pack(pop)

#define ATTR_LONG_NAME     \
	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

/*@}*/

#endif