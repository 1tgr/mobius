/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/fs.h>

#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <wchar.h>
#include <string.h>
#include <ctype.h>


#pragma pack(push, 1)
typedef struct both32_t both32_t;
struct both32_t
{
	uint32_t native;	/* little endian */
	uint32_t foreign;	/* big endian */
};


typedef struct both16_t both16_t;
struct both16_t
{
	uint16_t native;	/* little endian */
	uint16_t foreign;	/* big endian */
};


typedef struct path_t path_t;
struct path_t
{
	uint8_t name_length;
	uint8_t num_extended_sectors;
	uint32_t first_sector;
	uint16_t parent_record_num;
	uint8_t name[1];
};


typedef struct entry_t entry_t;
struct entry_t
{
	uint8_t struct_size;
	uint8_t num_extended_sectors;
	both32_t first_sector;
	both32_t length;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t gmt_offset;
	uint8_t flags;
	uint8_t unit_size;
	uint8_t interleave_gap;
	both16_t volume_seq_num;
	uint8_t id_length;
	uint8_t id[1];
};

#define DIR_FLAG_HIDDEN		1
#define DIR_FLAG_DIRECTORY	2
#define DIR_FLAG_ASSOC		4
#define DIR_FLAG_FORMAT		8
#define DIR_FLAG_MORE		127

#define MAX_NAME_LENGTH		32


typedef struct date_time_t date_time_t;
struct date_time_t
{
	uint8_t year[4];
	uint8_t month[2];
	uint8_t day[2];
	uint8_t hour[2];
	uint8_t minute[2];
	uint8_t second[2];
	uint8_t sec100[2];
	int8_t gmt_offset;
};


typedef struct pvd_t pvd_t;
struct pvd_t
{
	uint8_t id[8];
	char system_id[32];
	char volume_id[32];
	uint8_t zero1[8];
	both32_t num_sectors;
	uint8_t zero2[32];
	both16_t volume_set_size;
	both16_t volume_seq_num;
	both16_t sector_size;
	both32_t path_table_length;
	uint32_t le_path_table_1_sector;
	uint32_t le_path_table_2_sector;
	uint32_t be_path_table_1_sector;
	uint32_t be_path_table_2_sector;
	union
	{
		entry_t entry;
		uint8_t padding[34];
	} root;
	uint8_t volume_set_id[128];
	uint8_t publisher_id[128];
	uint8_t data_prep_id[128];
	uint8_t app_id[128];
	uint8_t copyright_file[37];
	uint8_t abstract_file[37];
	uint8_t biblio_file[37];
	date_time_t create_date;
	date_time_t modify_date;
	date_time_t expire_date;
	date_time_t effective_date;
	uint8_t reserved[1167];
};
#pragma pack(pop)

#define SECTOR_SIZE 2048

CASSERT(sizeof(date_time_t) == 17);
CASSERT(sizeof(((pvd_t*) 0)->root) == 34);
CASSERT(sizeof(pvd_t) == SECTOR_SIZE);


typedef struct cdnode_t cdnode_t;
struct cdnode_t
{
	cdnode_t *next;
	vnode_id_t id;
	int locks;
	vnode_id_t parent;
	entry_t entry;
	wchar_t *name;
	file_handle_t *directory;
};


typedef struct cdsearch_t cdsearch_t;
struct cdsearch_t
{
	cdnode_t *node;
	uint32_t offset;
	unsigned index;
};


typedef struct cdfs_t cdfs_t;
struct cdfs_t
{
	fsd_t fsd;
	device_t *device;
	cdnode_t *nodes[128];
};

static cdnode_t *CdfsGetNode(cdfs_t *cdfs, vnode_id_t id)
{
	cdnode_t *cdnode;

	cdnode = cdfs->nodes[id % _countof(cdfs->nodes)];
	while (cdnode != NULL)
    {
        if (cdnode->id == id)
        {
            KeAtomicInc(&cdnode->locks);
            return cdnode;
        }

		cdnode = cdnode->next;
    }

	return NULL;
}


static void CdfsReleaseNode(cdfs_t *cdfs, cdnode_t *cdnode)
{
	KeAtomicDec(&cdnode->locks);
}


