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
	word jmp;
	byte nop;
	byte oem_name[8];
	word bytes_per_sector;
	byte sectors_per_cluster;
	word reserved_sectors;
	byte num_fats;
	word num_root_entries;
	word sectors;
	byte media_descriptor;
	word sectors_per_fat;
	word sectors_per_track;
	word num_heads;
	dword hidden_sectors;
	dword total_sectors;
	byte drive;
	byte reserved2;
	byte boot_sig;
	dword serial_number;
	byte volume[11];
	byte system[8];
	byte code[450];
};

typedef struct fat_dirent_t fat_dirent_t;
struct fat_dirent_t
{
	byte name[8];
	byte extension[3];
	byte attribs;
	byte reserved[10];
	word write_time;
	word write_date;
	word first_cluster;
	dword file_length;
};

typedef struct fat_lfnslot_t fat_lfnslot_t;
struct fat_lfnslot_t
{
	byte id;				// sequence number for slot 
	wchar_t name0_4[5]; 	// first 5 characters in name 
	byte attrivs;			// attribute byte
	byte reserved;			// always 0 
	byte alias_checksum;	// checksum for 8.3 alias 
	wchar_t name5_10[6];	// 6 more characters in name
	word first_cluster; 	// starting cluster number
	wchar_t name11_12[2];	// last 2 characters in name
};

#pragma pack(pop)

#define ATTR_LONG_NAME     \
	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

//@}

#endif