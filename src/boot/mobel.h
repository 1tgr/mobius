/* $Id: mobel.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

#ifndef MOBEL_H__
#define MOBEL_H__

#define PAGE_SIZE	0x1000
#define LOAD_ADDR	0x100000UL

/* some functions return -1 when I don't know what else to return */
#define	ERR_EOF		-2	/* end of file */
#define	ERR_IO		-3	/* disk read error */
#define	ERR_MEM		-4	/* out of memory */
#define	ERR_SW		-5	/* software error ("can't happen") */
#define	ERR_FILE	-6	/* invalid file format */
#define	ERR_FILESYSTEM	-7

#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2

/* "An inode is an [unique] integer associated with a file on the filesystem."
For FAT, we use the first cluster of the file as the inode */
typedef unsigned short	inode_t;

typedef unsigned long	sector_t;

typedef unsigned long	pos_t;

typedef struct
{
	unsigned char int13_dev;
	unsigned short sects, heads, bytes_per_sector;
} dev_t;

typedef struct
{
	pos_t pos;
	struct _mount *mount;
	inode_t inode;
	unsigned long file_size;
	unsigned is_dir : 1;
	char name[16]; /* xxx - 16 is arbitrary...maybe use malloc()? */
	inode_t cur_inode;
	sector_t cur_sector;
} file_t;

typedef struct
{
	int (*open_root)(file_t *root_dir, struct _mount *mount);
	int (*readdir)(file_t *dir, file_t *file);
	int (*read)(file_t *file, unsigned char *buf, unsigned len);
/*	void *info; */
	unsigned char info[64]; /* xxx - use malloc() */
} fsinfo_t;

typedef struct _mount
{
	dev_t *dev;
	fsinfo_t fsinfo;
	sector_t partition_start;
	inode_t curr_dir;
} mount_t;

int cprintf(const char *fmt, ...);
int read_sector(dev_t *dev, sector_t sector, unsigned char **blk);
int fat_mount(mount_t *mount, dev_t *dev, unsigned char part);

/* non-standard (not ANSI nor UNIX) I/O functions, but they were
eaiser to write than open(), readdir(), etc. (and more efficient,
since they deal directly with inodes, rather than path names) */
int my_open(file_t *file, char *path);
int my_seek(file_t *file, long offset, int whence);
int my_read(file_t *file, void *buf, unsigned len);
int my_close(file_t *file);

extern mount_t _mount;

#define	read_le16(X)	*(unsigned short *)(X)
#define	read_le32(X)	*(unsigned long *)(X)

#endif