static vnode_id_t CdfsGenerateNodeId(cdfs_t *cdfs,
									 vnode_id_t parent, 
									 unsigned index_within_parent)
{
	if (parent == VNODE_NONE)
		return index_within_parent;
    else
	{
		cdnode_t *pnode;
		vnode_id_t id;

		pnode = CdfsGetNode(cdfs, parent);
		id = (pnode->entry.first_sector.native << 6) | index_within_parent;
		CdfsReleaseNode(cdfs, pnode);

		return id;
	}
}


static vnode_id_t CdfsAllocNode(cdfs_t *cdfs, 
								vnode_id_t parent, 
								unsigned index_within_parent, 
								const entry_t *entry,
								const wchar_t *name)
{
    vnode_id_t id;
    cdnode_t *cdnode;

	id = CdfsGenerateNodeId(cdfs, parent, index_within_parent);

    cdnode = CdfsGetNode(cdfs, id);
	if (cdnode != VNODE_NONE)
	{
		CdfsReleaseNode(cdfs, cdnode);
		return id;
	}

	cdnode = malloc(sizeof(*cdnode));
	if (cdnode == NULL)
		return VNODE_NONE;

    cdnode->id = id;
    cdnode->locks = 0;
    cdnode->parent = parent;
    cdnode->entry = *entry;
	cdnode->name = _wcsdup(name);
	cdnode->directory = NULL;
	cdnode->next = cdfs->nodes[cdnode->id % _countof(cdfs->nodes)];
	cdfs->nodes[id % _countof(cdfs->nodes)] = cdnode;

	if (cdnode->entry.flags & DIR_FLAG_DIRECTORY)
	{
		vnode_t vnode_this = { &cdfs->fsd, id };

		cdnode->directory = FsOpen(&vnode_this, L"/", FILE_READ);
		if (cdnode->directory == NULL)
			wprintf(L"CdfsAllocVnode: failed to open %x as a directory\n", id);
	}

    return id;
}


void CdfsDismount(fsd_t *fsd)
{
	cdfs_t *cdfs;

	cdfs = (cdfs_t*) fsd;
}


void CdfsGetFsInfo(fsd_t *fsd, fs_info_t *info)
{
	cdfs_t *cdfs;

	cdfs = (cdfs_t*) fsd;
	if (info->flags & FS_INFO_CACHE_BLOCK_SIZE)
		info->cache_block_size = SECTOR_SIZE;
	if (info->flags & FS_INFO_SPACE_TOTAL)
		info->space_total = 0;
	if (info->flags & FS_INFO_SPACE_FREE)
		info->space_free = 0;
}


status_t CdfsParseElement(fsd_t *fsd, const wchar_t *name, 
						  wchar_t **new_path, vnode_t *node)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;
	entry_t entry;
	uint32_t offset, sector;
	uint8_t entry_name[MAX_NAME_LENGTH];
	wchar_t entry_name_wide[MAX_NAME_LENGTH + 1];
	unsigned i, index;
	size_t bytes_read;

	cdfs = (cdfs_t*) fsd;
	cdnode = CdfsGetNode(cdfs, node->id);
	if (cdnode == NULL)
	{
		wprintf(L"CdfsParseElement(%x/%s): invalid parent node\n",
			node->id, name);
		return ENOTFOUND;
	}

	if (cdnode->directory == NULL)
	{
        wprintf(L"CdfsParseElement(%x/%s): parent is not a directory\n", 
			node->id, name);
        return ENOTADIR;
    }

	offset = 0;
	sector = 0;
	index = 0;
	while (true)
	{
		if (!FsRead(cdnode->directory, 
			&entry, 
			offset, 
			offsetof(entry_t, id), 
			&bytes_read))
			return errno;

		if (bytes_read < offsetof(entry_t, id) ||
			entry.struct_size == 0)
			break;

		assert(entry.id_length < _countof(entry_name));

		if (index >= 2)
		{
			if (!FsRead(cdnode->directory, 
				entry_name,
				offset + offsetof(entry_t, id), 
				min(_countof(entry_name), entry.id_length),
				&bytes_read))
				return errno;

			if (bytes_read < min(_countof(entry_name) - 1, entry.id_length))
				break;

			for (i = 0; entry_name[i] != ';' && i < entry.id_length; i++)
				entry_name_wide[i] = (wchar_t) (unsigned char) tolower(entry_name[i]);

			entry_name_wide[i] = '\0';

			if (_wcsicmp(entry_name_wide, name) == 0)
			{
				//cdnode_t *temp;
				//wprintf(L"CdfsParseElement(%x/%s): ", node->id, name);
				node->id = CdfsAllocNode(cdfs, node->id, index, &entry, entry_name_wide);
				CdfsReleaseNode(cdfs, cdnode);
				//temp = CdfsGetNode(cdfs, node->id);
				//wprintf(L"%p %x\n", temp->name, node->id);
				//CdfsReleaseNode(cdfs, temp);
				return 0;
			}
		}

		offset += entry.struct_size;
		if (offset / SECTOR_SIZE != sector)
		{
			assert(offset / SECTOR_SIZE < sector + 1);
			sector++;
			offset = sector * SECTOR_SIZE;
		}

		index++;
	}

	CdfsReleaseNode(cdfs, cdnode);
	return ENOTFOUND;
}


