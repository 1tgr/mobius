/* $Id: ext2.c,v 1.1 2002/05/15 01:08:22 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>
#include <kernel/cache.h>

#include <errno.h>
#include <wchar.h>

typedef struct superblock_t superblock_t;
struct superblock_t
{
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    /* EXT2_DYNAMIC_REV Specific */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    char s_volume_name[16];
    uint8_t s_last_mounted[64];
    uint32_t s_algo_bitmap;
    /* Performance Hints */
    uint8_t s_prealloc_blocks;
    uint8_t s_prealloc_dir_blocks;
    uint16_t s_reserved;
    /* Journaling Support */
    uint8_t s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    /* Unused */
    uint8_t padding[788];
};

CASSERT(sizeof(superblock_t) == 1024);

typedef struct gpdesc_t gpdesc_t;
struct gpdesc_t
{
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
};

CASSERT(sizeof(gpdesc_t) == 32);

typedef struct inode_t inode_t;
struct inode_t
{
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t i_osd2[12];
};

CASSERT(sizeof(inode_t) == 128);

#pragma pack(push, 1)
typedef struct ext2_dirent_t ext2_dirent_t;
struct ext2_dirent_t
{
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[1];
};

CASSERT(sizeof(ext2_dirent_t) == 9);
#pragma pack(pop)

#define EXT2_S_IFMT     0xF000  /* format mask */
#define EXT2_S_IFSOCK   0xA000  /* socket */
#define EXT2_S_IFLNK    0xC000  /* symbolic link */
#define EXT2_S_IFREG    0x8000  /* regular file */
#define EXT2_S_IFBLK    0x6000  /* block device */
#define EXT2_S_IFDIR    0x4000  /* directory */
#define EXT2_S_IFCHR    0x2000  /* character device */
#define EXT2_S_IFIFO    0x1000  /* fifo */

#define EXT2_S_ISUID    0x0800  /* SUID */
#define EXT2_S_ISGID    0x0400  /* SGID */
#define EXT2_S_ISVTX    0x0200  /* sticky bit */

#define EXT2_S_IRWXU    0x01C0  /* user access rights mask */
#define EXT2_S_IRUSR    0x0100  /* read */
#define EXT2_S_IWUSR    0x0080  /* write */
#define EXT2_S_IXUSR    0x0040  /* execute */

#define EXT2_S_IRWXG    0x0038  /* group access rights mask */
#define EXT2_S_IRGRP    0x0020  /* read */
#define EXT2_S_IWGRP    0x0010  /* write */
#define EXT2_S_IXGRP    0x0008  /* execute */

#define EXT2_S_IRWXO    0x0007  /* others access rights mask */
#define EXT2_S_IROTH    0x0004  /* read */
#define EXT2_S_IWOTH    0x0002  /* write */
#define EXT2_S_IXOTH    0x0001  /* execute */

typedef struct ext2_dir_t ext2_dir_t;
struct ext2_dir_t
{
    ext2_dir_t *hash_next;
    unsigned copies;
    unsigned ino;
    inode_t inode;
    wchar_t *name;
    ext2_dir_t *subdirs;
    unsigned num_subdirs;
    cache_t *cache;
};

typedef struct ext2_t ext2_t;
struct ext2_t
{
    fsd_t fsd;
    device_t *dev;
    superblock_t super_block;
    gpdesc_t *groups;
    unsigned num_groups;
    unsigned block_size;
    ext2_dir_t *dir_hash[31];
};

typedef struct ext2_search_t ext2_search_t;
struct ext2_search_t
{
    ext2_dir_t *dir;
    uint64_t pos;
};

typedef struct ext2_file_t ext2_file_t;
struct ext2_file_t
{
    inode_t inode;
    unsigned ino;
    wchar_t *name;
};

const wchar_t iso1tou[] = {
0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
0xFFFF
};

