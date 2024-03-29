/*
 * $History: ext2.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:30
 * Updated in $/coreos/drivers/ext2
 * Updated to keep in line with kernel
 */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/fs.h>
#include <kernel/cache.h>
#include "ext2.h"

#include <kernel/debug.h>

#include <errno.h>
#include <wchar.h>
#include <string.h>

#define EXT2_INODE_SIZE64(inode)    \
    (((uint64_t) (inode).i_dir_acl << 32) | (inode).i_size)

typedef struct ext2_dir_t ext2_dir_t;
struct ext2_dir_t
{
    ext2_dir_t *hash_next;
    unsigned copies;
    unsigned ino;
    ext2_inode_t inode;
    cache_t *cache;
};


typedef struct ext2_t ext2_t;
struct ext2_t
{
    fsd_t fsd;
    device_t *dev;
    ext2_super_block_t super_block;
    ext2_group_desc_t *groups;
    unsigned num_groups;
    unsigned block_size;
    ext2_dir_t *dir_hash[31];

    struct inode_hash
    {
        unsigned ino;
        ext2_inode_t inode;
    } inode_hash[128];
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
    ext2_inode_t inode;
    unsigned ino;
};


static uint32_t Ext2GetBlockNumber(ext2_t *ext2, uint32_t numbers_block, unsigned index)
{
    uint32_t *buf, ret;

    buf = malloc(ext2->block_size);
    if (buf == NULL)
        return 0;

    if (IoReadSync(ext2->dev, 
        numbers_block << (10 + ext2->super_block.s_log_block_size),
        buf,
        ext2->block_size) != ext2->block_size)
    {
        free(buf);
        return 0;
    }

    ret = buf[index];
    free(buf);
    return ret;
}


static uint64_t Ext2CalculateDeviceOffset(ext2_t *ext2, 
                                          uint64_t file_offset, 
                                          const ext2_inode_t *inode)
{
    uint32_t block;
    uint64_t offset;
    unsigned numbers_per_block;

    block = file_offset >> (10 + ext2->super_block.s_log_block_size);
    numbers_per_block = ext2->block_size / sizeof(uint32_t);
    if (block < 12)
    {
        block = inode->i_block[block];
    }
    else if (block < 12 
        + numbers_per_block)
    {
        /* single indirect */
        block = Ext2GetBlockNumber(ext2, inode->i_block[12], block - 12);
    }
    else if (block < 12 
        + numbers_per_block 
        + numbers_per_block * numbers_per_block)
    {
        /* double indirect */
        assert(false);
        block = 0;
    }
    else if (block < 12 
        + numbers_per_block 
        + numbers_per_block * numbers_per_block
        + numbers_per_block * numbers_per_block * numbers_per_block)
    {
        /* triple indirect */
        assert(false);
        block = 0;
    }

    assert(block != 0);

    offset = (uint64_t) block << (10 + ext2->super_block.s_log_block_size);
    offset += file_offset & ((1 << (10 + ext2->super_block.s_log_block_size)) - 1);
    offset &= -512;
    return offset;
}


static status_t Ext2GetDirectoryEntry(ext2_t *ext2, ext2_dir_t *dir, uint64_t *posn, dirent_t *dirent)
{
    ext2_dir_entry_t *di;
    page_array_t *array;
    unsigned i;
    uint8_t *ptr;
    uint64_t pos;

    pos = *posn;
again:
    if (pos >= (((uint64_t) dir->inode.i_dir_acl << 32) | dir->inode.i_size))
        return EEOF;

    if (CcIsBlockValid(dir->cache, pos))
    {
        array = CcRequestBlock(dir->cache, pos, NULL);
        if (array == NULL)
            return errno;

        CcReleaseBlock(dir->cache, pos, 0, true, false);
    }
    else
    {
        uint64_t offset;

        array = CcRequestBlock(dir->cache, pos, NULL);
        if (array == NULL)
            return errno;

        offset = 
            Ext2CalculateDeviceOffset(ext2, pos, &dir->inode);
        TRACE2("Ext2GetDirectoryEntry(not cached): pos = %lu, offset = %lu\n", 
            (uint32_t) pos, (uint32_t) offset);
        if (IoReadPhysicalSync(ext2->dev, 
            offset, 
            array, 
            ext2->block_size) != ext2->block_size)
            /* xxx -- need to return something more useful here */
        {
            CcReleaseBlock(dir->cache, pos, 0, false, false);
            return EHARDWARE;
        }

        CcReleaseBlock(dir->cache, pos, 0, true, false);
    }

    ptr = MemMapPageArray(array);
    if (ptr == NULL)
        return errno;

    di = (ext2_dir_entry_t*) 
        (ptr + (pos & ((1 << (10 + ext2->super_block.s_log_block_size)) - 1)));
    if (di->rec_len == 0/* ||
        di->name_len == 0*/)
    {
        MemDeletePageArray(array);
        return EEOF;
    }

    pos += di->rec_len;

    if (di->inode == 0 ||
        (di->name[0] == '.' 
        && (di->name_len == 1 || (di->name_len == 2 && di->name[1] == '.'))))
    {
        MemDeletePageArray(array);
        goto again;
    }

    dirent->vnode = di->inode;
    for (i = 0; i < min(di->name_len, _countof(dirent->name) - 1); i++)
        dirent->name[i] = (wchar_t) (unsigned char) di->name[i];

    dirent->name[i] = '\0';

    MemDeletePageArray(array);

    *posn = pos;
    return 0;
}


