#ifndef MOBEL_PE_H__
#define MOBEL_PE_H__

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned short wchar_t;

typedef char bool;
#define false	0
#define true	1

#define countof(a)	(sizeof(a) / sizeof((a)[0]))

extern char boot_buf[64];

void writechar(char ch);
int cputs(const char *str);
void cputl(unsigned long num);
void cputld(unsigned long num);
char readkey(void);
void readline(char *buf, int max);

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

#define FILE_ATTR_READ_ONLY 	0x01
#define FILE_ATTR_HIDDEN		0x02
#define FILE_ATTR_SYSTEM		0x04
#define FILE_ATTR_VOLUME_ID 	0x08
#define FILE_ATTR_DIRECTORY 	0x10
#define FILE_ATTR_ARCHIVE		0x20

#define FILE_ATTR_LONG_NAME     \
	(FILE_ATTR_READ_ONLY | FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM | FILE_ATTR_VOLUME_ID)

#define FAT_CLUSTER_ROOT			0

#define FAT_CLUSTER_AVAILABLE		0
#define FAT_CLUSTER_RESERVED_START	0xfff0
#define FAT_CLUSTER_RESERVED_END	0xfff6
#define FAT_CLUSTER_BAD				0xfff7
#define FAT_CLUSTER_EOC_START		0xfff8
#define FAT_CLUSTER_EOC_END			0xffff

#define IS_EOC_CLUSTER(c) \
	((c) >= FAT_CLUSTER_EOC_START && (c) <= FAT_CLUSTER_EOC_END)
#define IS_RESERVED_CLUSTER(c) \
	((c) >= FAT_CLUSTER_RESERVED_START && (c) <= FAT_CLUSTER_RESERVED_END)

#define FAT_MAX_NAME				(8 + 1 + 3 + 1)

#define DISK_READ	2
#define DISK_RESET	0

typedef struct device_t device_t;
struct device_t
{
	fat_bootsector_t bpb;
	unsigned char id;
	unsigned int sects;
	unsigned int heads;
	enum { fat12, fat16, fat32 } type;
	unsigned long first_sector;
	unsigned long data_start;
	unsigned long bytes_per_cluster;
};

bool BootFatInitDevice(unsigned char id, device_t *dev);
bool BootFatLookupFile(const device_t *dev, unsigned long dir, 
					   const char *name, fat_dirent_t *di);
unsigned int
	BootFatReadCluster(const device_t *dev, unsigned long cluster, void *buf);
unsigned long
	BootFatGetNextCluster(const device_t *dev, unsigned long cluster);
void BootFatAssembleName(const fat_dirent_t *entries, char *name);

#endif