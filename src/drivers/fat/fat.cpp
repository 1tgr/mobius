/* $Id: fat.cpp,v 1.17 2002/06/22 18:10:23 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/cache.h>
#include <kernel/io.h>
#include <os/defs.h>

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

struct FatDirectory
{
    fat_dirent_t di;
    bool is_dir;
    unsigned num_entries;
    fat_dirent_t *entries;
    FatDirectory **subdirs;
};

struct FatFile
{
    fat_dirent_t di;
    bool is_dir;
    unsigned num_clusters;
    wchar_t *name;
    uint32_t *clusters;
};

struct FatSearch
{
    FatDirectory *dir;
    unsigned index;
};

typedef struct fat_ioextra_t fat_ioextra_t;
struct fat_ioextra_t
{
    enum { started, reading, finished } state;
    uint32_t bytes_read;
    unsigned cluster_index;
    size_t mod;
    uint32_t *clusters;
    request_dev_t dev_request;

    file_t *file;
    page_array_t *pages;
    size_t length;
    fs_asyncio_t *io;
    bool is_reading;
};

class Fat : public fsd_t
{
public:
    void dismount();
    void get_fs_info(fs_info_t *inf);

    status_t create_file(const wchar_t *path, fsd_t **redirect, void **cookie);
    status_t lookup_file(const wchar_t *path, fsd_t **redirect, void **cookie);
    status_t get_file_info(void *cookie, uint32_t type, void *buf);
    status_t set_file_info(void *cookie, uint32_t type, const void *buf);
    void free_cookie(void *cookie);

    bool read_file(file_t *file, page_array_t *pages, size_t length, fs_asyncio_t *io);
    bool write_file(file_t *file, page_array_t *pages, size_t length, fs_asyncio_t *io);
    bool ioctl_file(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io);
    bool passthrough(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io);

    status_t opendir(const wchar_t *path, fsd_t **redirect, void **dir_cookie);
    status_t readdir(void *dir_cookie, dirent_t *buf);
    void free_dir_cookie(void *dir_cookie);

    status_t mount(const wchar_t *path, fsd_t *newfsd);

    void finishio(request_t *req);
    void flush_cache(file_t *fd);

    fsd_t *Init(driver_t *drv, const wchar_t *dest);

protected:
    bool ConstructDir(FatDirectory *dir, const fat_dirent_t *di);
    void DestructDir(FatDirectory *dir);
    uint32_t GetNextCluster(uint32_t cluster);
    /*handle_t AllocFile(const fat_dirent_t *di, uint32_t flags);
    bool FreeFile(handle_t fd);*/
    bool LookupFile(FatDirectory *dir, wchar_t *path, fat_dirent_t *di);
    uint32_t ClusterToOffset(uint32_t cluster);
    void StartIo(fat_ioextra_t *extra);
    //bool FlushBlock(FatFile *file, uint64_t block);
    /*bool ReadDir(FatDirectory *dir, request_fs_t *req_fs);*/
    bool ReadWriteFile(file_t *file, page_array_t *pages, size_t length, 
        fs_asyncio_t *io, bool is_reading);

    static unsigned AssembleName(fat_dirent_t *entries, wchar_t *name);
    size_t SizeOfDir(const fat_dirent_t *di);

    device_t *m_device;
    fat_bootsector_t m_bpb;
    FatDirectory *m_root;
    unsigned m_fat_bits;
    unsigned m_bytes_per_cluster;
    unsigned m_root_start;
    unsigned m_data_start;
    unsigned m_cluster_shift;
    unsigned m_sectors;
    uint8_t *m_fat;
};

#define SECTOR_SIZE                 512

CASSERT(sizeof(fat_bootsector_t) == SECTOR_SIZE);
CASSERT(sizeof(fat_dirent_t) == 32);
CASSERT(sizeof(fat_dirent_t) == sizeof(fat_lfnslot_t));

#define FAT_MAX_NAME                256

#define FAT_CLUSTER_ROOT            0

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