static ext2_dir_t *Ext2GetDirectory(ext2_t *ext2, wchar_t *path, unsigned ino);
static void Ext2ReleaseDirectory(ext2_t *ext2, ext2_dir_t *dir);

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

        pos = 0;
        while ((ret = Ext2GetDirectoryEntry(ext2, dir, &pos, dirent)) == 0)
        {
            if (_wcsicmp(dirent->name, ch) == 0)
            {
                wcscpy(ch, dirent->name);
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

        if (slash != NULL)
            *slash = '/';
        ch = slash + 1;
        Ext2ReleaseDirectory(ext2, dir);
    } while (!at_end);

    free(dirent);
    return ino;
}


static bool Ext2LoadInode(ext2_t *ext2, unsigned ino, ext2_inode_t *inode)
{
    unsigned group, hash, i;
    ext2_inode_t all_inodes[512 / sizeof(ext2_inode_t)];

    /* xxx -- these should really be uint64_t's */
    uint32_t offset, mod;

    hash = ino % _countof(ext2->inode_hash);
    if (ext2->inode_hash[hash].ino == ino)
    {
        TRACE2("Ext2LoadInode: hit cache inode %u at hash = %x\n", ino, hash);
        *inode = ext2->inode_hash[hash].inode;
        return true;
    }

    ino--;
    group = ino / ext2->super_block.s_inodes_per_group;
    mod = (ino % ext2->super_block.s_inodes_per_group) * sizeof(ext2_inode_t);
    assert(group < ext2->num_groups);

    offset = ext2->groups[group].bg_inode_table 
        << (10 + ext2->super_block.s_log_block_size);
    offset += mod & -512;
    mod %= 512;

    TRACE4("Ext2LoadInode: ino = %u, group = %u, offset = %lx, mod = %lx\n",
        ino, group, offset, mod);
    if (IoReadSync(ext2->dev, offset, all_inodes, 512) != 512)
        return false;

    TRACE2("Ext2LoadInode: loaded inode %u at hash = %x\n", ino + 1, hash);
    *inode = all_inodes[mod / sizeof(ext2_inode_t)];

    ino = (ino & -_countof(all_inodes)) + 1;
    for (i = 0; i < _countof(all_inodes); i++)
    {
        hash = (ino + i) % _countof(ext2->inode_hash);

        if (all_inodes[i].i_links_count == 0)
            TRACE2("Ext2LoadInode: skipped unused inode %u at hash = %x\n",
                ino + i, hash);
        else
        {
            TRACE2("Ext2LoadInode: cached inode %u at hash = %x\n",
                ino + i, hash);
            ext2->inode_hash[hash].ino = ino + i;
            ext2->inode_hash[hash].inode = all_inodes[i];
        }
    }

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
        ext2_inode_t inode;

        if (!Ext2LoadInode(ext2, ino, &inode))
            return NULL;

        dir = malloc(sizeof(ext2_dir_t));
        if (dir == NULL)
            return NULL;

        dir->hash_next = NULL;
        dir->ino = ino;
        dir->inode = inode;
        dir->copies = 1;

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
    }

    dir->copies++;
    return dir;
}


