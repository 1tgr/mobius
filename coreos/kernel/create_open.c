/*
 * $History: create_open.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:29
 * Updated in $/coreos/kernel
 * Put opened/closed file lookup code in FsFindOpenFile, in file.c
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 7/03/04    Time: 19:04
 * Created in $/coreos/kernel
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

#include <string.h>
#include <errno.h>
#include <wchar.h>

/*
 * Attempt to parse a path, at the given starting point, and turn it into a vnode
 */
status_t FsPathToVnode(const vnode_t *start, const wchar_t *path, 
                       vnode_t *vn, bool do_create, const wchar_t **out)
{
    wchar_t *name, *ch, *slash, *newname;
    status_t ret;
    vnode_t node, mount_point, old_node;

    // Take a copy of the path; we're going to modify it
    ch = name = _wcsdup(path);

    if (do_create)
        *out = NULL;

    // Is this an absolute path (starting with /)?
    if (*ch == '/' || *ch == '\0')
    {
        // ...skip any superfluous / at the beginning
        while (*ch == '/')
            ch++;

        // ...and start at the starting point provided
        node = *start;
    }
    else
    {
        // ...else start in the current process's working directory
        node = current()->process->cur_dir;
    }

    assert(node.fsd != NULL);
    while (*ch != '\0')
    {
        old_node = node;

        /*
         * If this is the root of a mounted FD, switch over to the mounted FS 
         *  unless the file name is "..".
         * If the file name is "..", give it to the FSD which handles the 
         *  directory under the mount point.
         */
        if (FsLookupMountPoint(&mount_point, &node) &&
            wcscmp(ch, L"..") != 0)
            node = mount_point;

        // Separate the next path component
        slash = wcschr(ch, '/');
        if (slash == NULL)
        {
            /*
             * Reached the end of the path name
             */

            // If we're creating a file...
            if (do_create)
            {
                // ...then whatever's left in the path name is the name of 
                //  the file itself

                /* xxx - what do we do here if some FSD followed a link? */
                /*  path no longer points to the full name. */
                *out = path + (ch - name);
                break;
            }

            // ...else the component is already nul-terminated, because the 
            //  path name is nul-terminated
        }
        else
        {
            // Nul-terminate this path component
            *slash = '\0';
        }

        // Assume we're not getting a symbolic link
        newname = NULL;
        assert(node.fsd != NULL);

        if (node.fsd->vtbl->parse_element == NULL)
        {
            // A file system that doesn't know about paths?
            ret = ENOTIMPL;
        }
        else
        {
            // Ask the FSD to look up this path component
            ret = node.fsd->vtbl->parse_element(node.fsd, ch, &newname, &node);
        }

        // If the FSD gaves us an error (likely 'file not found')
        if (ret != 0)
        {
            // Clean up and return the error it gave us
            free(name);
            return ret;
        }

        // Have we been given a symbolic link?
        if (newname != NULL)
        {
            // ...yes, so start again with a new path name

            free(name);
            ch = name = newname;
            node = old_node;

            if (*ch == '/' || *ch == '\0')
            {
                while (*ch == '/')
                    ch++;

                node = fs_root;
            }
        }
        else if (slash == NULL)
        {
            // ...no, and it's the end of the path, so break out of the loop
            ch = L"";
        }
        else
        {
            // ...no, and we've got more to parse, so advance beyond this 
            //  path component
            ch = slash + 1;
        }
    }

    free(name);
    *vn = node;
    return 0;
}


/*!
 *    \brief    Creates a file
 *
 *    \param    path    Full path specification for the file
 *    \param    flags    Bitmask of access flags for the file
 *    \sa    \p FILE_READ, \p FILE_WRITE
 *    \return    Handle to the new file
 */
file_handle_t *FsCreateOpen(const vnode_t *start, const wchar_t *path, 
                            uint32_t flags, bool do_create)
{
    void *cookie;
    vnode_t node;
    status_t ret;
    const wchar_t *nodename;
    file_t *file;

    if (start == NULL)
        start = &fs_root;

    ret = FsPathToVnode(start, path, &node, do_create, &nodename);
    if (ret != 0)
    {
        wprintf(L"FsCreateOpen(%s): failed to parse path, ret = %d\n", path, ret);
        errno = ret;
        return NULL;
    }

    file = FsFindOpenFile(&node);
    if (file == NULL)
    {
        //wprintf(L"FsCreateOpen: creating new file object for %s at %p/%x\n", path, node.fsd, node.id);

        if (do_create && node.fsd->vtbl->create_file != NULL)
            ret = node.fsd->vtbl->create_file(node.fsd, node.id, nodename, &cookie);
        else if (!do_create && node.fsd->vtbl->lookup_file != NULL)
            ret = node.fsd->vtbl->lookup_file(node.fsd, node.id, flags, &cookie);
        else
            ret = ENOTIMPL;

        if (ret != 0)
        {
            wprintf(L"FsCreateOpen(%s): failed to look up file, ret = %d\n", path, ret);
            errno = ret;
            return NULL;
        }

        file = FsCreateFileObject(node.fsd, node.id, cookie);
        if (file == NULL)
        {
            wprintf(L"FsCreateOpen(%s): failed to create file object\n", path);
            return NULL;
        }
    }

    return FsCreateFileHandle(file, flags);
}


file_handle_t *FsCreate(const vnode_t *start, const wchar_t *path, uint32_t flags)
{
    return FsCreateOpen(start, path, flags, true);
}


file_handle_t *FsOpen(const vnode_t *start, const wchar_t *path, uint32_t flags)
{
    return FsCreateOpen(start, path, flags, false);
}