static uint64_t Ext2CalculateDeviceOffset(ext2_t *ext2, 
                                          uint64_t file_offset, 
                                          const inode_t *inode)
{
    uint32_t block;
    uint64_t offset;

    block = file_offset >> (10 + ext2->super_block.s_log_block_size);
    if (block < 13)
    {
        block = inode->i_block[block];
        wprintf(L"Ext2CalculateDeviceOffset: block = %lu\n", block);
    }
    else
    {
        /* xxx -- look up indirect blocks */
        assert(block < 13);
        block = 0;
    }

    offset = (uint64_t) block << (10 + ext2->super_block.s_log_block_size);
    offset += file_offset & ((1 << (10 + ext2->super_block.s_log_block_size)) - 1);
    offset &= -512;
    return offset;
}

static status_t Ext2GetDirectoryEntry(ext2_t *ext2, ext2_dir_t *dir, uint64_t *posn, dirent_t *dirent)
{
    ext2_dirent_t *di;
    page_array_t *array;
    unsigned i;
    uint8_t *ptr;
    uint64_t pos;

    pos = *posn;
again:
    /*wprintf(L"Ext2GetDirectoryEntry(%lu): pos = %lu\n", dir->ino, (uint32_t) pos);*/

    if (pos >= (((uint64_t) dir->inode.i_dir_acl << 32) | dir->inode.i_size))
        return EEOF;

    if (CcIsBlockValid(dir->cache, pos))
    {
        array = CcRequestBlock(dir->cache, pos);
        if (array == NULL)
            return errno;

        CcReleaseBlock(dir->cache, pos, true, false);
    }
    else
    {
        uint64_t offset;

        array = CcRequestBlock(dir->cache, pos);
        if (array == NULL)
            return errno;

        offset = 
            Ext2CalculateDeviceOffset(ext2, pos, &dir->inode);
        wprintf(L"Ext2GetDirectoryEntry(not cached): pos = %lu, offset = %lu\n", 
            (uint32_t) pos, (uint32_t) offset);
        if (IoReadPhysicalSync(ext2->dev, 
            offset, 
            array, 
            ext2->block_size) != ext2->block_size)
            /* xxx -- need to return something more useful here */
        {
            CcReleaseBlock(dir->cache, pos, false, false);
            return EHARDWARE;
        }

        CcReleaseBlock(dir->cache, pos, true, false);
    }

    ptr = MemMapPageArray(array, PRIV_KERN | PRIV_PRES);
    if (ptr == NULL)
        return errno;

    di = (ext2_dirent_t*) 
        (ptr + (pos & ((1 << (10 + ext2->super_block.s_log_block_size)) - 1)));
    if (di->rec_len == 0)
    {
        MemDeletePageArray(array);
        return EEOF;
    }

    pos += di->rec_len;

    if (di->name[0] == '.' 
        && (di->name_len == 1 || (di->name_len == 2 && di->name[1] == '.')))
    {
        MemDeletePageArray(array);
        goto again;
    }

    dirent->vnode = di->inode;
    for (i = 0; i < min(di->name_len, _countof(dirent->name) - 1); i++)
        dirent->name[i] = iso1tou[(unsigned char) di->name[i]];

    dirent->name[i] = '\0';
    MemDeletePageArray(array);

    *posn = pos;
    return 0;
}

static ext2_dir_t *Ext2GetDirectory(ext2_t *ext2, wchar_t *path, unsigned ino);

static void Ext2ReleaseDirectory(ext2_t *ext2, ext2_dir_t *dir)
{
    dir->copies--;
    if (dir->copies == 0)
    {
        /* free directory */
    }
}