status_t CdfsCreateFile(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, 
						void **cookie)
{
	cdfs_t *cdfs;

	cdfs = (cdfs_t*) fsd;
	return EACCESS;
}


status_t CdfsLookupFile(fsd_t *fsd, vnode_id_t node, uint32_t open_flags, 
						void **cookie)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;

	cdfs = (cdfs_t*) fsd;
	cdnode = CdfsGetNode(cdfs, node);
	if (cdnode == NULL)
	{
		wprintf(L"CdfsLookupFile(%x): not found\n",
			node);
		return EINVALID;
	}

	*cookie = cdnode;
	return 0;
}


status_t CdfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;
	dirent_all_t *di;

	cdfs = (cdfs_t*) fsd;
	cdnode = cookie;
	di = buf;

	switch (type)
	{
	case FILE_QUERY_NONE:
		return 0;

	case FILE_QUERY_DIRENT:
		di->dirent.vnode = cdnode->id;
		wcsncpy(di->dirent.name, cdnode->name, _countof(di->dirent.name));
		return 0;

	case FILE_QUERY_STANDARD:
		di->standard.length = cdnode->entry.length.native;

		di->standard.attributes = 0;
		if (cdnode->entry.flags & DIR_FLAG_DIRECTORY)
			di->standard.attributes |= FILE_ATTR_DIRECTORY;
		if (cdnode->entry.flags & DIR_FLAG_HIDDEN)
			di->standard.attributes |= FILE_ATTR_HIDDEN;

		FsGuessMimeType(wcsrchr(cdnode->name, '.'),
			di->standard.mimetype,
			_countof(di->standard.mimetype));
		return 0;
	}

	return ENOTIMPL;
}


status_t CdfsSetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, 
						 const void *buf)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;

	cdfs = (cdfs_t*) fsd;
	cdnode = cookie;
	return EACCESS;
}


void CdfsFreeCookie(fsd_t *fsd, void *cookie)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;

	cdfs = (cdfs_t*) fsd;
	cdnode = cookie;
	CdfsReleaseNode(cdfs, cdnode);
}


