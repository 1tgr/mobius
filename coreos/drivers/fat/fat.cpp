/*
 *  Source code the the Möbius FAT driver
 *
 *  Author: Tim Robinson
 *
 *  This driver is written in C++ partly because it seems easier to write 
 *      FSDs in C++ (entries in the FSD vtbl map to C++ virtual functions),
 *      and partly to prove and to ensure that the VFS (written in C) can
 *      talk to C++.
 *
 *  This driver currently has support for FAT12 and FAT16 on any Möbius storage
 *      device (it's been tested with the ATA and floppy disk drivers to far).
 *      Currently long file names are not supported, although adding that would
 *      be a matter of modifying the Fat::AssembleName function. FAT32 is also 
 *      not supported (mainly because I don't have a FAT32 disk to test it on).
 *      Adding FAT32 support would probably just be a matter of modifying 
 *      Fat::GetNextCluster and the routines that access the starting cluster
 *      in the fat_dirent_t structure, although for large hard disks it would
 *      make sense to stop loading all the FAT into memory at mount time,
 *      and start cacheing it, possibly with a kernel file cache object.
 *
 *  One big thing that this driver is missing is write support. It is currently
 *      possible to write to FAT files as long as the file is not grown (which
 *      would require writing to the FAT). It is not possible to create files
 *      or directories.
 */

/*
 * $History: fat.cpp $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 14/03/04   Time: 0:28
 * Updated in $/coreos/drivers/fat
 * Added ValidateBootSector function
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/cache.h>
#include <kernel/io.h>
#include <os/defs.h>
#include <os/hash.h>

#include <kernel/device>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <errno.h>
#include <wchar.h>
#include <ctype.h>
#include <string.h>

extern "C" void *__morecore(size_t nbytes);

#include "fat.h"

using namespace kernel;

/*
 * Structure representing a FAT directory entry
 * These are cached by the FSD and hashed by their vnode ID. Vnode IDs
 *  are generated sequentially and not re-used until the 32-bit vnode
 *  counter wraps around.
 * xxx -- should probably start re-using vnode IDs - opening 2^32 files
 *  or directories without remounting the FS would break it.
 */
struct FatVnode
{
    FatVnode *next;             // pointer to the next hashed entry
    unsigned locks;             // reference count
    vnode_id_t id;              // vnode ID
    vnode_id_t parent;          // ID of this vnode's parent
    wchar_t *name;              // full nul-terminated name of the vnode
    fat_dirent_t di;            // directory entry of the vnode
    //unsigned num_dir_entries;   // directory: number of entries
    //fat_dirent_t *dir_entries;  // directory: entries, or NULL if file
    file_handle_t *directory;
    unsigned first_cluster;
};

/*
 * FAT file structure; this is the FSD's file cookie.
 * Nothing more than a pointer to the file's vnode and a flat list
 *  of clusters (for easy random access).
 */
struct FatFile
{
    FatVnode *vnode;
    unsigned num_clusters;
    uint32_t *clusters;
};

/*
 * FAT directory structure; this is the FSD's directory cookie.
 * Nothing more than a pointer to the directory's vnode and the index
 *  of the next directory entry to be retrieved by readdir.
 */
struct FatSearch
{
    FatVnode *vnode;
    unsigned index;
};


class Fat : public fsd_t
{
public:
    void dismount();
    void get_fs_info(fs_info_t *inf);

    status_t parse_element(const wchar_t *name, wchar_t **new_path, vnode_t *node);
    status_t create_file(vnode_id_t dir, const wchar_t *name, void **cookie);
    status_t lookup_file(vnode_id_t file, uint32_t open_flags, void **cookie);
    status_t get_file_info(void *cookie, uint32_t type, void *buf);
    status_t set_file_info(void *cookie, uint32_t type, const void *buf);
    void free_cookie(void *cookie);

    status_t read_write_file(const fs_request_t *req, size_t *bytes);
    bool ioctl_file(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io);
    bool passthrough(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io);

    status_t mkdir(vnode_id_t dir, const wchar_t *name, void **dir_cookie);
    status_t opendir(vnode_id_t dir, void **dir_cookie);
    status_t readdir(void *dir_cookie, dirent_t *buf);
    void free_dir_cookie(void *dir_cookie);

    void finishio(request_t *req);
    void flush_cache(file_t *fd);

    fsd_t *Init(driver_t *drv, const wchar_t *dest);

protected:
    static bool ValidateBootSector(const fat_bootsector_t *bs);
    uint32_t GetNextCluster(uint32_t cluster);
    uint32_t ClusterToOffset(uint32_t cluster);
    bool ReadWriteFile(file_t *file, page_array_t *pages, size_t length, 
        fs_asyncio_t *io, bool is_reading);
    FatVnode *GetVnode(vnode_id_t id);
    void ReleaseVnode(FatVnode *vnode);
    vnode_id_t AllocVnode(vnode_id_t parent, const fat_dirent_t *di, 
        unsigned first_cluster, const wchar_t *name);