static unsigned Ext2LookupInode(ext2_t *ext2, wchar_t *path)
{
    unsigned ino;
    wchar_t *slash;
    bool at_end;
    ext2_dir_t *dir;
    wchar_t *ch;
    dirent_t *dirent;
    status_t ret;
    uint64_t pos;

    if (path[0] == '/')
        path++;

    /* Start in the root */
    ino = 2;
    if (*path == '\0')
        return ino;

    /* Allocate dirent on the heap to save stack space */
    dirent = malloc(sizeof(dirent_t));
    ch = path;
    do
    {
        dir = Ext2GetDirectory(ext2, path, ino);
        if (dir == NULL)
        {
            free(dirent);
            return 0;
        }

        slash = wcschr(ch, '/');
        if (slash == NULL)
            at_end = true;
        else
        {
            *slash = '\0';
            at_end = false;
        }

        wprintf(L"Ext2LookupInode: %s\n", ch);

        pos = 0;
        while ((ret = Ext2GetDirectoryEntry(ext2, dir, &pos, dirent)) == 0)
        {
            if (_wcsicmp(dirent->name, ch) == 0)
            {
                ino = dirent->vnode;
                break;
            }
        }

        if (ret != 0)
        {
            Ext2ReleaseDirectory(ext2, dir);

            /*if (ret == EEOF)
                return ENOTFOUND;
            else
                return ret;*/

            return 0;
        }

        wprintf(L"Ext2LookupInode: ino = %u\n", ino);
        if (slash != NULL)
            *slash = '/';
        ch = slash + 1;
        Ext2ReleaseDirectory(ext2, dir);
    } while (!at_end);

    free(dirent);
    return ino;
}

static bool Ext2LoadInode(ext2_t *ext2, unsigned ino, inode_t *inode)
{
    unsigned group;
    uint8_t all_inodes[512];

    /* xxx -- these should really be uint64_t's */
    uint32_t offset, mod;

    ino--;
    group = ino / ext2->super_block.s_inodes_per_group;
    mod = (ino % ext2->super_block.s_inodes_per_group) * sizeof(inode_t);
    assert(group < ext2->num_groups);

    offset = ext2->groups[group].bg_inode_table 
        << (10 + ext2->super_block.s_log_block_size);
    offset += mod & -512;
    mod %= 512;

    wprintf(L"Ext2LoadInode: ino = %u, group = %u, offset = %lx, mod = %lx\n",
        ino, group, offset, mod);
    if (IoReadSync(ext2->dev, offset, all_inodes, 512) != 512)
        return false;

    memcpy(inode, all_inodes + mod, sizeof(inode_t));
    return true;
}

static ext2_dir_t *Ext2GetDirectory(ext2_t *ext2, wchar_t *path, unsigned ino)
{
    ext2_dir_t *dir, *prev;

    if (ino == 0)
        ino = Ext2LookupInode(ext2, path);

    if (ino == 0)
        return NULL;

    prev = NULL;
    dir = ext2->dir_hash[ino % _countof(ext2->dir_hash)];
    while (dir != NULL)
    {
        if (dir->ino == ino)
            break;

        prev = dir;
        dir = dir->hash_next;
    }

    if (dir == NULL)
    {
        inode_t inode;

        if (!Ext2LoadInode(ext2, ino, &inode))
            return NULL;

        dir = malloc(sizeof(ext2_dir_t));
        if (dir == NULL)
            return NULL;

        dir->hash_next = NULL;
        dir->ino = ino;
        dir->inode = inode;
        dir->copies = 0;

        dir->cache = CcCreateFileCache(ext2->block_size);
        if (dir->cache == NULL)
        {
            free(dir);
            return NULL;
        }

        if (prev == NULL)
            ext2->dir_hash[ino % _countof(ext2->dir_hash)] = dir;
        else
            prev->hash_next = dir;

        wprintf(L"Ext2GetDirectory(%s)\n", path);
        wprintf(L"\tmode = %x\n", inode.i_mode);
        wprintf(L"\tsize = %lu * 4GB + %lu\n", inode.i_dir_acl, inode.i_size);
        wprintf(L"\tblocks = %lx\n", inode.i_blocks);
    }

    dir->copies++;
    return dir;
}