static void Ext2ReleaseDirectory(ext2_t *ext2, ext2_dir_t *dir)
{
    ext2_dir_t *item, *prev;
    unsigned hash;

    dir->copies--;
    if (dir->copies == 0)
    {
        hash = dir->ino % _countof(ext2->dir_hash);

        item = ext2->dir_hash[hash];
        prev = NULL;
        while (item != NULL)
        {
            if (item == dir)
                break;
            prev = item;
            item = item->hash_next;
        }

        assert(item != NULL);

        if (prev == NULL)
            ext2->dir_hash[hash] = dir->hash_next;
        else
            prev->hash_next = dir->hash_next;

        CcDeleteFileCache(dir->cache);
        free(dir);
    }
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
    if (info->flags & FS_INFO_CACHE_BLOCK_SIZE)
        info->cache_block_size = ext2->block_size;
    if (info->flags & FS_INFO_SPACE_TOTAL)
        info->space_total = ext2->super_block.s_blocks_count 
            << (10 + ext2->super_block.s_log_block_size);
    if (info->flags & FS_INFO_SPACE_FREE)
        info->space_free = ext2->super_block.s_free_blocks_count
            << (10 + ext2->super_block.s_log_block_size);
}


status_t Ext2ParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
{
    ext2_t *ext2;
    unsigned ino;
    ext2_dir_t *dir;
    dirent_t *dirent;
    status_t ret;
    uint64_t pos;

    ext2 = (ext2_t*) fsd;

    if (node->id == VNODE_ROOT)
        ino = 2;
    else
        ino = node->id;

    //wprintf(L"Ext2ParseElement(%s, %u)\n", name, ino);

    /* Allocate dirent on the heap to save stack space */
    dirent = malloc(sizeof(dirent_t));
    dir = Ext2GetDirectory(ext2, NULL, ino);
    if (dir == NULL)
    {
        free(dirent);
        return 0;
    }

    pos = 0;
    while ((ret = Ext2GetDirectoryEntry(ext2, dir, &pos, dirent)) == 0)
    {
        if (_wcsicmp(dirent->name, name) == 0)
        {
            //wcscpy(ch, dirent->name);
            node->id = dirent->vnode;
            break;
        }
    }

    free(dirent);
    if (ret != 0)
    {
        Ext2ReleaseDirectory(ext2, dir);

        if (ret == EEOF)
            ret = ENOTFOUND;

        /*if (ret == EEOF)
            return ENOTFOUND;
        else
            return ret;*/

        return ret;
    }

    //if (slash != NULL)
        //*slash = '/';
    //ch = slash + 1;
    Ext2ReleaseDirectory(ext2, dir);
    //wprintf(L"\t => %u\n", node->id);
    return 0;
}


status_t Ext2CreateFile(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, void **cookie)
{
    return ENOTIMPL;
}


status_t Ext2LookupFile(fsd_t *fsd, vnode_id_t id, uint32_t open_flags, void **cookie)
{
    unsigned ino;
    ext2_t *ext2;
    //wchar_t *copy;
    ext2_file_t *file;

    ext2 = (ext2_t*) fsd;

    /*copy = _wcsdup(path);
    ino = Ext2LookupInode(ext2, copy);

    if (ino == 0)
    {
        free(copy);
        return ENOTFOUND;
    }*/

    if (id == VNODE_ROOT)
        ino = 2;
    else
        ino = id;

    file = malloc(sizeof(ext2_file_t));
    if (file == NULL)
    {
        //free(copy);
        return errno;
    }

    if (!Ext2LoadInode(ext2, ino, &file->inode))
    {
        /* xxx -- use a more descriptive error code */
        //free(copy);
        free(file);
        return EHARDWARE;
    }

    file->ino = ino;
    /*path = wcsrchr(copy, '/');
    if (path != NULL)
        file->name = _wcsdup(path + 1);
    else
        file->name = _wcsdup(path);

    free(copy);*/
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
        //wcsncpy(di->dirent.name, file->name, _countof(di->dirent.name) - 1);
        wcscpy(di->dirent.name, L"");
        return 0;

    case FILE_QUERY_STANDARD:
        di->standard.length = EXT2_INODE_SIZE64(file->inode);
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

        if ((di->standard.attributes & (FILE_ATTR_DEVICE | FILE_ATTR_DIRECTORY)) == 0)
            /*FsGuessMimeType(wcsrchr(file->name, '.'), 
                di->standard.mimetype, 
                _countof(di->standard.mimetype));*/
            wcscpy(di->standard.mimetype, L"");
        else
            memset(di->standard.mimetype, 0, sizeof(di->standard.mimetype));

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
    free(file);
}


