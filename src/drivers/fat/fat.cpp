/* $Id: fat.cpp,v 1.11 2002/03/04 23:50:17 pavlovskii Exp $ */

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

#include "fat.h"

using namespace kernel;

struct FatDirectory : public file_t
{
    fat_dirent_t di;
    bool is_dir;
    unsigned num_entries;
    /* fat_dirent_t entries[]; */
};

struct FatFile : public file_t
{
    fat_dirent_t di;
    bool is_dir;
    unsigned num_clusters;
    cache_t *cache;
    /* uint32_t clusters[]; */
};

class Fat : public device
{
public:
    bool request(request_t *req);
    bool isr(uint8_t irq);
    void finishio(request_t *req);

    device_t *Init(driver_t *drv, const wchar_t *path, device_t *dev);

protected:
    bool	ConstructDir(FatDirectory *dir, const fat_dirent_t *di);
    uint32_t	GetNextCluster(uint32_t cluster);
    handle_t	AllocFile(const fat_dirent_t *di, uint32_t flags);
    bool	FreeFile(handle_t fd);
    bool	LookupFile(FatDirectory *dir, wchar_t *path, fat_dirent_t *di);
    uint32_t	ClusterToOffset(uint32_t cluster);
    void	StartIo(asyncio_t *io);
    bool	ReadDir(FatDirectory *dir, request_fs_t *req_fs);
    bool	ReadWriteFile(FatFile *file, request_fs_t *req_fs);
    bool	QueryFile(request_fs_t *req_fs);

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

#define SECTOR_SIZE	       512

CASSERT(sizeof(fat_bootsector_t) == SECTOR_SIZE);
CASSERT(sizeof(fat_dirent_t) == 16);
CASSERT(sizeof(fat_dirent_t) == sizeof(fat_lfnslot_t));

#define FAT_MAX_NAME		    256

#define FAT_CLUSTER_ROOT	    0

#define FAT_CLUSTER_AVAILABLE	     0
#define FAT_CLUSTER_RESERVED_START    0xfff0
#define FAT_CLUSTER_RESERVED_END    0xfff6
#define FAT_CLUSTER_BAD 	       0xfff7
#define FAT_CLUSTER_EOC_START	     0xfff8
#define FAT_CLUSTER_EOC_END	       0xffff

#define IS_EOC_CLUSTER(c) \
    ((c) >= FAT_CLUSTER_EOC_START && (c) <= FAT_CLUSTER_EOC_END)
#define IS_RESERVED_CLUSTER(c) \
    ((c) >= FAT_CLUSTER_RESERVED_START && (c) <= FAT_CLUSTER_RESERVED_END)

bool Fat::ConstructDir(FatDirectory *dir, const fat_dirent_t *di)
{
    size_t bytes, size;
    
    size = SizeOfDir(di) - sizeof(FatDirectory);
    dir->fsd = this;
    dir->pos = 0;
    dir->flags = FILE_READ;
    dir->is_dir = true;
    dir->di = *di;
    dir->num_entries = size / sizeof(fat_dirent_t);

    if (di->first_cluster == FAT_CLUSTER_ROOT)
    {
	TRACE2("\tReading root directory: %u bytes at sector %u\n",
	    size,
	    m_root_start);
	bytes = IoReadSync(m_device, 
	    m_root_start * SECTOR_SIZE, 
	    dir + 1, 
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
	fat_dirent_t *entries;

	cluster = di->first_cluster;
	ptr = (uint8_t*) (dir + 1);
	bytes_read = 0;
	TRACE2("\tReading sub directory: %u bytes at cluster %u\n",
	    size,
	    cluster);
	while (bytes_read < size &&
	    !IS_EOC_CLUSTER(cluster))
	{
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

	entries = (fat_dirent_t*) (dir + 1);
	for (i = 0; i < dir->num_entries; i++)
	    if ((entries[i].name[0] == '.' && 
		entries[i].name[1] == ' ') ||
		(entries[i].name[0] == '.' && 
		entries[i].name[1] == '.' && 
		entries[i].name[2] == ' '))
	    entries[i].name[0] = 0xe5;
    }

    return true;
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

handle_t Fat::AllocFile(const fat_dirent_t *di, uint32_t flags)
{
    FatFile *file;
    handle_t fd;
    unsigned num_clusters, i, cluster;
    uint32_t *ptr;

    num_clusters = 
	(di->file_length + m_bytes_per_cluster - 1) & -m_bytes_per_cluster;
    num_clusters /= m_bytes_per_cluster;
    fd = HndAlloc(NULL, sizeof(FatFile) + num_clusters * sizeof(uint32_t), 'file');
    file = (FatFile*) HndLock(NULL, fd, 'file');
    if (file == NULL)
    {
	HndClose(NULL, fd, 'file');
	return NULL;
    }

    file->fsd = this;
    file->pos = 0;
    file->flags = flags;
    file->di = *di;
    file->is_dir = false;
    file->num_clusters = num_clusters;
    file->cache = CcCreateFileCache(m_bytes_per_cluster);

    ptr = (uint32_t*) (file + 1);
    cluster = di->first_cluster;
    for (i = 0; i < num_clusters; i++)
    {
	*ptr++ = cluster;
	cluster = GetNextCluster(cluster);
    }

    assert(IS_EOC_CLUSTER(cluster));
    HndUnlock(NULL, fd, 'file');
    return fd;
}

bool Fat::FreeFile(handle_t fd)
{
    FatFile *file;

    file = (FatFile*) HndLock(NULL, fd, 'file');
    if (file == NULL)
	return false;

    if (!file->is_dir)
	CcDeleteFileCache(file->cache);

    HndUnlock(NULL, fd, 'file');
    return HndClose(NULL, fd, 'file');
}

unsigned Fat::AssembleName(fat_dirent_t *entries, wchar_t *name)
{
    fat_lfnslot_t *lfn;
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
	    
	    *ptr++ = tolower(entries[0].name[i]);
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

		*ptr++ = tolower(entries[0].extension[i]);
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
	TRACE1("fat: SizeOfDir(root): %u\n", size);
    }
    else
    {
	uint32_t cluster;
	
	cluster = di->first_cluster;
	size = 0;
	while (!IS_EOC_CLUSTER(cluster))
	{
	    size += m_bytes_per_cluster;
	    cluster = GetNextCluster(cluster);
	}

	TRACE1("fat: SizeOfDir: %u\n", size);
    }

    return sizeof(FatDirectory) + size;
}

bool Fat::LookupFile(FatDirectory *dir, wchar_t *path, fat_dirent_t *di)
{
    wchar_t *slash, name[FAT_MAX_NAME];
    unsigned i, count;
    fat_dirent_t *entries;
    bool find_dir;

    slash = wcschr(path, '/');
    if (slash)
    {
	*slash = '\0';
	find_dir = true;
    }
    else
	find_dir = false;
    
    i = 0;
    entries = ((fat_dirent_t*) (dir + 1));
    while (i < dir->num_entries && entries[i].name[0] != '\0')
    {
	if (entries[i].name[0] == 0xe5 ||
	    entries[i].attribs & ATTR_LONG_NAME)
	{
	    i++;
	    continue;
	}

	count = AssembleName(entries + i, name);
	
	if (_wcsicmp(name, path) == 0)
	{
	    TRACE2("%s found at %u\n", name, entries[i].first_cluster);

	    if (find_dir)
	    {
		dir = (FatDirectory*) malloc(SizeOfDir(di));
		if (dir &&
		    ConstructDir(dir, entries + i) &&
		    LookupFile(dir, slash + 1, di))
		{
		    free(dir);
		    return true;
		}
		else
		{
		    free(dir);
		    return false;
		}
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

typedef struct fat_ioextra_t fat_ioextra_t;
struct fat_ioextra_t
{
    enum { started, reading, finished } state;
    FatFile *file;
    uint32_t bytes_read;
    unsigned cluster_index;
    size_t user_length, mod;
    uint32_t *clusters;
    request_dev_t dev_request;
};

void Fat::StartIo(asyncio_t *io)
{
    fat_ioextra_t *extra;
    unsigned bytes;
    uint8_t *buf, *page;
    request_fs_t *req_fs;

    extra = (fat_ioextra_t*) io->extra;
    req_fs = (request_fs_t*) io->req;
    
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
		req_fs->header.code = extra->dev_request.header.code;
		break;
	    }

	    wprintf(L"Fat::StartIo: io finished\n");
	    /* The block is now valid */
	    CcReleaseBlock(extra->file->cache, extra->file->pos, true);
	}

	bytes = extra->user_length - extra->bytes_read;
	if (bytes > m_bytes_per_cluster)
	    bytes = m_bytes_per_cluster;
	
	TRACE4("FatStartIo: pos = %u cluster = %u = %x bytes = %u\n", 
	    (uint32_t) extra->file->pos, 
	    extra->cluster_index, 
	    extra->clusters[extra->cluster_index], 
	    bytes);

	if (CcIsBlockValid(extra->file->cache, extra->file->pos))
	{
	    /* The cluster is already in the file's cache, so just memcpy() it */

	    wprintf(L"Fat::StartIo: found cluster in cache\n");
	    buf = (uint8_t*) DevMapBuffer(io);
	    page = (uint8_t*) CcRequestBlock(extra->file->cache, 
		extra->file->pos);
	    memcpy(buf + io->mod_buffer_start + extra->bytes_read,
		page,
		bytes);
	    DevUnmapBuffer();
	    CcReleaseBlock(extra->file->cache, extra->file->pos, true);

	    extra->mod += bytes;
	    if (extra->mod >= m_bytes_per_cluster)
	    {
		extra->mod -= m_bytes_per_cluster;

		extra->cluster_index++;
		if (extra->cluster_index >= extra->file->num_clusters)
		{
		    /* last cluster reached: invalid chain? */
		    wprintf(L"fat: last cluster reached\n");
		    extra->state = fat_ioextra_t::finished;
		    req_fs->header.result = EEOF;
		    goto start;
		}
	    }

	    extra->file->pos += bytes;
	    extra->bytes_read += bytes;

	    if (extra->bytes_read >= extra->user_length)
	    {
		/*
		 * We've finished, so we'll switch to the 'finished' state 
		 *    for cleanup.
		 */
		extra->state = fat_ioextra_t::finished;
		req_fs->header.result = 0;
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

	    wprintf(L"Fat::StartIo: cluster is not cached\n");

	    buf = (uint8_t*) CcRequestBlock(extra->file->cache, extra->file->pos);

	    /*buf = (uint8_t*) DevMapBuffer(io);*/
	    extra->dev_request.header.code = 
		(req_fs->header.code == FS_READ) ? DEV_READ : DEV_WRITE;
	    /*extra->dev_request.params.buffered.buffer = 
		buf + io->mod_buffer_start + extra->bytes_read;
	    extra->dev_request.params.buffered.length = bytes;
	    extra->dev_request.params.buffered.offset = 
		ClusterToOffset(extra->clusters[extra->cluster_index]) + extra->mod;*/

	    extra->dev_request.params.buffered.buffer = buf;
	    extra->dev_request.params.buffered.length = m_bytes_per_cluster;
	    extra->dev_request.params.buffered.offset = 
		ClusterToOffset(extra->clusters[extra->cluster_index]);
	    extra->dev_request.header.param = io;

	    //wprintf(L"FatStartIo: req = %p\n", &extra->dev_request.header);
	    if (!IoRequest(this, m_device, &extra->dev_request.header))
	    {
		wprintf(L"Fat::StartIo: request failed straight away\n");
		DevUnmapBuffer();
		extra->state = fat_ioextra_t::finished;
		req_fs->header.result = extra->dev_request.header.result;
		goto start;
	    }

	    /*DevUnmapBuffer();*/
	}
	break;

    case fat_ioextra_t::finished:
	TRACE0("FatStartIo: finished request\n");
	req_fs->params.buffered.length = extra->bytes_read;
	HndUnlock(io->owner->process,
	    req_fs->params.buffered.file, 
	    'file');
	free(extra);
	DevFinishIo(io->dev, io, req_fs->header.result);
	break;
    }
}

bool Fat::ReadDir(FatDirectory *dir, request_fs_t *req_fs)
{
    fat_dirent_t *di;
    size_t len;
    dirent_t *buf;
    unsigned count;

    di = (fat_dirent_t*) (dir + 1) + dir->pos;
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
    buf = (dirent_t*) req_fs->params.fs_read.buffer;
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

    HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
    return true;
}

bool Fat::ReadWriteFile(FatFile *file, request_fs_t *req_fs)
{
    asyncio_t *io;
    fat_ioextra_t *extra;
    size_t user_length;
    
    if (file->pos >> m_cluster_shift >= file->num_clusters)
    {
	req_fs->header.result = EEOF;
	req_fs->params.buffered.length = 0;
	HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
	return false;
    }

    user_length = req_fs->params.fs_read.length;
    if (file->pos + user_length >= file->di.file_length &&
	req_fs->header.code == FS_READ)
	user_length = file->di.file_length - file->pos;
    if (user_length == 0)
    {
	TRACE0("fat: null read\n");
	req_fs->header.result = EEOF;
	req_fs->params.buffered.length = 0;
	HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
	return false;
    }

    io = queueRequest(&req_fs->header, sizeof(request_fs_t),
	req_fs->params.buffered.buffer,
	req_fs->params.buffered.length);
    if (io == NULL)
	return false;
    
    io->extra = malloc(sizeof(fat_ioextra_t));
    assert(io->extra != NULL);
    extra = (fat_ioextra_t*) io->extra;
    extra->state = fat_ioextra_t::started;
    extra->file = file;
    extra->cluster_index = file->pos >> m_cluster_shift;
    extra->bytes_read = 0;
    extra->dev_request.header.code = 0;
    
    /* xxx - change this to 64-bit modulo */
    extra->mod = (uint32_t) file->pos % m_bytes_per_cluster;

    extra->clusters = (uint32_t*) (file + 1);
    extra->user_length = user_length;
    
    ((request_fs_t*) io->req)->params.buffered.length = 0;
    extra->state = fat_ioextra_t::reading;
    
    //wprintf(L"Request: starting IO, extra = %p\n", io->extra);
    StartIo(io);
    return true;
}

bool Fat::QueryFile(request_fs_t *req_fs)
{
    wchar_t *path;
    dirent_t *buf;
    fat_dirent_t di;

    path = _wcsdup(req_fs->params.fs_queryfile.name);
    if (!LookupFile(m_root, path + 1, &di))
    {
	req_fs->header.code = ENOTFOUND;
	return false;
    }

    free(path);

    switch (req_fs->params.fs_queryfile.query_class)
    {
    case FILE_QUERY_NONE:
	return true;

    case FILE_QUERY_STANDARD:
	if (req_fs->params.fs_queryfile.buffer_size < sizeof(dirent_t))
	{
	    req_fs->header.code = EBUFFER;
	    return false;
	}

	buf = (dirent_t*) req_fs->params.fs_queryfile.buffer;
	/* xxx - need to do something about this */
	AssembleName(&di, buf->name);
	buf->length = di.file_length;
	buf->standard_attributes = di.attribs;
	return true;
    }
    
    req_fs->header.code = ENOTIMPL;
    return false;
}

bool Fat::request(request_t *req)
{
    request_fs_t *req_fs;
    wchar_t *path, *wildcard;
    fat_dirent_t di;
    FatFile *file;
    FatDirectory *dir;

    req_fs = (request_fs_t*) req;
    switch (req->code)
    {
    case FS_OPEN:
	path = _wcsdup(req_fs->params.fs_open.name);
	if (!LookupFile(m_root, path + 1, &di))
	{
	    req->code = ENOTFOUND;
	    return false;
	}

	TRACE2("fat: opening %s at cluster %u\n", path, di.first_cluster);
	free(path);

	req_fs->params.fs_open.file = AllocFile(&di, 
	    req_fs->params.fs_open.flags);
	return req_fs->params.fs_open.file == NULL ? false : true;

    case FS_QUERYFILE:
	return QueryFile(req_fs);

    case FS_OPENSEARCH:
	path = _wcsdup(req_fs->params.fs_opensearch.name);
	wildcard = wcsrchr(path + 1, '/');
	if (wildcard)
	{
	    *wildcard = '\0';
	    wildcard++;
	    TRACE2("fat: open search: path = %s, wildcard = %s\n",
		path + 1, wildcard);
	    if (!LookupFile(m_root, path + 1, &di))
	    {
		req->code = ENOTFOUND;
		free(path);
		return false;
	    }
	}
	else
	{
	    wildcard = path + 1;
	    memset(&di, 0, sizeof(di));
	    /*root_entry.file_length = m_bpb.num_root_entries * sizeof(fat_dirent_t);*/
	    di.file_length = m_root->num_entries * sizeof(fat_dirent_t);
	    di.first_cluster = FAT_CLUSTER_ROOT;
	    TRACE0("fat: opening search for root directory\n");
	}

	TRACE2("fat: opening search %s at cluster %u\n", path, di.first_cluster);
	free(path);

	req_fs->params.fs_opensearch.file = HndAlloc(NULL, SizeOfDir(&di), 'file');
	dir = (FatDirectory*) HndLock(NULL, req_fs->params.fs_opensearch.file, 'file');
	if (dir == NULL)
	{
	    req->code = EHANDLE;
	    return false;
	}

	if (!ConstructDir(dir, &di))
	{
	    req->code = 1;
	    return false;
	}

	HndUnlock(NULL, req_fs->params.fs_opensearch.file, 'file');
	return true;

    case FS_CLOSE:
	return FreeFile(req_fs->params.fs_close.file);

    case FS_READ:
    case FS_WRITE:
	file = (FatFile*) HndLock(NULL, req_fs->params.fs_read.file, 'file');
	if (file == NULL)
	    return false;

	if (file->is_dir)
	{
	    if (req->code == FS_READ)
		return ReadDir((FatDirectory*) file, req_fs);
	    else
	    {
		HndUnlock(NULL, req_fs->params.fs_write.file, 'file');
		req->code = EACCESS;
		return false;
	    }
	}
	else
	    return ReadWriteFile(file, req_fs);
    }

    req->code = ENOTIMPL;
    return false;
}

bool Fat::isr(uint8_t irq)
{
    return false;
}

void Fat::finishio(request_t *req)
{
    asyncio_t *io;
    fat_ioextra_t *extra;
    
    wprintf(L"fat: finishio: req = %p\n", req);
    /*FOREACH (io, io)
    {*/
    assert(req != NULL);
    assert(req->param != NULL);

    io = (asyncio_t*) req->param;
    assert(io != NULL);

    extra = (fat_ioextra_t*) io->extra;
    assert(extra != NULL);

    assert(req == &extra->dev_request.header);
    StartIo(io);
    /*}*/

    /*assert(false && "Request not found");
    return false;*/
}

device_t *Fat::Init(driver_t *drv, const wchar_t *path, device_t *dev)
{
    fat_dirent_t root_entry;
    unsigned length, temp;
    uint32_t RootDirSectors, FatSectors;
    //wchar_t name[FAT_MAX_NAME];

    driver = drv;
    m_device = dev;

    TRACE0("\tReading boot sector\n");
    length = IoReadSync(dev, 0, &m_bpb, sizeof(m_bpb));
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
	m_bpb.sectors_per_fat * 
	m_bpb.bytes_per_sector);
    m_fat = (uint8_t*) malloc(m_bpb.sectors_per_fat * m_bpb.bytes_per_sector);
    assert(m_fat != NULL);

    TRACE3("\tFAT starts at sector %d = 0x%x = %p\n",
	m_bpb.reserved_sectors,
	m_bpb.reserved_sectors * 
	    m_bpb.bytes_per_sector,
	m_fat);

    length = m_bpb.sectors_per_fat * 
	m_bpb.bytes_per_sector;
    TRACE0("\tReading FAT\n");
    if (IoReadSync(m_device, 
	m_bpb.reserved_sectors * 
	    m_bpb.bytes_per_sector,
	m_fat,
	length) != length)
    {
	free(m_fat);
	delete this;
	return NULL;
    }

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
    m_root = (FatDirectory*) malloc(SizeOfDir(&root_entry));
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

device_t *FatMountFs(driver_t *drv, const wchar_t *path, device_t *dev)
{
    Fat *root;
    
    root = new Fat;
    if (root == NULL)
	return NULL;

    return root->Init(drv, path, dev);
}

extern "C" bool DrvInit(driver_t *drv)
{
    drv->add_device = NULL;
    drv->mount_fs = FatMountFs;
    return true;
}