void Ext2Dismount(fsd_t *fsd)
{
    ext2_t *ext2;
    ext2 = (ext2_t*) fsd;
    IoCloseDevice(ext2->dev);
    free(ext2);
}

void Ext2GetFsInfo(fsd_t *fsd, fs_info_t *info)
{
    ext2_t *ext2;
    ext2 = (ext2_t*) fsd;
    info->cache_block_size = ext2->block_size;
}

status_t Ext2CreateFile(fsd_t *fsd, const wchar_t *path, 
    fsd_t **redirect, void **cookie)
{
    return ENOTIMPL;
}

status_t Ext2LookupFile(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, 
                        void **cookie)
{
    unsigned ino;
    ext2_t *ext2;
    wchar_t *copy;
    ext2_file_t *file;

    ext2 = (ext2_t*) fsd;

    wprintf(L"Ext2LookupFile(%s)\n", path);
    copy = _wcsdup(path);
    ino = Ext2LookupInode(ext2, copy);
    free(copy);

    if (ino == 0)
        return ENOTFOUND;

    file = malloc(sizeof(ext2_file_t));
    if (file == NULL)
        return errno;

    if (!Ext2LoadInode(ext2, ino, &file->inode))
    {
        /* xxx -- use a more descriptive error code */
        free(file);
        return EHARDWARE;
    }

    file->ino = ino;
    copy = wcsrchr(path, '/');
    if (copy != NULL)
        file->name = _wcsdup(copy + 1);
    else
        file->name = _wcsdup(copy);

    *cookie = file;
    return 0;
}

status_t Ext2GetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    dirent_all_t *di;
    ext2_file_t *file;

    file = cookie;
    di = buf;

    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_DIRENT:
        di->dirent.vnode = file->ino;
        memset(di->dirent.name, 0, sizeof(di->dirent.name));
        wcsncpy(di->dirent.name, file->name, _countof(di->dirent.name) - 1);
        return 0;

    case FILE_QUERY_STANDARD:
        di->standard.length = ((uint64_t) file->inode.i_dir_acl << 32) 
            | file->inode.i_size;
        di->standard.attributes = 0;

        switch (file->inode.i_mode & EXT2_S_IFMT)
        {
        case EXT2_S_IFBLK:
        case EXT2_S_IFCHR:
            di->standard.attributes |= FILE_ATTR_DEVICE;
            break;

        case EXT2_S_IFDIR:
            di->standard.attributes |= FILE_ATTR_DIRECTORY;
            break;
        }

        FsGuessMimeType(wcsrchr(file->name, '.'), 
            di->standard.mimetype, 
            _countof(di->standard.mimetype));
        return 0;
    }

    return ENOTIMPL;
}

status_t Ext2SetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, const void *buf)
{
    return ENOTIMPL;
}

void Ext2FreeCookie(fsd_t *fsd, void *cookie)
{
    ext2_file_t *file;
    file = cookie;
    free(file->name);
    free(file);
}

bool Ext2ReadFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
    size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

bool Ext2WriteFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
    size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

status_t Ext2OpenDir(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **dir_cookie)
{
    ext2_t *ext2;
    ext2_dir_t *dir;
    wchar_t *copy;
    ext2_search_t *search;

    ext2 = (ext2_t*) fsd;
    copy = _wcsdup(path);
    dir = Ext2GetDirectory(ext2, copy, 0);
    if (dir == NULL)
    {
        free(copy);
        return ENOTFOUND;
    }

    free(copy);

    search = malloc(sizeof(ext2_search_t));
    if (search == NULL)
        return errno;

    search->dir = dir;
    search->pos = 0;
    *dir_cookie = search;
    return 0;
}

status_t Ext2ReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    ext2_search_t *search;
    search = dir_cookie;
    return Ext2GetDirectoryEntry((ext2_t*) fsd, search->dir, &search->pos, buf);
}

void Ext2FreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    ext2_search_t *search;
    search = dir_cookie;
    Ext2ReleaseDirectory((ext2_t*) fsd, search->dir);
    free(search);
}