    static unsigned AssembleName(fat_dirent_t *entries, wchar_t *name);
    size_t SizeOfDir(const FatVnode *vnode);

    device_t *m_device;             // storage device
    fat_bootsector_t m_bpb;         // boot sector
    unsigned m_fat_bits;            // number of bits per FAT entry
    unsigned m_bytes_per_cluster;   // number of bytes per cluster
    unsigned m_root_start;          // sector offset of the root directory
    unsigned m_data_start;          // sector offset of the data area
    unsigned m_cluster_shift;       // shift value for clusters <=> bytes
    unsigned m_sectors;             // total number of sectors
    //uint8_t *m_fat;                 // memory copy of FAT
    file_handle_t *m_fat;
    vnode_id_t m_next_id;           // next vnode ID to be issued
    FatVnode *m_vnodes[128];        // hash table of vnode structures
};

/*
 * xxx -- some systems (e.g. PC98) can have funny sector sizes (e.g. 2048 
 *  bytes). Hopefully the storage device driver on those systems will support
 *  the same sector size, or at least emulate 512 bytes/sector.
 */
#define SECTOR_SIZE                 512

/*
 * Make some compile-time assertions on the sizes of some structures. 
 *  Provides some confirmation that proper packing has taken effect.
 */
CASSERT(sizeof(fat_bootsector_t) == SECTOR_SIZE);
CASSERT(sizeof(fat_dirent_t) == 32);
CASSERT(sizeof(fat_dirent_t) == sizeof(fat_lfnslot_t));

#define FAT_MAX_NAME                256

/* Dummy cluster value for the first FAT */
#define FAT_CLUSTER_FAT             0x10000000
/* Dummy cluster value for the root directory */
#define FAT_CLUSTER_ROOT            0x20000000

#define FAT_CLUSTER_AVAILABLE       0
#define FAT_CLUSTER_RESERVED_START  0xfff0
#define FAT_CLUSTER_RESERVED_END    0xfff6
#define FAT_CLUSTER_BAD             0xfff7
#define FAT_CLUSTER_EOC_START       0xfff8
#define FAT_CLUSTER_EOC_END         0xffff

#define IS_EOC_CLUSTER(c) \
    ((c) >= FAT_CLUSTER_EOC_START && (c) <= FAT_CLUSTER_EOC_END)
#define IS_RESERVED_CLUSTER(c) \
    ((c) >= FAT_CLUSTER_RESERVED_START && (c) <= FAT_CLUSTER_RESERVED_END)

/* From BeOS dosfs: (UnicodeMappings.cpp) */
const wchar_t msdostou[] = {
0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
0xFFFF
};


bool Fat::ValidateBootSector(const fat_bootsector_t *bs)
{
    switch ((uint8_t) bs->jmp)
    {
    case 0xeb:
        if (bs->nop != 0x90)
            return false;
        break;

    case 0xe9:
        break;

    default:
        wprintf(L"Fat::ValidateBootSector: failed on jmp = %02x\n", (uint8_t) bs->jmp);
        return false;
    }

    switch (bs->bytes_per_sector)
    {
    case 512:
    case 1024:
    case 2048:
    case 4096:
        break;

    default:
        wprintf(L"Fat::ValidateBootSector: failed on bytes_per_sector = %04x\n", bs->bytes_per_sector);
        return false;
    }

    switch (bs->sectors_per_cluster)
    {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
    case 128:
        break;

    default:
        wprintf(L"Fat::ValidateBootSector: failed on sectors_per_cluster = %02x\n", bs->sectors_per_cluster);
        break;
    }

    switch (bs->media_descriptor)
    {
    case 0xF0:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
    case 0xFE:
    case 0xFF:
        break;

    default:
        wprintf(L"Fat::ValidateBootSector: failed on media_descriptor = %02x\n", bs->media_descriptor);
    }


    return true;
}


/*
 *  Look up the next cluster value from the FAT for a particular cluster.
 *  Handles FAT12 and FAT16 transparently
 *  xxx -- modify this for FAT32
 */