bool Fat::ConstructDir(FatDirectory *dir, const fat_dirent_t *di)
{
    size_t bytes, size;
    
    size = SizeOfDir(di);
    /*dir->fsd = this;
    dir->pos = 0;
    dir->flags = FILE_READ;
    dir->cache = NULL;*/
    dir->is_dir = true;
    dir->di = *di;
    dir->num_entries = size / sizeof(fat_dirent_t);
    dir->entries = new fat_dirent_t[dir->num_entries];
    dir->subdirs = new FatDirectory*[dir->num_entries];
    memset(dir->subdirs, 0, sizeof(FatDirectory*) * dir->num_entries);

    if (di->first_cluster == FAT_CLUSTER_ROOT)
    {
	TRACE2("\tReading root directory: %u bytes at sector %u\n",
	    size,
	    m_root_start);
	bytes = IoReadSync(m_device, 
	    m_root_start * SECTOR_SIZE, 
	    dir->entries, 
	    size);
	if (bytes < size)
	{
	    wprintf(L"fat: failed to read root directory: read %u bytes\n", 
		bytes);
	    return false;
	}
    }
    else
    {
	uint32_t cluster;
	uint8_t *ptr;
	size_t bytes_read;
	unsigned i;

	cluster = di->first_cluster;
	ptr = (uint8_t*) dir->entries;
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

	for (i = 0; i < dir->num_entries; i++)
	    if ((dir->entries[i].name[0] == '.' && 
		 dir->entries[i].name[1] == ' ') ||
		(dir->entries[i].name[0] == '.' && 
		 dir->entries[i].name[1] == '.' && 
		 dir->entries[i].name[2] == ' '))
	    dir->entries[i].name[0] = 0xe5;
    }

    return true;
}

void Fat::DestructDir(FatDirectory *dir)
{
    unsigned i;

    for (i = 0; i < dir->num_entries; i++)
        if (dir->subdirs[i] != NULL)
        {
            DestructDir(dir->subdirs[i]);
            delete dir->subdirs[i];
        }

    delete[] dir->entries;
    delete[] dir->subdirs;
    delete dir;
}

uint32_t Fat::GetNextCluster(uint32_t cluster)
{
    uint32_t FATOffset;
    uint16_t w;
    uint8_t* b;

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

    b = m_fat + FATOffset;
    w = *(uint16_t*) b;
    /*dump(b - 2, 16); */

    if (m_fat_bits == 12)
    {
	if (cluster & 1)    /* cluster is odd */
	    w >>= 4;
	else		    /* cluster is even */
	    w &= 0xfff;
	
	if (w >= 0xff0)
	    w |= 0xf000;
    }
    
    return w;
}