status_t CdfsReadWriteFile(fsd_t *fsd, const fs_request_t *req, size_t *bytes)
{
	cdfs_t *cdfs;
    cdnode_t *cdnode;
	io_callback_t cb;
	request_dev_t *dev_request;
	int length;
	uint32_t aligned_file_length;

	cdfs = (cdfs_t*) fsd;
    cdnode = req->file->fsd_cookie;

	//wprintf(L"CdfsReadWriteFile(%x): %u bytes at %u\n",
		//cdnode->id, req->length, (unsigned) req->pos);

	if (!req->is_reading)
	{
		wprintf(L"CdfsReadWriteFile: read-only\n");
		return EACCESS;
	}

	aligned_file_length = (cdnode->entry.length.native + SECTOR_SIZE - 1) & -SECTOR_SIZE;
	if (req->pos >= aligned_file_length)
	{
		wprintf(L"CdfsReadWriteFile: read past end of file (aligned_file_length = %u)\n",
			aligned_file_length);
		return EEOF;
	}

	if (req->pos + req->length >= aligned_file_length)
		length = aligned_file_length - req->pos;
	else
		length = req->length;

	assert((req->pos & (SECTOR_SIZE - 1)) == 0);
	assert((length & (SECTOR_SIZE - 1)) == 0);
	assert(length == SECTOR_SIZE);

	dev_request = malloc(sizeof(*dev_request));
	if (dev_request == NULL)
		return errno;

    dev_request->header.code = DEV_READ;
    dev_request->params.buffered.pages = req->pages;
    dev_request->params.buffered.length = length;
    dev_request->params.buffered.offset = 
		cdnode->entry.first_sector.native * SECTOR_SIZE + 
		req->pos;
    dev_request->header.param = req->io;

	//wprintf(L"\t=> %u bytes at 0x%x\n", 
		//dev_request->params.buffered.length, 
		//dev_request->params.buffered.offset);

    cb.type = IO_CALLBACK_FSD;
    cb.u.fsd = &cdfs->fsd;
    if (!IoRequest(&cb, cdfs->device, &dev_request->header))
    {
		status_t ret;
        wprintf(L"CdfsReadWriteFile: request failed straight away\n");
        *bytes = dev_request->params.buffered.length;
		ret = dev_request->header.result;
		free(dev_request);
		return ret;
    }

	return SIOPENDING;
}


bool CdfsIoctlFile(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
				   size_t length, fs_asyncio_t *io)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;

	cdfs = (cdfs_t*) fsd;
	cdnode = file->fsd_cookie;
	return false;
}


bool CdfsPassthrough(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
					 size_t length, fs_asyncio_t *io)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;

	cdfs = (cdfs_t*) fsd;
	cdnode = file->fsd_cookie;
	return false;
}


status_t CdfsMkDir(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, 
				   void **dir_cookie)
{
	cdfs_t *cdfs;

	cdfs = (cdfs_t*) fsd;
	return EACCESS;
}


status_t CdfsOpenDir(fsd_t *fsd, vnode_id_t dir, void **dir_cookie)
{
	cdfs_t *cdfs;
	cdnode_t *cdnode;
	cdsearch_t *search;

	cdfs = (cdfs_t*) fsd;

    cdnode = CdfsGetNode(cdfs, dir);
    if (cdnode == NULL)
    {
        wprintf(L"CdfsOpenDir: node %x not found\n", dir);
        return ENOTFOUND;
    }

    if (cdnode->directory == NULL)
    {
        wprintf(L"CdfsOpenDir: node %x is not a directory\n", dir);
        CdfsReleaseNode(cdfs, cdnode);
        return ENOTADIR;
    }

    search = malloc(sizeof(*search));
    if (search == NULL)
    {
        CdfsReleaseNode(cdfs, cdnode);
        return errno;
    }

    search->node = cdnode;
    search->offset = 0;
	search->index = 0;
    *dir_cookie = search;
    return 0;
}


status_t CdfsReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
	cdfs_t *cdfs;
	cdsearch_t *search;
	entry_t entry;
	uint32_t sector;
	size_t bytes_read;
	unsigned i;
	char entry_name[MAX_NAME_LENGTH];
	wchar_t entry_name_wide[MAX_NAME_LENGTH + 1];
	bool found;

	cdfs = (cdfs_t*) fsd;
	search = dir_cookie;

	sector = search->offset / SECTOR_SIZE;
	found = false;
	while (!found)
	{
		if (!FsRead(search->node->directory, 
			&entry, 
			search->offset, 
			offsetof(entry_t, id), 
			&bytes_read))
			return errno;

		if (bytes_read < offsetof(entry_t, id))
			return EHARDWARE;

		if (entry.struct_size == 0)
			return EEOF;

		assert(entry.id_length < _countof(entry_name));

		if (search->index >= 2 && entry.id_length > 0)
		{
			if (!FsRead(search->node->directory, 
				entry_name,
				search->offset + offsetof(entry_t, id), 
				min(_countof(entry_name), entry.id_length),
				&bytes_read))
				return errno;

			if (bytes_read < min(_countof(entry_name) - 1, entry.id_length))
				return EHARDWARE;

			for (i = 0; entry_name[i] != ';' && i < entry.id_length; i++)
				entry_name_wide[i] = (wchar_t) (unsigned char) tolower(entry_name[i]);

			entry_name_wide[i] = '\0';

			buf->vnode = CdfsGenerateNodeId(cdfs, search->node->id, search->index);
			wcsncpy(buf->name, entry_name_wide, _countof(buf->name));
			found = true;
		}

		search->offset += entry.struct_size;
		if (search->offset / SECTOR_SIZE != sector)
		{
			assert(search->offset / SECTOR_SIZE < sector + 1);
			sector++;
			search->offset = sector * SECTOR_SIZE;
		}

		search->index++;
	}

	return 0;
}


void CdfsFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
	cdfs_t *cdfs;
	cdsearch_t *search;

	cdfs = (cdfs_t*) fsd;
	search = dir_cookie;
	CdfsReleaseNode(cdfs, search->node);
	free(search);
}


void CdfsFinishIo(fsd_t *fsd, request_t *req)
{
	cdfs_t *cdfs;
	request_dev_t *dev_request;

	cdfs = (cdfs_t*) fsd;

    assert(req != NULL);
    assert(req->param != NULL);
    dev_request = (request_dev_t*) req;

    if (dev_request->header.result != 0)
        wprintf(L"cdfs: device failure: %d\n", dev_request->header.result);

	FsNotifyCompletion(req->param, 
		dev_request->params.buffered.length, 
		dev_request->header.result);
	free(req);
}


void CdfsFlushCache(fsd_t *fsd, file_t *fd)
{
	cdfs_t *cdfs;

	cdfs = (cdfs_t*) fsd;
}


static const vtbl_fsd_t cdfs_vtbl =
{
	CdfsDismount,
	CdfsGetFsInfo,
	CdfsParseElement,
	CdfsCreateFile,
	CdfsLookupFile,
	CdfsGetFileInfo,
	CdfsSetFileInfo,
	CdfsFreeCookie,
	CdfsReadWriteFile,
	CdfsIoctlFile,
	CdfsPassthrough,
	CdfsMkDir,
	CdfsOpenDir,
	CdfsReadDir,
	CdfsFreeDirCookie,
	CdfsFinishIo,
	CdfsFlushCache,
};


fsd_t *CdfsMountFs(driver_t *drv, const wchar_t *dest)
{
    cdfs_t *cdfs;
	pvd_t pvd;
	vnode_id_t root_id;
	char *ch;

	cdfs = malloc(sizeof(*cdfs));
	if (cdfs == NULL)
		goto error0;

	memset(cdfs, 0, sizeof(*cdfs));
	cdfs->fsd.vtbl = &cdfs_vtbl;
	cdfs->device = IoOpenDevice(dest);
	if (cdfs->device == NULL)
	{
		wprintf(L"cdfs(%s): failed to open device\n", dest);
		goto error1;
	}

	if (IoReadSync(cdfs->device, 16 * SECTOR_SIZE, &pvd, sizeof(pvd)) < sizeof(pvd))
	{
		wprintf(L"cdfs(%s): failed to read PVD\n", dest);
		goto error1;
	}

	for (ch = pvd.volume_id; 
		*ch != ' ' && ch < pvd.volume_id + _countof(pvd.volume_id) - 1; 
		ch++)
		;

	*ch = '\0';
	wprintf(L"CdfsMountFs: mounted '%S' from %s\n", pvd.volume_id, dest);
	root_id = CdfsAllocNode(cdfs, VNODE_NONE, 1, &pvd.root.entry, L"");
	assert(root_id == VNODE_ROOT);
    return &cdfs->fsd;

error1:
	free(cdfs);
error0:
	return NULL;
}


bool DrvInit(driver_t *drv)
{
    drv->add_device = NULL;
    drv->mount_fs = CdfsMountFs;
    return true;
}