uint32_t Fat::GetNextCluster(uint32_t cluster)
{
    uint32_t FATOffset;
    uint16_t w;
    //uint8_t* b;

    if (cluster >= FAT_CLUSTER_FAT ||
        cluster >= FAT_CLUSTER_ROOT)
        return cluster + 1;

    if (cluster >=
        (uint32_t) (m_sectors / m_bpb.sectors_per_cluster))
    {
        wprintf(L"Cluster 0x%x (%d) beyond total clusters 0x%x (%d)\n",
            cluster, cluster,
            m_sectors / m_bpb.sectors_per_cluster,
            m_sectors / m_bpb.sectors_per_cluster);
        assert(false);
    }

    switch (m_fat_bits)
    {
    case 12:
        FATOffset = cluster + cluster / 2;
        break;
    case 16:
        FATOffset = cluster * 2;
        break;
    default:
        assert(m_fat_bits == 12 || m_fat_bits == 16);
        return (uint32_t) -1;
    }

    // xxx -- won't work for FAT32
    if (!FsRead(m_fat, &w, FATOffset, sizeof(w), NULL))
    {
        wprintf(L"Fat::GetNextCluster(%x): FsRead failed\n");
        return (uint32_t) -1;
    }

    if (m_fat_bits == 12)
    {
        if (cluster & 1)    // cluster is odd
            w >>= 4;
        else                // cluster is even
            w &= 0xfff;
        
        if (w >= 0xff0)
            w |= 0xf000;
    }
    
    return w;
}

/*
 *  Assembles a full nul-terminated name for a FAT directory entry. Returns
 *      the number of directory entries consumed by the name (1 for short 
 *      names, > 1 for long names).
 *  xxx -- handle long file names
 */
unsigned Fat::AssembleName(fat_dirent_t *entries, wchar_t *name)
{
    fat_lfnslot_t *lfn;
    uint16_t ch;

    lfn = (fat_lfnslot_t*) entries;
    if ((entries[0].attribs & ATTR_LONG_NAME) == ATTR_LONG_NAME)
        return 1;
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

            ch = msdostou[entries[0].name[i]];
            *ptr++ = towlower(ch);
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

                ch = msdostou[entries[0].extension[i]];
                *ptr++ = towlower(ch);
            }
        }

        *ptr = '\0';
        return 1;
    }
}

/*
 *  Returns the size of the directory entry records of a directory. Works for
 *      the root directory as well as subdirectories.
 */
size_t Fat::SizeOfDir(const FatVnode *vnode)
{
    size_t size;

    if (vnode->first_cluster == FAT_CLUSTER_ROOT)
    {
        size = m_bpb.num_root_entries * sizeof(fat_dirent_t);
        size = (size + m_bytes_per_cluster - 1) & -m_bytes_per_cluster;
        TRACE3("fat: SizeOfDir(root): %u entries, %u bpc => %u\n", 
            m_bpb.num_root_entries, m_bytes_per_cluster, size);
    }
    else
    {
        uint32_t cluster;
        
        cluster = vnode->first_cluster;
        size = 0;
        while (!IS_EOC_CLUSTER(cluster))
        {
            TRACE1("[%x]", cluster);
            size += m_bytes_per_cluster;
            cluster = GetNextCluster(cluster);
        }

        TRACE1("fat: SizeOfDir: %u\n", size);
    }

    return size;
}

/*
 *  Returns the volume-relative offset for a particular cluster.
 *  xxx -- needs to return uint64_t, to handle drives bigger than 4GB
 */
uint32_t Fat::ClusterToOffset(uint32_t cluster)
{
    if (cluster >= FAT_CLUSTER_ROOT)
        return m_root_start * m_bpb.bytes_per_sector
            + (cluster - FAT_CLUSTER_ROOT) * m_bytes_per_cluster;
    else if (cluster >= FAT_CLUSTER_FAT)
        return m_bpb.reserved_sectors * m_bpb.bytes_per_sector 
            + (cluster - FAT_CLUSTER_FAT) * m_bytes_per_cluster;
    else
        return m_data_start * m_bpb.bytes_per_sector 
            + (cluster - 2) * m_bytes_per_cluster;
}

/*
 *  Called by the VFS to read from or write to a file
 */
status_t Fat::read_write_file(const fs_request_t *req, size_t *bytes)
{
    FatFile *fatfile;
    io_callback_t cb;
    unsigned cluster_index;
    request_dev_t *dev_request;

    fatfile = (FatFile*) req->file->fsd_cookie;
    cluster_index = req->pos >> m_cluster_shift;

    if (cluster_index >= fatfile->num_clusters)
    {
        TRACE3("fat: position %u beyond end of clusters (%u > %u)\n",
            (uint32_t) req->pos, 
            req->pos >> m_cluster_shift,
            fatfile->num_clusters);
        *bytes = 0;
        return EEOF;
    }

    assert((req->pos & (m_bytes_per_cluster - 1)) == 0);
    assert((req->length & (m_bytes_per_cluster - 1)) == 0);
    assert(req->length == (size_t) m_bytes_per_cluster);

    TRACE3("Fat::ReadWriteFile: %s: %u bytes at %x\n", 
        req->is_reading ? L"reading" : L"writing",
        req->length,
        (unsigned) req->pos);

    dev_request = new request_dev_t;
    dev_request->header.code = DEV_READ;
    dev_request->params.buffered.pages = req->pages;
    dev_request->params.buffered.length = req->length;
    dev_request->params.buffered.offset = ClusterToOffset(fatfile->clusters[cluster_index]);
    dev_request->header.param = req->io;

    cb.type = IO_CALLBACK_FSD;
    cb.u.fsd = this;
    if (!IoRequest(&cb, m_device, &dev_request->header))
    {
        status_t ret;
        wprintf(L"Fat::StartIo: request failed straight away\n");
        *bytes = dev_request->params.buffered.length;
        ret = dev_request->header.result;
        delete dev_request;
        return ret;
    }

    return SIOPENDING;
}

