/*
 * $History: ramdisk_tar.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 13:56
 * Updated in $/coreos/kernel
 * Corrected file ID assignment
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>

typedef struct tar_file_t tar_file_t;

/*
 * TAR file format from Max Maischein <Max_Maischein@spam.fido.de>
 */

/*
 * Standard Archive Format - Standard TAR - USTAR
 */
#define  RECORDSIZE  512
#define  NAMSIZ      100
#define  TUNMLEN      32
#define  TGNMLEN      32

typedef union record record_t;
union record {
    char	charptr[RECORDSIZE];
    struct header {
	char    name[NAMSIZ];
	char    mode[8];
	char    uid[8];
	char    gid[8];
	char    size[12];
	char    mtime[12];
	char    chksum[8];
	char    linkflag;
	char    linkname[NAMSIZ];
	char    magic[8];
	char    uname[TUNMLEN];
	char    gname[TGNMLEN];
	union
	{
	    struct
	    {
		char    major[8];
		char    minor[8];
	    } dev;
	    struct
	    {
		tar_file_t *file;
	    } mobius;
	} extra;
    } header;
};

/* The checksum field is filled with this while the checksum is computed. */
#define    CHKBLANKS    "	"	/* 8 blanks, no null */

/* The magic field is filled with this if uname and gname are valid. */
#define    TMAGIC    "ustar  "	/* 7 chars and a null */

/* The magic field is filled with this if this is a GNU format dump entry */
#define    GNUMAGIC  "GNUtar "	/* 7 chars and a null */

/* The linkflag defines the type of file */
#define  LF_OLDNORMAL '\0'       /* Normal disk file, Unix compatible */
#define  LF_NORMAL    '0'	/* Normal disk file */
#define  LF_LINK      '1'	/* Link to previously dumped file */
#define  LF_SYMLINK   '2'	/* Symbolic link */
#define  LF_CHR       '3'	/* Character special file */
#define  LF_BLK       '4'	/* Block special file */
#define  LF_DIR       '5'	/* Directory */
#define  LF_FIFO      '6'	/* FIFO special file */
#define  LF_CONTIG    '7'	/* Contiguous file */

/* Further link types may be defined later. */

/* Bits used in the mode field - values in octal */
#define  TSUID    04000	/* Set UID on execution */
#define  TSGID    02000	/* Set GID on execution */
#define  TSVTX    01000	/* Save text (sticky bit) */

/* File permissions */
#define  TUREAD   00400	/* read by owner */
#define  TUWRITE  00200	/* write by owner */
#define  TUEXEC   00100	/* execute/search by owner */
#define  TGREAD   00040	/* read by group */
#define  TGWRITE  00020	/* write by group */
#define  TGEXEC   00010	/* execute/search by group */
#define  TOREAD   00004	/* read by other */
#define  TOWRITE  00002	/* write by other */
#define  TOEXEC   00001	/* execute/search by other */


struct tar_file_t
{
    tar_file_t *prev, *next;
    tar_file_t *child_first, *child_last;
    tar_file_t *hash_next;
    vnode_id_t id;
    record_t *record;
};


typedef struct tar_file_dynamic_t tar_file_dynamic_t;
struct tar_file_dynamic_t
{
    tar_file_t file;
    record_t record;
};


typedef struct tar_t tar_t;
struct tar_t
{
    fsd_t fsd;
    record_t *data, *end;
    record_t root_record;
    tar_file_t root;
    unsigned num_files;
    tar_file_t *files[128];
    vnode_id_t next_id;
};


static tar_file_t *TarNodeToFile(tar_t *tar, vnode_id_t node)
{
    if (node == VNODE_ROOT)
	return &tar->root;
    else
    {
	tar_file_t *file;

	for (file = tar->files[node % _countof(tar->files)]; file != NULL; file = file->hash_next)
	    if (file->id == node)
		return file;

	return NULL;
    }
}


static vnode_id_t TarInsertFile(tar_t *tar, tar_file_t *file)
{
    unsigned hash;
    file->id = KeAtomicInc((unsigned*) &tar->next_id);
    hash = file->id % _countof(tar->files);
    file->hash_next = tar->files[hash];
    tar->files[hash] = file;
    return file->id;
}