unsigned Fat::AssembleName(fat_dirent_t *entries, wchar_t *name)
{
    fat_lfnslot_t *lfn;
    uint16_t ch;

    lfn = (fat_lfnslot_t*) entries;
    if ((entries[0].attribs & ATTR_LONG_NAME) == ATTR_LONG_NAME)
    {
	/*assert(false);*/
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

size_t Fat::SizeOfDir(const fat_dirent_t *di)
{
    size_t size;

    if (di->first_cluster == FAT_CLUSTER_ROOT)
    {
	size = m_bpb.num_root_entries * sizeof(fat_dirent_t);
	size = (size + m_bytes_per_cluster - 1) & -m_bytes_per_cluster;
	TRACE3("fat: SizeOfDir(root): %u entries, %u bpc => %u\n", 
            m_bpb.num_root_entries, m_bytes_per_cluster, size);
    }
    else
    {
	uint32_t cluster;
	
	cluster = di->first_cluster;
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

bool Fat::LookupFile(FatDirectory *dir, wchar_t *path, fat_dirent_t *di)
{
    wchar_t *slash, name[FAT_MAX_NAME];
    unsigned i, count;
    fat_dirent_t *entries;
    bool find_dir;
    FatDirectory *sub;

    slash = wcschr(path, '/');
    if (slash)
    {
	*slash = '\0';
	find_dir = true;
    }
    else
	find_dir = false;

    i = 0;
    entries = dir->entries;
    while (i < dir->num_entries && entries[i].name[0] != '\0')
    {
        if (entries[i].name[0] == 0xe5 ||
	    entries[i].attribs & ATTR_LONG_NAME)
	{
	    i++;
	    continue;
	}

        count = AssembleName(entries + i, name);
	//TRACE2("%s %s\n", path, name);
	if (_wcsicmp(name, path) == 0)
	{
	    TRACE2("%s found at %u\n", name, entries[i].first_cluster);

	    if (find_dir)
	    {
                sub = dir->subdirs[i];
                if (sub == NULL)
                {
		    dir->subdirs[i] = sub = new FatDirectory;
                    if (!ConstructDir(sub, entries + i))
                    {
                        dir->subdirs[i] = NULL;
                        DestructDir(sub);
                        return false;
                    }
                }

		return LookupFile(sub, slash + 1, di);
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

uint32_t Fat::ClusterToOffset(uint32_t cluster)
{
    return m_data_start * m_bpb.bytes_per_sector 
	+ (cluster - 2) * m_bytes_per_cluster;
}

void Fat::StartIo(fat_ioextra_t *extra)
{
    unsigned bytes;
    uint8_t *buf, *page;
    page_array_t *array;
    io_callback_t cb;
    FatFile *fatfile;

    fatfile = (FatFile*) extra->file->fsd_cookie;

start:
    //wprintf(L"[%u]", extra->state);
    switch (extra->state)
    {
    case fat_ioextra_t::started:
	assert(extra->state != fat_ioextra_t::started);
	return;

    case fat_ioextra_t::reading:
	if (extra->dev_request.header.code)
	{
	    /*
	     * We've just finished transferring data from the device
	     * into the cache.
	     */
	    extra->dev_request.header.code = 0;
	    bytes = extra->dev_request.params.buffered.length;
	    if (bytes == 0)
	    {
		/* device failure */
		wprintf(L"fat: device failure\n");
		extra->io->op.result = extra->dev_request.header.code;
                extra->state = fat_ioextra_t::finished;
		goto start;
	    }

	    TRACE0("Fat::StartIo: io finished\n");
	    /* The block is now valid */
	    CcReleaseBlock(extra->file->cache, 
                extra->file->pos + extra->bytes_read, 
                true, 
                false);
	}

	bytes = extra->length - extra->bytes_read;
	/*if (bytes > m_bytes_per_cluster)
	    bytes = m_bytes_per_cluster;*/
        if (extra->mod + bytes > m_bytes_per_cluster)
            bytes = m_bytes_per_cluster - extra->mod;

        /*if (!extra->is_reading)
        {
            wprintf(L"FatStartIo: pos = %u cluster = %u = %x bytes = %u: %s", 
	        (uint32_t) (extra->file->pos + extra->bytes_read), 
	        extra->cluster_index, 
	        fatfile->clusters[extra->cluster_index], 
	        bytes,
                CcIsBlockValid(extra->file->cache, extra->file->pos + extra->bytes_read) ? L"cached" : L"not cached");
        }*/

	if (CcIsBlockValid(extra->file->cache, extra->file->pos + extra->bytes_read))
	{
            TRACE0("cached\n");
	    /* The cluster is already in the file's cache, so just memcpy() it */

            buf = (uint8_t*) MemMapPageArray(extra->pages, 
                PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);
	    array = CcRequestBlock(extra->file->cache, extra->file->pos + extra->bytes_read);
            page = (uint8_t*) MemMapPageArray(array, 
                PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);

            if (extra->is_reading)
                memcpy(buf + extra->bytes_read,
		    page + extra->mod,
		    bytes);
            else
                memcpy(page + extra->mod,
                    buf + extra->bytes_read,
		    bytes);

	    MemUnmapTemp();
	    CcReleaseBlock(extra->file->cache, extra->file->pos + extra->bytes_read, 
                true, 
                !extra->is_reading);
            MemDeletePageArray(array);

	    extra->mod += bytes;
	    if (extra->mod >= m_bytes_per_cluster)
	    {
		extra->mod -= m_bytes_per_cluster;

		extra->cluster_index++;
		if (extra->cluster_index >= fatfile->num_clusters)
		{
		    /* last cluster reached: invalid chain? */
		    TRACE0("fat: last cluster reached\n");
		    extra->state = fat_ioextra_t::finished;
                    extra->io->op.result = EEOF;
		    goto start;
		}
	    }

	    /*extra->file->pos += bytes;*/
	    extra->bytes_read += bytes;

	    if (extra->bytes_read >= extra->length)
	    {
		/*
		 * We've finished, so we'll switch to the 'finished' state 
		 *    for cleanup.
		 */
		extra->state = fat_ioextra_t::finished;
                extra->io->op.result = 0;
	    }
	    else
		/*
		 * More data need to be read.
		 * Clear 'code' so that the code above doesn't think we just
		 *    finished reading from the device.
		 */
		extra->dev_request.header.code = 0;

	    goto start;
	}
	else
	{
	    /*
	     * The cluster is not cached, so we need to lock the relevant
	     *	  cache block and read the entire cluster into there.
	     * When the read has finished we'll try the cache again.
	     */

	    TRACE0("not cached\n");

	    array = CcRequestBlock(extra->file->cache, extra->file->pos + extra->bytes_read);
            extra->cluster_index = 
                (extra->file->pos + extra->bytes_read) >> extra->file->cache->block_shift;

	    extra->dev_request.header.code = DEV_READ;
	    extra->dev_request.params.buffered.pages = array;
	    extra->dev_request.params.buffered.length = m_bytes_per_cluster;
	    extra->dev_request.params.buffered.offset = 
		ClusterToOffset(fatfile->clusters[extra->cluster_index]);
	    extra->dev_request.header.param = extra;

            cb.type = IO_CALLBACK_FSD;
            cb.u.fsd = this;
	    if (!IoRequest(&cb, m_device, &extra->dev_request.header))
	    {
		wprintf(L"Fat::StartIo: request failed straight away\n");
		DevUnmapBuffer();
                MemDeletePageArray(array);
		extra->state = fat_ioextra_t::finished;
		extra->io->op.result = extra->dev_request.header.result;
		goto start;
	    }
            else
                MemDeletePageArray(array);
	}
	break;

    case fat_ioextra_t::finished:
        MemDeletePageArray(extra->pages);
        FsNotifyCompletion(extra->io, extra->bytes_read, extra->io->op.result);
	free(extra);
	break;
    }
}

/*bool Fat::ReadDir(FatDirectory *dir, request_fs_t *req_fs)
{
    fat_dirent_t *di;
    size_t len;
    dirent_t *buf;
    unsigned count;

    di = dir->entries + dir->pos;
    while ((di->name[0] == 0xe5 ||
        di->attribs & ATTR_LONG_NAME) &&
        dir->pos < dir->num_entries)
    {
        dir->pos++;
        di++;
    }

    if (dir->pos >= dir->num_entries ||
        di->name[0] == '\0')
    {
        if (di->name[0] == '\0')
            TRACE0("fat: directory is empty\n");
        else
            TRACE0("fat: end of file\n");

        req_fs->header.result = EEOF;
        HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
        return false;
    }
    
    len = req_fs->params.fs_read.length;

    req_fs->params.fs_read.length = 0;
    buf = (dirent_t*) MemMapPageArray(req_fs->params.fs_read.pages, 
        PRIV_KERN | PRIV_PRES | PRIV_WR);
    while (req_fs->params.fs_read.length < len)
    {
        if (di->name[0] == 0xe5 ||
            di->attribs & ATTR_LONG_NAME)
        {
            TRACE1("%u: long file name or deleted\n", (unsigned) dir->pos);
            di++;
            dir->pos++;
        }
        else
        {
            count = AssembleName(di, buf->name);
            buf->length = di->file_length;
            buf->standard_attributes = di->attribs;
            TRACE2("%u: \"%s\"\n", (unsigned) dir->pos, buf->name);

            di += count;
            buf++;
            req_fs->params.fs_read.length += sizeof(dirent_t);
            dir->pos += count;
        }

        if (dir->pos >= dir->num_entries || 
            di->name[0] == '\0')
        {
            if (di->name[0] == '\0')
                TRACE0("fat: finished because of directory\n");
            else
                TRACE0("fat: finished because of end of request\n");

            break;
        }
    }

    MemUnmapTemp();
    HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
    return true;
}*/

bool Fat::ReadWriteFile(file_t *file, page_array_t *pages, size_t length, 
                        fs_asyncio_t *io, bool is_reading)
{
    fat_ioextra_t *extra;
    FatFile *fatfile;

    fatfile = (FatFile*) file->fsd_cookie;
    if (file->pos >> m_cluster_shift >= fatfile->num_clusters)
    {
        TRACE3("fat: position %u beyond end of clusters (%u > %u)\n",
            (uint32_t) file->pos, 
            file->pos >> m_cluster_shift,
            fatfile->num_clusters);
        io->op.bytes = 0;
        io->op.result = EEOF;
        return false;
    }

    if (file->pos + length >= fatfile->di.file_length && is_reading)
	length = fatfile->di.file_length - file->pos;
    if (length == 0)
    {
	TRACE0("fat: null read\n");
        io->op.bytes = 0;
        io->op.result = EEOF;
	return false;
    }

    io->original->result = io->op.result = SIOPENDING;

    extra = new fat_ioextra_t;
    extra->state = fat_ioextra_t::reading;
    extra->file = file;
    extra->cluster_index = file->pos >> m_cluster_shift;
    extra->bytes_read = 0;
    extra->dev_request.header.code = 0;

    /* xxx - change this to 64-bit modulo */
    /*extra->mod = (uint32_t) file->pos % m_bytes_per_cluster;*/
    extra->mod = file->pos & ((1 << m_cluster_shift) - 1);

    extra->pages = MemCopyPageArray(pages->num_pages, pages->mod_first_page, 
        pages->pages);
    extra->length = length;
    extra->io = io;
    extra->is_reading = is_reading;

    TRACE2("Fat::ReadWriteFile: %s: %u bytes\n", 
        is_reading ? L"reading" : L"writing",
        length);
    StartIo(extra);
    return true;
}

void Fat::dismount()
{
}

void Fat::get_fs_info(fs_info_t *inf)
{
    if (inf->flags & FS_INFO_CACHE_BLOCK_SIZE)
        inf->cache_block_size = m_bytes_per_cluster;
    if (inf->flags & FS_INFO_SPACE_TOTAL)
        inf->space_total = 0;
    if (inf->flags & FS_INFO_SPACE_FREE)
        inf->space_free = 0;
}

status_t Fat::create_file(const wchar_t *path, fsd_t **redirect, void **cookie)
{
    return ENOTIMPL;
}

status_t Fat::lookup_file(const wchar_t *path, fsd_t **redirect, void **cookie)
{
    fat_dirent_t di;
    FatFile *fatfile;
    unsigned num_clusters, i;
    uint32_t *ptr, cluster;
    wchar_t *copy;
    const wchar_t *ch;

    TRACE1("Fat::lookup_file(%s): ", path);
    copy = _wcsdup(path);
    if (!LookupFile(m_root, copy + 1, &di))
    {
        free(copy);
        return ENOTFOUND;
    }

    TRACE1("cluster = %lx\n", di.first_cluster);
    free(copy);
    num_clusters = 
	(di.file_length + m_bytes_per_cluster - 1) & -m_bytes_per_cluster;
    num_clusters /= m_bytes_per_cluster;

    /*fatfile = (FatFile*) malloc(sizeof(FatFile) 
        + num_clusters * sizeof(uint32_t));*/
    fatfile = new FatFile;
    if (fatfile == NULL)
        return errno;

    ch = wcsrchr(path, '/');
    if (ch == NULL)
        ch = path;
    else
        ch++;

    fatfile->di = di;
    fatfile->is_dir = false;
    fatfile->num_clusters = num_clusters;
    fatfile->name = _wcsdup(ch);
    ptr = fatfile->clusters = new uint32_t[num_clusters];

    /*ptr = (uint32_t*) (fatfile + 1);*/
    cluster = di.first_cluster;
    for (i = 0; i < num_clusters; i++)
    {
	*ptr++ = cluster;
	if (!IS_EOC_CLUSTER(cluster))
	    cluster = GetNextCluster(cluster);
    }

    *cookie = fatfile;
    return 0;
}

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
        wcsncpy(di->dirent.name, fatfile->name, _countof(di->dirent.name) - 1);
        di->dirent.vnode = 0;
        return 0;

    case FILE_QUERY_STANDARD:
        di->standard.length = fatfile->di.file_length;
        di->standard.attributes = fatfile->di.attribs;
        FsGuessMimeType(wcsrchr(fatfile->name, '.'), 
            di->standard.mimetype, 
            _countof(di->standard.mimetype) - 1);
        return 0;
    }

    return ENOTIMPL;
}

status_t Fat::set_file_info(void *cookie, uint32_t type, const void *buf)
{
    return EACCESS;
}

void Fat::free_cookie(void *cookie)
{
    FatFile *fatfile;
    fatfile = (FatFile*) cookie;
    delete[] fatfile->clusters;
    free(fatfile->name);
    free(fatfile);
}

bool Fat::read_file(file_t *file, page_array_t *pages, size_t length, fs_asyncio_t *io)
{
    return ReadWriteFile(file, pages, length, io, true);
}

bool Fat::write_file(file_t *file, page_array_t *pages, size_t length, fs_asyncio_t *io)
{
    return ReadWriteFile(file, pages, length, io, false);
}

bool Fat::ioctl_file(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

bool Fat::passthrough(file_t *file, uint32_t code, void *buf, size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

status_t Fat::opendir(const wchar_t *path, fsd_t **redirect, void **dir_cookie)
{
    FatDirectory *dir;
    FatSearch *search;
    fat_dirent_t di;
    wchar_t *temp;

    if (path[0] == '/')
        path++;

    if (path[0] == '\0')
        di = m_root->di;
    else
    {
        temp = _wcsdup(path);
        if (!LookupFile(m_root, temp, &di))
        {
            free(temp);
            return ENOTFOUND;
        }

        free(temp);
    }

    dir = new FatDirectory;
    if (!ConstructDir(dir, &di))
    {
        DestructDir(dir);
        return EHARDWARE;
    }

    search = new FatSearch;
    if (search == NULL)
    {
        DestructDir(dir);
        return errno;
    }

    search->dir = dir;
    search->index = 0;
    *dir_cookie = search;
    return 0;
}

status_t Fat::readdir(void *dir_cookie, dirent_t *buf)
{
    FatSearch *search;
    fat_dirent_t *entry;
    unsigned i;

    search = (FatSearch*) dir_cookie;
    entry = search->dir->entries + search->index;
    while (true)
    {
        if (search->index >= search->dir->num_entries ||
            entry->name[0] == '\0')
            return EEOF;

        /* xxx - check return buffer length in buf->name */
        i = AssembleName(entry, buf->name);

        if (entry->name[0] != 0xe5 &&
            (entry->attribs & ATTR_LONG_NAME) == 0)
            break;

        search->index += i;
        entry += i;
    }

    buf->vnode = 0;
    search->index += i;
    return 0;
}

void Fat::free_dir_cookie(void *dir_cookie)
{
    FatSearch *search;
    search = (FatSearch*) dir_cookie;
    DestructDir(search->dir);
    delete search;
}

status_t Fat::mount(const wchar_t *path, fsd_t *newfsd)
{
    return ENOTIMPL;
}

void Fat::finishio(request_t *req)
{
    fat_ioextra_t *extra;

    TRACE1("fat: finishio: req = %p\n", req);
    assert(req != NULL);
    assert(req->param != NULL);

    extra = (fat_ioextra_t*) req->param;
    assert(req == &extra->dev_request.header);
    StartIo(extra);
}

void Fat::flush_cache(file_t *fd)
{
    uint64_t block;
    unsigned i, cluster_index;
    page_array_t *array;
    uint32_t *clusters;
    request_dev_t dev_request;

    for (i = 0; i < fd->cache->num_blocks; i++)
    {
        block = i << fd->cache->block_shift;
        if (CcIsBlockDirty(fd->cache, block))
        {
            array = CcRequestBlock(fd->cache, block);

            clusters = ((FatFile*) fd->fsd_cookie)->clusters;
            cluster_index = block >> m_cluster_shift;
	    dev_request.header.code = DEV_WRITE;
	    dev_request.params.buffered.pages = array;
	    dev_request.params.buffered.length = m_bytes_per_cluster;
	    dev_request.params.buffered.offset = 
		ClusterToOffset(clusters[cluster_index]);

            wprintf(L"Fat::flush_cache: writing dirty block %lu = sector 0x%x\n", 
                (uint32_t) block, (uint32_t) dev_request.params.buffered.offset / 512);
            if (!IoRequestSync(m_device, &dev_request.header))
                wprintf(L"Fat::flush_cache: IoRequestSync failed\n");

            MemDeletePageArray(array);
            CcReleaseBlock(fd->cache, block, true, false);
        }
        else
            wprintf(L"Fat::flush_cache: ignoring clean block %lu\n", (uint32_t) block);
    }
}

int next;

int
rand(void)
{
  /* This multiplier was obtained from Knuth, D.E., "The Art of
     Computer Programming," Vol 2, Seminumerical Algorithms, Third
     Edition, Addison-Wesley, 1998, p. 106 (line 26) & p. 108 */
  next = next * 6364136223846793005LL + 1;
  /* was: next = next * 0x5deece66dLL + 11; */
  return (int)((next >> 21) & RAND_MAX);
}

void FatThread1(void)
{
    char var;
    uint32_t start, end;
    wprintf(L"FatThread1: starting\n");
    var = (rand() % 26) + 'A';
    while (true)
    {
        start = SysUpTime();
        end = start + 1000;
        while (SysUpTime() < end)
            ;

        end = SysUpTime();
        wprintf(L"[%u]%c%u%%", 
            ThrGetCurrent()->id, var, (ThrGetCurrent()->cputime * 100) / (end - start));
        ThrGetCurrent()->cputime = 0;

        var++;
        if (var > 'Z')
            var = 'A';
        /*ThrSleep(ThrGetCurrent(), (var - 'A') * 100);
        KeYield();*/
    }
}

void FatThread2(void)
{
    char var;
    uint32_t start, end;
    wprintf(L"FatThread2: starting\n");
    var = (rand() % 26) + 'a';
    while (true)
    {
        start = SysUpTime();
        end = start + 2000;
        while (SysUpTime() < end)
            ;

        end = SysUpTime();
        wprintf(L"[%u]%c%u%%", 
            ThrGetCurrent()->id, var, (ThrGetCurrent()->cputime * 100) / (end - start));
        ThrGetCurrent()->cputime = 0;

        var--;
        if (var < 'a')
            var = 'z';
        /*ThrSleep(ThrGetCurrent(), (var - 'a') * 100);
        KeYield();*/
    }
}

fsd_t *Fat::Init(driver_t *drv, const wchar_t *path)
{
    fat_dirent_t root_entry;
    unsigned length, temp;
    uint32_t RootDirSectors, FatSectors;
    //wchar_t name[FAT_MAX_NAME];

    /*driver = drv;*/

    /*next = SysUpTime();
    ThrCreateThread(NULL, true, FatThread1, false, NULL, 16);
    ThrCreateThread(NULL, true, FatThread2, false, NULL, 16);*/

    m_device = IoOpenDevice(path);
    if (m_device == NULL)
        return false;

    TRACE0("\tReading boot sector\n");
    length = IoReadSync(m_device, 0, &m_bpb, sizeof(m_bpb));
    if (length < sizeof(m_bpb))
    {
	TRACE1("Read failed: length = %u\n", length);
	delete this;
	return NULL;
    }

    TRACE1("\tDone: length = %u\n", length);
    m_sectors = m_bpb.sectors == 0 ? m_bpb.total_sectors : m_bpb.sectors;
    TRACE1("\tTotal of %u sectors\n", m_sectors);

    if (m_sectors > 20740)
	m_fat_bits = 16;
    else
	m_fat_bits = 12;

    m_bytes_per_cluster = 
	m_bpb.bytes_per_sector * m_bpb.sectors_per_cluster;
    temp = m_bytes_per_cluster;
    TRACE3("\tbytes_per_cluster = %u * %u = %u\n", 
	m_bpb.bytes_per_sector,
	m_bpb.sectors_per_cluster, 
	temp);
    for (m_cluster_shift = 0; (temp & 1) == 0; m_cluster_shift++)
	temp >>= 1;
    
    TRACE1("\tAllocating %u bytes for FAT\n", 
        m_bpb.sectors_per_fat * m_bpb.bytes_per_sector);
    m_fat = (uint8_t*) __morecore(m_bpb.sectors_per_fat * m_bpb.bytes_per_sector);
    assert(m_fat != (uint8_t*) -1);

    TRACE3("\tFAT starts at sector %d = 0x%x = %p\n",
	m_bpb.reserved_sectors,
	m_bpb.reserved_sectors * 
	    m_bpb.bytes_per_sector,
	m_fat);

    length = m_bpb.sectors_per_fat * m_bpb.bytes_per_sector;
    TRACE4("bpc(before) = %u bps(before) = %u phys = %x phys(bpc) = %x\n", 
        m_bytes_per_cluster, m_bpb.bytes_per_sector,
        MemTranslate(m_fat),
        MemTranslate(&m_bytes_per_cluster));
    TRACE0("\tReading FAT\n");
    if (IoReadSync(m_device, 
	m_bpb.reserved_sectors * m_bpb.bytes_per_sector,
	m_fat,
	length) != length)
    {
	free(m_fat);
	delete this;
	return NULL;
    }

    TRACE2("bpc(after) = %u bps(after) = %u\n", 
        m_bytes_per_cluster, m_bpb.bytes_per_sector);

    RootDirSectors = (m_bpb.num_root_entries * 32) /
	m_bpb.bytes_per_sector;
    FatSectors = m_bpb.num_fats * m_bpb.sectors_per_fat;
    m_data_start = m_bpb.reserved_sectors + 
	FatSectors + 
	RootDirSectors;

    m_root_start = m_bpb.reserved_sectors + 
	/*m_bpb.hidden_sectors + */
	m_bpb.sectors_per_fat * m_bpb.num_fats;

    memset(&root_entry, 0, sizeof(root_entry));
    root_entry.file_length = m_bpb.num_root_entries * sizeof(fat_dirent_t);
    /*root_entry.file_length = SECTOR_SIZE * 4;*/
    root_entry.first_cluster = FAT_CLUSTER_ROOT;
    m_root = new FatDirectory;
    if (m_root == NULL ||
	!ConstructDir(m_root, &root_entry))
    {
	delete this;
	return NULL;
    }
    
    //AssembleName((fat_dirent_t*) (m_root + 1), name);
    //wprintf(L"\tFirst file in root is called %s\n", name);
    return this;
}

fsd_t *FatMountFs(driver_t *drv, const wchar_t *dest)
{
    Fat *root;
    
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

    /* Cygwin dcrt0.cc sources do this backwards */
    while (*++pfunc)
        ;
    while (--pfunc > __CTOR_LIST__)
        (*pfunc) ();

    drv->add_device = NULL;
    drv->mount_fs = FatMountFs;
    return true;
}