void Ext2FinishIo(fsd_t *fsd, request_t *req)
{
}

void Ext2FlushCache(fsd_t *fsd, file_t *fd)
{
}

static const fsd_vtbl_t ext2_vtbl =
{
    Ext2Dismount,
    Ext2GetFsInfo,
    Ext2CreateFile,
    Ext2LookupFile,
    Ext2GetFileInfo,
    Ext2SetFileInfo,
    Ext2FreeCookie,
    Ext2ReadFile,
    Ext2WriteFile,
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    Ext2OpenDir,
    Ext2ReadDir,
    Ext2FreeDirCookie,
    NULL,           /* mount */
    Ext2FinishIo,
    Ext2FlushCache,
};

fsd_t *Ext2Mount(driver_t *drv, const wchar_t *dest)
{
    const wchar_t *ch;
    ext2_t *ext2;
    size_t size;

    ch = wcsrchr(dest, '/');
    if (ch == NULL)
        ch = dest;
    else
        ch++;

    ext2 = malloc(sizeof(ext2_t));
    if (ext2 == NULL)
        return NULL;

    memset(ext2, 0, sizeof(ext2_t));
    ext2->fsd.vtbl = &ext2_vtbl;
    ext2->dev = IoOpenDevice(ch);
    if (ext2->dev == NULL)
    {
        free(ext2);
        return NULL;
    }

    if (IoReadSync(ext2->dev, 
        1024, 
        &ext2->super_block, 
        sizeof(ext2->super_block)) != sizeof(ext2->super_block))
    {
        Ext2Dismount(&ext2->fsd);
        return NULL;
    }

    if (ext2->super_block.s_magic != 0xef53)
    {
        wprintf(L"ext2: invalid superblock magic: %x\n", ext2->super_block.s_magic);
        Ext2Dismount(&ext2->fsd);
        return NULL;
    }

    wprintf(L"ext2 superblock:\n");
    wprintf(L"\ts_inodes_count: %lu\n", ext2->super_block.s_inodes_count);
    wprintf(L"\ts_blocks_count: %lu\n", ext2->super_block.s_blocks_count);
    wprintf(L"\ts_log_block_size: 1024 << %lu = %lu\n", 
        ext2->super_block.s_log_block_size, 1024 << ext2->super_block.s_log_block_size);
    wprintf(L"\ts_log_frag_size: 1024 << %lu = %lu\n", 
        ext2->super_block.s_log_frag_size, 1024 << ext2->super_block.s_log_frag_size);
    wprintf(L"\ts_blocks_per_group: %lu\n", ext2->super_block.s_blocks_per_group);
    wprintf(L"\ts_frags_per_group: %lu\n", ext2->super_block.s_frags_per_group);
    wprintf(L"\ts_inodes_per_group: %lu\n", ext2->super_block.s_inodes_per_group);
    wprintf(L"\ts_magic: %x\n", ext2->super_block.s_magic);
    wprintf(L"\ts_first_ino: %lu\n", ext2->super_block.s_first_ino);
    wprintf(L"\ts_volume_name: %16S\n", ext2->super_block.s_volume_name);

    ext2->block_size = 1024 << ext2->super_block.s_log_block_size;

    ext2->num_groups = ext2->super_block.s_blocks_count / 
        ext2->super_block.s_blocks_per_group;
    wprintf(L"\tnum_groups = %u\n", ext2->num_groups);

    size = sizeof(gpdesc_t) * ext2->num_groups;
    size = (size + ext2->block_size - 1) & -ext2->block_size;
    ext2->groups = malloc(size);

    if (IoReadSync(ext2->dev, 
        2 * ext2->block_size,
        ext2->groups,
        size) != size)
    {
        Ext2Dismount(&ext2->fsd);
        return NULL;
    }

    return &ext2->fsd;
}

bool DrvInit(driver_t *drv)
{
    drv->mount_fs = Ext2Mount;
    return true;
}