static void TarGetFileName(wchar_t *buf, size_t size, const record_t *record)
{
    const char *slash;
    size_t len;

    slash = strrchr(record->header.name, '/');
    if (slash == NULL)
	slash = record->header.name;
    else
	slash++;

    len = mbstowcs(buf, slash, size);
    if (len >= size - 1)
	buf[size] = '\0';
    else
	buf[len] = '\0';
}


static size_t TarGetFileLength(const record_t *record)
{
    unsigned i;
    wchar_t size[13];

    for (i = 0; i < _countof(record->header.size); i++)
	size[i] = record->header.size[i];
    size[12] = '\0';

    return wcstoul(size, NULL, 8);
}


status_t TarParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
{
    tar_t *tar;
    tar_file_t *file;
    wchar_t wc_name[NAMSIZ];

    tar = (tar_t*) fsd;
    file = TarNodeToFile(tar, node->id);
    if (file == NULL)
	return EINVALID;

    for (file = file->child_first; file != NULL; file = file->next)
    {
	TarGetFileName(wc_name, _countof(wc_name), file->record);

	if (_wcsicmp(name, wc_name) == 0)
	{
	    node->id = file->id;
	    return 0;
	}
    }

    return ENOTFOUND;
}


status_t TarLookupFile(fsd_t *fsd, vnode_id_t id, uint32_t open_flags, void **cookie)
{
    tar_t *tar;
    tar_file_t *file;

    tar = (tar_t*) fsd;
    file = TarNodeToFile(tar, id);
    if (file == NULL)
	return EINVALID;
    else
    {
	*cookie = file;
	return 0;
    }
}


status_t TarGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    dirent_all_t* di;
    tar_file_t *file;
    wchar_t name[NAMSIZ], *ext;

    di = buf;
    file = cookie;

    switch (type)
    {
    case FILE_QUERY_NONE:
	return 0;

    case FILE_QUERY_DIRENT:
	TarGetFileName(di->dirent.name, _countof(di->dirent.name), file->record);
	di->dirent.vnode = VNODE_NONE;
	return 0;

    case FILE_QUERY_STANDARD:
	TarGetFileName(name, _countof(name), file->record);
	di->standard.length = TarGetFileLength(file->record);

	switch (file->record->header.linkflag)
	{
	case LF_LINK:
	case LF_SYMLINK:
	    di->standard.attributes = FILE_ATTR_LINK;
	    break;

	case LF_DIR:
	    di->standard.attributes = FILE_ATTR_DIRECTORY;
	    break;

	default:
	    di->standard.attributes = 0;
	    break;
	}

	ext = wcsrchr(name, '.');
	FsGuessMimeType(ext, di->standard.mimetype, _countof(di->standard.mimetype));
	return 0;
    }

    return ENOTIMPL;
}


status_t TarReadWrite(fsd_t *fsd, const fs_request_t *req, size_t *bytes)
{
    tar_t *tar;
    tar_file_t *file;
    void *ptr, *dest;
    size_t length, file_length;

    tar = (tar_t*) fsd;
    file = req->file->fsd_cookie;

    *bytes = 0;
    length = req->length;
    file_length = TarGetFileLength(file->record);
    if (req->pos + length >= file_length)
	length = file_length - req->pos;

    if (length == 0)
	return EEOF;

    ptr = file->record[1].charptr + req->pos;
    dest = MemMapPageArray(req->pages);
    memcpy(dest, ptr, length);
    MemUnmapPageArray(req->pages);
    FsNotifyCompletion(req->io, length, 0);
    return 0;
}


status_t TarCreateDir(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, void **cookie)
{
    tar_file_t **index, *parent;
    tar_file_dynamic_t *child;
    tar_t *tar;

    tar = (tar_t*) fsd;

    parent = TarNodeToFile(tar, dir);
    if (parent == NULL)
	return EINVALID;

    child = malloc(sizeof(*child));
    if (child == NULL)
	return errno;

    index = malloc(sizeof(tar_file_t*));
    if (index == NULL)
    {
	free(child);
	return errno;
    }

    memset(child, 0, sizeof(*child));
    child->file.record = &child->record;
    wcstombs(child->record.header.name, name, sizeof(child->record.header.name));
    child->record.header.linkflag = LF_DIR;
    child->record.header.extra.mobius.file = &child->file;

    LIST_ADD(parent->child, (&child->file));
    TarInsertFile(tar, &child->file);

    *index = NULL;
    *cookie = index;
    return 0;
}