/*
 *  Looks up a FatVnode structure given its vnode ID. Increments the vnode's 
 *      reference count and returns the vnode if found; otherwise, returns 
 *      NULL. ReleaseVnode must be called for the vnode returned when you have
 *      finished with it.
 */
FatVnode *Fat::GetVnode(vnode_id_t id)
{
    FatVnode *vnode;

    for (vnode = m_vnodes[id % _countof(m_vnodes)]; vnode != NULL; vnode = vnode->next)
    {
        if (vnode->id == id)
        {
            KeAtomicInc(&vnode->locks);
            return vnode;
        }
    }

    return NULL;
}

/*
 *  Decreases the reference count on a vnode obtained from GetVnode or 
 *      AllocVnode. Deallocates the vnode if the reference count is zero.
 */
void Fat::ReleaseVnode(FatVnode *vnode)
{
    FatVnode *item, *prev;

    if (vnode->locks == 0)
    {
        prev = NULL;
        for (item = m_vnodes[vnode->id % _countof(m_vnodes)]; item != NULL; item = item->next)
        {
            if (item == vnode)
                break;

            prev = item;
        }

        assert(item != NULL);
        if (prev == NULL)
            m_vnodes[vnode->id % _countof(m_vnodes)] = vnode->next;
        else
            prev->next = vnode->next;

        free(vnode->name);

        //if (vnode->dir_entries != NULL)
            //delete[] vnode->dir_entries;

        if (vnode->directory != NULL)
        {
            HndClose(&vnode->directory->hdr);
            vnode->directory = NULL;
        }

        delete vnode;
    }
    else
        KeAtomicDec(&vnode->locks);
}

/*
 *  Allocates a new vnode structure for a directory entry. If a vnode 
 *      structure already exists for the entry, returns that instead. 
 *      ReleaseVnode must be called for the vnode returned when you have 
 *      finished with it.
 */
vnode_id_t Fat::AllocVnode(vnode_id_t parent, const fat_dirent_t *di, 
                           unsigned first_cluster, const wchar_t *name)
{
    unsigned i;
    FatVnode *vnode, *pnode;
    //size_t size, bytes;

    for (i = 0; i < _countof(m_vnodes); i++)
    {
        for (vnode = m_vnodes[i]; vnode != NULL; vnode = vnode->next)
        {
            if (vnode->parent == parent &&
                _wcsicmp(vnode->name, name) == 0)
            {
                KeAtomicInc(&vnode->locks);
                return vnode->id;
            }
        }
    }

    if (parent == VNODE_NONE)
        pnode = NULL;
    else
        pnode = GetVnode(parent);

    vnode = new FatVnode;
    vnode->id = m_next_id++;
    vnode->next = m_vnodes[vnode->id % _countof(m_vnodes)];
    vnode->locks = 0;
    vnode->parent = parent;
    vnode->name = _wcsdup(name);
    vnode->di = *di;
    vnode->first_cluster = first_cluster;
    assert(vnode->name != NULL);

    if (first_cluster == FAT_CLUSTER_ROOT ||
        (vnode->di.attribs & FILE_ATTR_DIRECTORY))
    {
        vnode->di.file_length = SizeOfDir(vnode);
        //wprintf(L"Fat::AllocVnode: directory, length = %u\n",
            //vnode->di.file_length);
    }

    /*if (first_cluster == FAT_CLUSTER_ROOT)
    {
        TRACE2("\tReading root directory: %u bytes at sector %u\n",
            size,
            m_root_start);

        size = SizeOfDir(vnode);
        vnode->num_dir_entries = size / sizeof(fat_dirent_t);
        vnode->dir_entries = new fat_dirent_t[vnode->num_dir_entries];
        bytes = IoReadSync(m_device, 
            m_root_start * SECTOR_SIZE, 
            vnode->dir_entries, 
            size);
        if (bytes < size)
        {
            wprintf(L"fat: failed to read root directory: read %u bytes\n", 
                bytes);
            return false;
        }
    }
    else if (vnode->di.attribs & FILE_ATTR_DIRECTORY)
    {
        uint32_t cluster;
        uint8_t *ptr;
        size_t bytes_read;
        unsigned i;

        size = SizeOfDir(vnode);
        vnode->num_dir_entries = size / sizeof(fat_dirent_t);
        vnode->dir_entries = new fat_dirent_t[vnode->num_dir_entries];
        cluster = first_cluster;
        ptr = (uint8_t*) vnode->dir_entries;
        bytes_read = 0;
        TRACE2("\tReading sub directory: %u bytes at cluster %u\n",
            size,
            cluster);
        while (bytes_read < size &&
            !IS_EOC_CLUSTER(cluster))
        {
            TRACE1("\tCluster %u...\n", cluster);
            bytes = IoReadSync(m_device, 
                ClusterToOffset(cluster), 
                ptr, 
                m_bytes_per_cluster);

            if (bytes < m_bytes_per_cluster)
            {
                wprintf(L"fat: failed to read sub-directory: read %u bytes\n", 
                    bytes);
                return false;
            }

            ptr += bytes;
            bytes_read += bytes;
            cluster = GetNextCluster(cluster);
        }

        for (i = 0; i < vnode->num_dir_entries; i++)
            if ((vnode->dir_entries[i].name[0] == '.' && 
                 vnode->dir_entries[i].name[1] == ' ') ||
                (vnode->dir_entries[i].name[0] == '.' && 
                 vnode->dir_entries[i].name[1] == '.' && 
                 vnode->dir_entries[i].name[2] == ' '))
            vnode->dir_entries[i].name[0] = 0xe5;
    }
    else
    {
        vnode->num_dir_entries = 0;
        vnode->dir_entries = NULL;
    }*/

    m_vnodes[vnode->id % _countof(m_vnodes)] = vnode;

    vnode->directory = NULL;
    if (first_cluster == FAT_CLUSTER_ROOT ||
        (vnode->di.attribs & FILE_ATTR_DIRECTORY))
    {
        vnode_t vnode_this = { this, vnode->id };

        vnode->directory = FsOpen(&vnode_this, L"/", FILE_READ);
        if (vnode->directory == NULL)
            wprintf(L"Fat::AllocVnode: failed to open %s as a directory\n", name);
    }

    return vnode->id;
}