void Ext2FinishIo(fsd_t *fsd, request_t *req)
{
    ext2_t *ext2;
    fs_asyncio_t *io;
    request_dev_t *req_dev;

    ext2 = (ext2_t*) fsd;
    req_dev = (request_dev_t*) req;

    io = req_dev->header.param;
    assert(io != NULL);
    assert(req_dev->header.code == DEV_READ);

    FsNotifyCompletion(io, req_dev->params.dev_read.length, req_dev->header.result);
    free(req);
}


status_t Ext2ReadWriteFile(fsd_t *fsd, const fs_request_t *req, size_t *bytes)
{
    ext2_t *ext2;
    ext2_file_t *fd;
    uint64_t file_length;
    size_t length;
    io_callback_t cb;
    request_dev_t *req_dev;

    ext2 = (ext2_t*) fsd;
    *bytes = 0;

    fd = req->file->fsd_cookie;
    file_length = EXT2_INODE_SIZE64(fd->inode);

    if (req->pos + req->length > file_length)
    {
        length = file_length - req->pos;

        if (length == 0)
            return EEOF;
    }
    else
        length = req->length;

    assert((req->pos & (ext2->block_size - 1)) == 0);
    assert((req->length & (ext2->block_size - 1)) == 0);
    assert(req->length == (size_t) ext2->block_size);

    req_dev = malloc(sizeof(*req_dev));
    if (req_dev == NULL)
        return errno;

    memset(req_dev, 0, sizeof(*req_dev));
    req_dev->header.code = req->is_reading ? DEV_READ : DEV_WRITE;
    req_dev->header.param = req->io;
    req_dev->params.dev_read.offset = Ext2CalculateDeviceOffset(ext2, req->pos, &fd->inode);
    req_dev->params.dev_read.length = ext2->block_size;
    req_dev->params.dev_read.pages = req->pages;

    IoRequest(&cb, ext2->dev, &req_dev->header);
    return SIOPENDING;
}


status_t Ext2OpenDir(fsd_t *fsd, vnode_id_t id, void **dir_cookie)
{
    ext2_t *ext2;
    ext2_dir_t *dir;
    ext2_search_t *search;

    ext2 = (ext2_t*) fsd;
    if (id == VNODE_ROOT)
        id = 2;

    dir = Ext2GetDirectory(ext2, NULL, id);
    if (dir == NULL)
        return ENOTFOUND;

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


void Ext2FlushCache(fsd_t *fsd, file_t *fd)
{
}


static const vtbl_fsd_t ext2_vtbl =
{
    Ext2Dismount,
    Ext2GetFsInfo,
    Ext2ParseElement,
    Ext2CreateFile,
    Ext2LookupFile,
    Ext2GetFileInfo,
    Ext2SetFileInfo,
    Ext2FreeCookie,
    Ext2ReadWriteFile,
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    NULL,           /* mkdir */
    Ext2OpenDir,
    Ext2ReadDir,
    Ext2FreeDirCookie,
    Ext2FinishIo,
    Ext2FlushCache,
};


fsd_t *Ext2Mount(driver_t *drv, const wchar_t *dest)
{
    ext2_t *ext2;
    size_t size;

    ext2 = malloc(sizeof(ext2_t));
    if (ext2 == NULL)
        return NULL;

    memset(ext2, 0, sizeof(ext2_t));
    ext2->fsd.vtbl = &ext2_vtbl;
    ext2->dev = IoOpenDevice(dest);
    if (ext2->dev == NULL)
    {
        wprintf(L"ext2: " SYS_DEVICES L"/%s not found\n", dest);
        free(ext2);
        return NULL;
    }

    if (IoReadSync(ext2->dev, 
        1024, 
        &ext2->super_block, 
        sizeof(ext2->super_block)) != sizeof(ext2->super_block))
    {
        wprintf(L"ext2: unable to read superblock from " SYS_DEVICES L"/%s\n", dest);
        Ext2Dismount(&ext2->fsd);
        return NULL;
    }

    if (ext2->super_block.s_magic != 0xef53)
    {
        wprintf(L"ext2: invalid superblock magic: %x\n", ext2->super_block.s_magic);
        Ext2Dismount(&ext2->fsd);
        return NULL;
    }

    ext2->block_size = 1024 << ext2->super_block.s_log_block_size;

    ext2->num_groups = ext2->super_block.s_blocks_count / 
        ext2->super_block.s_blocks_per_group;

#ifdef DEBUG
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
    wprintf(L"\tnum_groups = %u\n", ext2->num_groups);
#endif

    size = sizeof(ext2_group_desc_t) * ext2->num_groups;
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
