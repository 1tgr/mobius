#include <string.h>
#include "mobel.h"

/*////////////////////////////////////////////////////////////////////////////
	VFS
////////////////////////////////////////////////////////////////////////////*/
/*****************************************************************************
read directory entry from directory 'dir'
stores entry at 'file' (we use file_t instead of a DIR type)
*****************************************************************************/
int my_readdir(file_t *dir, file_t *file)
{
	fsinfo_t *fsinfo;
	mount_t *mount;

	mount = dir->mount;
	fsinfo = &mount->fsinfo;
	return fsinfo->readdir(dir, file);
}
/*****************************************************************************
find file 'name' in already-opened directory 'dir';
stores result at 'file' if success
*****************************************************************************/
int my_find(file_t *dir, file_t *file, char *name)
{
	int err;

/* seek to beginning of dir */
	err = my_seek(dir, 0, SEEK_SET);
	if(err != 0)
		return err;
/* read one 32-byte directory entry */
	do
	{
		err = my_readdir(dir, file);
		if(err != 0)
			return err;
	} while(strcmp(name, file->name));
	return 0;
}
/*****************************************************************************
convert path from relative to absolute, if necessary?
remove superfluous components of path (e.g. /./ )?
figure out mount point based on absolute path?
*****************************************************************************/
int my_open(file_t *file, char *path)
{
	char done = 0, *slash;
	file_t search_dir;
	fsinfo_t *fsinfo;
	mount_t *mount;
	int err;

	file->mount = mount = &_mount;
	fsinfo = &mount->fsinfo;
	if(path[0] == '/')
	{
/* skip first '/' */
		path++;
/* open root dir */
		err = fsinfo->open_root(&search_dir, mount);
		if(err != 0)
			return err;
/* done already? */
		if(path[0] == '\0')
		{
			memcpy(file, &search_dir, sizeof(file_t));
			return 0;
		}
	}
/* xxx - relative pathnames are not yet handled */
	else
		return -1;
	done = 0;
	do
	{
/* pick out one name in path */
		slash = strchr(path, '/');
		if(slash == NULL)
		{
			slash = path + strlen(path);
			done = 1;
		}
		*slash = '\0';
/* find file/dir of this name in search_dir */
		err = my_find(&search_dir, file, path);
		if(err != 0)
		{
			my_close(&search_dir);
			return err;
		}
		if(done)
			break;
/* CD to subdirectory */
		memcpy(&search_dir, file, sizeof(file_t));
/* advance to next name+ext pair in pathname
+1 to skip '/' */
		path = slash + 1;
	} while(path[0] != '\0');
	file->cur_inode = (inode_t) -1;
	file->cur_sector = (sector_t) -1;
	return 0;
}
/*****************************************************************************
*****************************************************************************/
int my_read(file_t *file, void *buf, unsigned len)
{
	fsinfo_t *fsinfo;
	mount_t *mount;

	mount = file->mount;
	fsinfo = &mount->fsinfo;
	return fsinfo->read(file, buf, len);
}
/*****************************************************************************
*****************************************************************************/
int my_seek(file_t *file, long offset, int whence)
{
	switch(whence)
	{
		case SEEK_SET:
			/* nothing */
			break;
		case SEEK_CUR:
			offset += file->pos;
			break;
		case SEEK_END:
			offset = (file->file_size - 1) - offset;
			break;
		default:
			return -1;
	}
	if(offset < 0)
		offset = 0;
	else if(offset >= file->file_size - 1)
		offset = file->file_size - 1;
	file->pos = offset;
	return 0;
}
/*****************************************************************************
*****************************************************************************/
#pragma argsused
int my_close(file_t *file)
{
	return 0;
}