/*
 *  Called by the VFS when a FAT file system is dismounted
 */
void Fat::dismount()
{
    wprintf(L"Fat::dismount\n");
    HndClose(&m_fat->hdr);
    IoCloseDevice(m_device);
}

/*
 *  Called by the VFS to obtain file system information
 */
void Fat::get_fs_info(fs_info_t *inf)
{
    if (inf->flags & FS_INFO_CACHE_BLOCK_SIZE)
        inf->cache_block_size = m_bytes_per_cluster;
    if (inf->flags & FS_INFO_SPACE_TOTAL)
        inf->space_total = 0;
    if (inf->flags & FS_INFO_SPACE_FREE)
        inf->space_free = 0;
}

/*
 *  Called by the VFS to obtain a vnode ID for an entry in a particular 
 *      directory
 */
status_t Fat::parse_element(const wchar_t *name, wchar_t **new_path, vnode_t *node)
{
    wchar_t entry_name[FAT_MAX_NAME];
    FatVnode *vnode_parent;
    //unsigned i, count;

    if (node->id == VNODE_ROOT &&
        name[0] == ':')
    {
        if (_wcsicmp(name, L":fat") == 0)
        {
            fat_dirent_t di =
            {
                { ':', 'F', 'A', 'T', ' ', ' ', ' ', ' ' },
                { ' ', ' ', ' ' },
                0,
                { 0, },
                0, 0,
                FAT_CLUSTER_RESERVED_START,
                m_bpb.sectors_per_fat * m_bpb.bytes_per_sector,
            };

            node->id = AllocVnode(node->id, &di, FAT_CLUSTER_FAT, L":FAT");
            return 0;
        }
    }

    vnode_parent = GetVnode(node->id);
    if (vnode_parent == NULL)
    {
        wprintf(L"Fat::parse_element: id %u not found\n", node->id);
        return ENOTFOUND;
    }

    if (vnode_parent->directory == NULL)
    {
        wprintf(L"Fat::parse_element: id %u is not a directory\n", node->id);
        return ENOTADIR;
    }

    /*
     * Handle parent directories for the VFS. We remove the . and .. entries 
     *  from the listings; it's easy to simulate them.
     */
    if (name[0] == '.' &&
        name[1] == '.' &&
        name[2] == '\0')
    {
        /*
         * The parent of the root directory is the root directory. This should
         *  only happen if a FAT file system is mounted at the root; 
         *  otherwise, the VFS will handle .. for us (by going to the parent 
         *  directory of the mount point).
         */
        if (node->id != VNODE_ROOT)
            node->id = vnode_parent->parent;

        return 0;
    }
    else
    {
        /*for (i = 0; i < vnode->num_dir_entries; i += count)
        {
            count = AssembleName(vnode->dir_entries + i, entry_name);
            assert(count > 0);

            if (_wcsicmp(entry_name, name) == 0)
            {
                node->id = AllocVnode(node->id, vnode->dir_entries + i, 
                    vnode->dir_entries[i].first_cluster, entry_name);
                ReleaseVnode(vnode);
                return 0;
            }
        }*/

        fat_dirent_t dirent;
        unsigned offset;
        size_t bytes;

        //wprintf(L"Fat::parse_element: %s in %u\n", name, node->id);

        offset = 0;
        while (FsRead(vnode_parent->directory, &dirent, offset, sizeof(dirent), &bytes))
        {
            if (bytes < sizeof(dirent))
            {
                wprintf(L"Fat::parse_element: only read %u bytes at %u, needed %u\n",
                    bytes, offset, sizeof(dirent));
                break;
            }

            //wprintf(L"\telement %u: ", offset / sizeof(dirent));

            if (dirent.name[0] == '\0')
            {
                //wprintf(L"end\n");
                break;
            }

            AssembleName(&dirent, entry_name);
            //wprintf(L"%s\n", entry_name);

            if (_wcsicmp(entry_name, name) == 0)
            {
                node->id = AllocVnode(node->id, &dirent, dirent.first_cluster, entry_name);
                ReleaseVnode(vnode_parent);
                //wprintf(L"\t\tfound\n");
                return 0;
            }

            offset += sizeof(dirent);
        }
    }

    return ENOTFOUND;
}