status_t TarOpenDir(fsd_t *fsd, vnode_id_t node, void **cookie)
{
    tar_file_t **index, *dir;
    tar_t *tar;

    tar = (tar_t*) fsd;

    dir = TarNodeToFile(tar, node);
    if (dir == NULL)
	return EINVALID;

    index = malloc(sizeof(tar_file_t*));
    if (index == NULL)
	return errno;

    *index = dir->child_first;
    *cookie = index;
    return 0;
}


status_t TarReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    tar_file_t **index;

    index = dir_cookie;
    if (*index == NULL)
	return EEOF;

    TarGetFileName(buf->name, _countof(buf->name), (*index)->record);
    buf->vnode = VNODE_NONE;
    *index = (*index)->next;
    return 0;
}


void TarFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    free(dir_cookie);
}


static const struct vtbl_fsd_t tar_vtbl =
{
    NULL,	   /* dismount */
    NULL,	   /* get_fs_info */
    TarParseElement,
    NULL,	   /* create_file */
    TarLookupFile,
    TarGetFileInfo,
    NULL,	   /* set_file_info */
    NULL,	   /* free_cookie */
    TarReadWrite,
    NULL,	   /* ioctl */
    NULL,	   /* passthrough */
    TarCreateDir,   /* mkdir */
    TarOpenDir,
    TarReadDir,
    TarFreeDirCookie,
    NULL,	   /* flush_cache */
};


static record_t *TarParseDirectory(tar_t *tar, tar_file_t *parent, record_t *first, record_t *eof)
{
    //static int nesting;
    record_t *record, *next;
    size_t length;
    tar_file_t *file;

    record = first;
    while (record < eof)
    {
	if (strncmp(record->header.name, 
	    parent->record->header.name, 
	    strlen(parent->record->header.name)) != 0)
	    break;

	length = TarGetFileLength(record);
	next = record + 1 + (length + sizeof(record_t) - 1) / sizeof(record_t);

	if (record->header.name[0] == '\0')
	    record = next;
	else
	{
	    file = malloc(sizeof(*file));
	    file->child_first = file->child_last = NULL;
	    file->record = record;
	    record->header.extra.mobius.file = file;
	    LIST_ADD(parent->child, file);
	    TarInsertFile(tar, file);

	    //wprintf(L"%*s %S, %u bytes: ", nesting, L"", record->header.name, length);
	    if (record->header.linkflag == LF_DIR)
	    {
		char *slash;

		//wprintf(L"directory\n");
		slash = strrchr(record->header.name, '/');
		if (slash != NULL)
		    *slash = '\0';

		//nesting++;
		record = TarParseDirectory(tar, file, next, eof);
		//nesting--;
	    }
	    else
	    {
		//wprintf(L"file\n");
		record = next;
	    }
	}
    }

    return record;
}


fsd_t* TarMountFs(driver_t* driver, const wchar_t *dest)
{
    tar_t *tar;
    record_t *data;
    size_t length;

    data = PHYSICAL(RdGetFilePhysicalAddress(dest, &length));
    if (data == NULL)
    {
	wprintf(L"tarfs: can't find tar file %s\n", dest);
	return NULL;
    }

    tar = malloc(sizeof(*tar));
    if (tar == NULL)
	return NULL;

    memset(tar, 0, sizeof(*tar));

    tar->fsd.vtbl = &tar_vtbl;
    tar->data = data;
    tar->end = (record_t*) ((uint8_t*) data + length);

    tar->root_record.header.linkflag = LF_DIR;
    tar->root_record.header.extra.mobius.file = &tar->root;
    tar->root.record = &tar->root_record;

    tar->next_id = VNODE_ROOT;
    TarInsertFile(tar, &tar->root);

    data = TarParseDirectory(tar, &tar->root, tar->data, tar->end);
    assert(data == tar->end);

    return &tar->fsd;
}


driver_t tarfs_driver =
{
    NULL,
    NULL,
    NULL,
    NULL,
    TarMountFs
};