/*
 *  Called by the VFS to create a new directory entry
 */
status_t Fat::create_file(vnode_id_t dir, const wchar_t *name, void **cookie)
{
    return ENOTIMPL;
}

/*
 *  Called by the VFS when a file vnode is opened
 */
status_t Fat::lookup_file(vnode_id_t file, uint32_t open_flags, void **cookie)
{
    FatVnode *vnode;
    FatFile *fatfile;
    unsigned num_clusters, i;
    uint32_t *ptr, cluster;

    vnode = GetVnode(file);
    if (vnode == NULL)
    {
        wprintf(L"Fat::lookup_file: vnode %u not found\n", file);
        return ENOTFOUND;
    }

    TRACE1("cluster = %lx\n", vnode->first_cluster);
    num_clusters = 
        (vnode->di.file_length + m_bytes_per_cluster - 1) & -m_bytes_per_cluster;
    num_clusters /= m_bytes_per_cluster;

    fatfile = new FatFile;
    if (fatfile == NULL)
        return errno;

    fatfile->vnode = vnode;
    fatfile->num_clusters = num_clusters;
    ptr = fatfile->clusters = new uint32_t[num_clusters];

    cluster = vnode->first_cluster;
    for (i = 0; i < num_clusters; i++)
    {
        *ptr++ = cluster;
        if (!IS_EOC_CLUSTER(cluster))
            cluster = GetNextCluster(cluster);
    }

    *cookie = fatfile;
    return 0;
}

/*
 *  Called by the VFS to obtain information on a file or directory
 */
status_t Fat::get_file_info(void *cookie, uint32_t type, void *buf)
{
    FatFile *fatfile;
    dirent_all_t *di;

    fatfile = (FatFile*) cookie;
    di = (dirent_all_t*) buf;
    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_DIRENT:
        wcsncpy(di->dirent.name, fatfile->vnode->name, _countof(di->dirent.name) - 1);
        di->dirent.vnode = fatfile->vnode->id;
        return 0;

    case FILE_QUERY_STANDARD:
        di->standard.length = fatfile->vnode->di.file_length;
        di->standard.attributes = fatfile->vnode->di.attribs;
        FsGuessMimeType(wcsrchr(fatfile->vnode->name, '.'), 
            di->standard.mimetype, 
            _countof(di->standard.mimetype) - 1);
        return 0;
    }

    return ENOTIMPL;
}

/*
 *  Called by the VFS to update information on a file or directory
 */
status_t Fat::set_file_info(void *cookie, uint32_t type, const void *buf)
{
    return EACCESS;
}

/*
 *  Called by the VFS to free a file cookie obtained from lookup_file or
 *      create_file.
 */
void Fat::free_cookie(void *cookie)
{
    FatFile *fatfile;
    fatfile = (FatFile*) cookie;
    delete[] fatfile->clusters;
    ReleaseVnode(fatfile->vnode);
    free(fatfile);
}

/*
 *  Called by the VFS to issue an ioctl on a file mapped to a device
 */
bool Fat::ioctl_file(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

/*
 *  Called by the VFS to issue a request on a file mapped to a device
 */
bool Fat::passthrough(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

/*
 *  Called by the VFS to create a directory
 */
status_t Fat::mkdir(vnode_id_t dir, const wchar_t *name, void **dir_cookie)
{
    return ENOTIMPL;
}

/*
 *  Called by the VFS to open a directory for listing
 */
status_t Fat::opendir(vnode_id_t dir, void **dir_cookie)
{
    FatSearch *search;
    FatVnode *vnode;

    vnode = GetVnode(dir);
    if (vnode == NULL)
    {
        wprintf(L"Fat::opendir: vnode %u not found\n", dir);
        return ENOTFOUND;
    }

    if (vnode->directory == NULL)
    {
        wprintf(L"Fat::opendir: vnode %u is not a directory\n", dir);
        ReleaseVnode(vnode);
        return ENOTADIR;
    }

    search = new FatSearch;
    if (search == NULL)
    {
        ReleaseVnode(vnode);
        return errno;
    }

    search->vnode = vnode;
    search->index = 0;
    *dir_cookie = search;
    return 0;
}

/*
 *  Called by the VFS to obtain the next file name from a directory listing
 */
status_t Fat::readdir(void *dir_cookie, dirent_t *buf)
{
    FatSearch *search;
    fat_dirent_t entry;
    size_t bytes;

    //wprintf(L"Fat::readdir\n");
    search = (FatSearch*) dir_cookie;
    assert(search->vnode->directory != NULL);
    do
    {
        //wprintf(L"Fat::readdir: index = %u\n", search->index);
        if (!FsRead(search->vnode->directory, &entry, 
            search->index * sizeof(entry), sizeof(entry), &bytes))
        {
            wprintf(L"Fat::readdir: FsRead failed\n");
            return errno;
        }

        //wprintf(L"Fat::readdir: %u bytes read\n", bytes);

        if (bytes < sizeof(entry))
            wprintf(L"Fat::readdir: only %u bytes read, needed %u\n",
                bytes, sizeof(entry));

        if (entry.name[0] == '\0')
        {
            //wprintf(L"Fat::readdir: zero entry, end of list\n");
            return EEOF;
        }

        search->index++;
    } while ((entry.name[0] == 0xe5) ||
            (entry.attribs & ATTR_LONG_NAME) == ATTR_LONG_NAME ||
            (entry.name[0] == '.' && entry.name[1] == ' ') ||
            (entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == ' '));

    //wprintf(L"Fat::readdir: finished: %s\n", buf->name);
    AssembleName(&entry, buf->name);
    buf->vnode = 0;
    return 0;
}

/*
 *  Called by the VFS to free a directory cookie obtained from mkdir or opendir
 */
void Fat::free_dir_cookie(void *dir_cookie)
{
    FatSearch *search;
    search = (FatSearch*) dir_cookie;
    ReleaseVnode(search->vnode);
    delete search;
}

/*
 *  Called by the I/O manager when a request issued to the storage device by 
 *      StartIo completes. Calls StartIo to resume I/O on the request.
 */
void Fat::finishio(request_t *req)
{
    request_dev_t *dev_request;

    TRACE2("fat: finishio: req = %p, result = %d\n", req, req->result);
    assert(req != NULL);
    assert(req->param != NULL);
    dev_request = (request_dev_t*) req;

    if (dev_request->header.result != 0)
        wprintf(L"fat: device failure: %d\n", dev_request->header.result);
    else
        TRACE0("Fat::StartIo: io finished\n");

    FsNotifyCompletion((fs_asyncio_t*) req->param, 
        dev_request->params.buffered.length, 
        dev_request->header.result);
    delete req;
}

/*
 *  Called by the VFS to flush dirty blocks in a file's cache
 */
void Fat::flush_cache(file_t *fd)
{
    uint64_t block;
    unsigned i, cluster_index;
    page_array_t *array;
    uint32_t *clusters;
    request_dev_t dev_request;
    status_t ret;

    for (i = 0; i < fd->cache->num_blocks; i++)
    {
        block = i << fd->cache->block_shift;
        if (CcIsBlockDirty(fd->cache, block))
        {
            array = CcRequestBlock(fd->cache, block, NULL);

            clusters = ((FatFile*) fd->fsd_cookie)->clusters;
            cluster_index = block >> m_cluster_shift;
            dev_request.header.code = DEV_WRITE;
            dev_request.params.buffered.pages = array;
            dev_request.params.buffered.length = m_bytes_per_cluster;
            dev_request.params.buffered.offset = 
                ClusterToOffset(clusters[cluster_index]);

            wprintf(L"Fat::flush_cache: writing dirty block %lu = sector 0x%x\n", 
                (uint32_t) block, (uint32_t) dev_request.params.buffered.offset / 512);
            ret = IoRequestSync(m_device, &dev_request.header);
            if (ret != 0)
                wprintf(L"Fat::flush_cache: IoRequestSync failed: %d\n", ret);

            MemDeletePageArray(array);
            CcReleaseBlock(fd->cache, block, 0, true, false);
        }
    }
}

/*
 *  Called by FatMountFs to mount a FAT volume
 */
fsd_t *Fat::Init(driver_t *drv, const wchar_t *path)
{
    const vnode_t vnode_this = { this, VNODE_ROOT };
    fat_dirent_t root_entry;
    unsigned length, temp;
    uint32_t RootDirSectors, FatSectors;
    vnode_id_t vnode_root;
    uint32_t clusters;

    m_fat = NULL;
    m_device = IoOpenDevice(path);
    if (m_device == NULL)
        return false;

    /* Read the boot sector */
    TRACE0("\tReading boot sector\n");
    length = IoReadSync(m_device, 0, &m_bpb, sizeof(m_bpb));
    if (length < sizeof(m_bpb))
    {
        TRACE1("Read failed: length = %u\n", length);
        delete this;
        return NULL;
    }

    if (!Fat::ValidateBootSector(&m_bpb))
    {
        delete this;
        return NULL;
    }

    /* Calculate the total number of sectors on the volume */
    TRACE1("\tDone: length = %u\n", length);
    m_sectors = m_bpb.sectors == 0 ? m_bpb.total_sectors : m_bpb.sectors;
    TRACE1("\tTotal of %u sectors\n", m_sectors);
    clusters = m_sectors / m_bpb.sectors_per_cluster;

    /*
     * Guess the FAT entry size based on the total number of clusters 
     *  (ref: Microsoft FAT document)
     */
    if (clusters >= 65525)
        m_fat_bits = 32;
    //else if (m_sectors > 20740)
    else if (clusters >= 4085)
        m_fat_bits = 16;
    else
        m_fat_bits = 12;

    if (m_fat_bits == 32)
    {
        wprintf(L"Fat::Init: FAT32 not supported (clusters = %u)\n", clusters);
        delete this;
        return NULL;
    }

    m_bytes_per_cluster = 
        m_bpb.bytes_per_sector * m_bpb.sectors_per_cluster;
    temp = m_bytes_per_cluster;
    TRACE3("\tbytes_per_cluster = %u * %u = %u\n", 
        m_bpb.bytes_per_sector,
        m_bpb.sectors_per_cluster, 
        temp);

    /*
     * Store the shift value to translate between bytes and clusters, to avoid
     *  doing multiplication and division (especially of 64-bit file offsets).
     * Assumes that the cluster size is a power of 2.
     */
    for (m_cluster_shift = 0; (temp & 1) == 0; m_cluster_shift++)
        temp >>= 1;

    RootDirSectors = (m_bpb.num_root_entries * 32) /
        m_bpb.bytes_per_sector;
    
    FatSectors = m_bpb.num_fats * m_bpb.sectors_per_fat;

    /* The root directory comes after the boot sector and the FAT(s) */
    m_root_start = m_bpb.reserved_sectors + FatSectors;

    /*
     * Work out the start of the data area; this comes after the boot sector,
     *  the FAT(s) and the root directory
     */
    m_data_start = m_root_start + RootDirSectors;

    /*
     * Create a dummy directory entry for the root directory, to create a 
     *  vnode for it.
     */
    memset(&root_entry, 0, sizeof(root_entry));
    root_entry.file_length = m_bpb.num_root_entries * sizeof(fat_dirent_t);
    root_entry.first_cluster = FAT_CLUSTER_RESERVED_START;

    for (temp = 0; temp < _countof(m_vnodes); temp++)
        m_vnodes[temp] = NULL;

    /* Create a vnode for the root directory, with a hard-coded vnode ID */
    m_next_id = VNODE_ROOT;
    vnode_root = AllocVnode(VNODE_NONE, &root_entry, FAT_CLUSTER_ROOT, L"");
    assert(vnode_root == VNODE_ROOT);

    m_fat = FsOpen(&vnode_this, L"/:fat", FILE_READ | FILE_WRITE);
    if (m_fat == NULL)
    {
        wprintf(L"Fat::Init: failed to open :fat\n");
        return NULL;
    }

    return this;
}

fsd_t *FatMountFs(driver_t *drv, const wchar_t *dest)
{
    Fat *root;
    
    /*
     * We don't want to use C++ exceptions (the only reasonable way to signal 
     *  an error from a constructor). Construct the Fat object in two stages.
     */

    root = new Fat;
    if (root == NULL)
        return NULL;

    return root->Init(drv, dest);
}

extern "C" void (*__CTOR_LIST__[])();
extern "C" void (*__DTOR_LIST__[])();

extern "C" bool DrvInit(driver_t *drv)
{
    void (**pfunc)() = __CTOR_LIST__;

    /*
     * Call any static/global constructors (not necessary for the FAT driver)
     *
     * Cygwin dcrt0.cc sources do this backwards, so we do too.
     */
    while (*++pfunc)
        ;
    while (--pfunc > __CTOR_LIST__)
        (*pfunc) ();

    drv->add_device = NULL;
    drv->mount_fs = FatMountFs;
    return true;
}
