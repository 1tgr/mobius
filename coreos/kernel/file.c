/*
 * $History: file.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:31
 * Updated in $/coreos/kernel
 * Moved open/closed file lookup code into FsFindOpenFile
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 7/03/04    Time: 19:03
 * Created in $/coreos/kernel
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/slab.h>
#include <kernel/init.h>
#include <kernel/cache.h>
#include <kernel/thread.h>

// Hash table of files currently open
static spinlock_t sem_files;
static file_t *fs_files[128];

// List of files to be closed
static semaphore_t *mux_closing;
static file_t *fs_closing_first, *fs_closing_last;
static slab_t slab_file;

extern struct process_t proc_idle;

/*
 * Generate a hash code for a FSD/node ID pair
 */
static uint32_t FsHashVnode(const vnode_t *node)
{
    return (uint32_t) ((addr_t) node->fsd ^ node->id) % _countof(fs_files);
}


/*
 * Creates a file_t object
 */
file_t *FsCreateFileObject(fsd_t *fsd, vnode_id_t id, void *fsd_cookie)
{
    file_t *file;
    fs_info_t info;
    uint32_t hash;

    file = SlabAlloc(&slab_file);
    if (file == NULL)
        return NULL;

    file->vnode.fsd = fsd;
    file->vnode.id = id;
    file->fsd_cookie = fsd_cookie;

    // Obtain this file system's cache block size. Default to zero, which
    //  indicates that the FSD doesn't want its files cached.
    info.flags = FS_INFO_CACHE_BLOCK_SIZE;
    info.cache_block_size = 0;
    if (fsd->vtbl->get_fs_info != NULL)
        fsd->vtbl->get_fs_info(fsd, &info);

    // Create the cache object if we need to
    if (info.cache_block_size != 0)
        file->cache = CcCreateFileCache(info.cache_block_size);
    else
        file->cache = NULL;

    // This file is now open, so insert it into the open files table
    hash = FsHashVnode(&file->vnode);
    SpinAcquire(&sem_files);
    file->u.open.hash_next = fs_files[hash];
    fs_files[hash] = file;
    SpinRelease(&sem_files);

    return file;
}


/*
 * Close a file_t object
 */
static void FsCloseFile(file_t *file)
{
    fsd_t *fsd;

    fsd = file->vnode.fsd;
    if (file->cache != NULL)
    {
        if (fsd->vtbl->flush_cache != NULL)
            fsd->vtbl->flush_cache(fsd, file);

        CcDeleteFileCache(file->cache);
        file->cache = NULL;
    }

    if (fsd->vtbl->free_cookie != NULL)
        fsd->vtbl->free_cookie(fsd, file->fsd_cookie);

    SlabFree(&slab_file, file);
}


/*
 * Reference a file_t object
 */
file_t *FsReferenceFile(file_t *file)
{
    KeAtomicInc(&file->u.open.refs);
    return file;
}


/*
 * Release a file_t object
 */
void FsReleaseFile(file_t *file)
{
    file_t *prev, *cur;
    uint32_t hash;

    if (file->u.open.refs == 0)
    {
        /*
         * Remove the file from the table of open files
         */
        hash = FsHashVnode(&file->vnode);
        SpinAcquire(&sem_files);
        cur = fs_files[hash];
        prev = NULL;
        while (cur != NULL)
        {
            if (cur == file)
            {
                if (prev == NULL)
                    fs_files[hash] = file->u.open.hash_next;
                else
                    prev->u.open.hash_next = file->u.open.hash_next;

                break;
            }

            cur = cur->u.open.hash_next;
            prev = cur;
        }

        SpinRelease(&sem_files);

        // If the file is not cached...
        if (file->cache == NULL)
            // ...we can close it right now
            FsCloseFile(file);
        else
        {
            // ...else we add it to the closing list to be closed later, or 
            //  possibly re-opened soon

            //wprintf(L"FsReleaseFile: adding to closing list %p/%u\n", file->vnode.fsd, file->vnode.id);

            SemDown(mux_closing);
            if (fs_closing_last != NULL)
                fs_closing_last->u.closing.next = file;
            file->u.closing.prev = fs_closing_last;
            file->u.closing.next = NULL;
            fs_closing_last = file;
            if (fs_closing_first == NULL)
                fs_closing_first = file;
            SemUp(mux_closing);
        }
    }
    else
        KeAtomicDec(&file->u.open.refs);
}


/*
 * Entry point of the FsFileCloser thread, which periodically flushes to disk
 *  any files that have been put on the closing list
 */
static void FsFileCloserThread(void)
{
    file_t *file;

    while (true)
    {
        SemDown(mux_closing);

        // For each file on the closing list...
        while (fs_closing_first != NULL)
        {
            // ...remove it from the closing list
            file = fs_closing_first;
            assert(file->u.closing.prev == NULL);
            if (file->u.closing.next != NULL)
                file->u.closing.next->u.closing.prev = NULL;
            fs_closing_first = file->u.closing.next;
            if (fs_closing_last == file)
                fs_closing_last = NULL;
            file->u.closing.next = NULL;
            SemUp(mux_closing);

            //wprintf(L"FsFileCloserThread: closing %p/%x\n", file->vnode.fsd, file->vnode.id);

            // ...and close it
            FsCloseFile(file);

            SemDown(mux_closing);
        }

        SemUp(mux_closing);
        ThrSleep(current(), 10000);
        KeYield();
    }
}


file_t *FsFindOpenFile(const vnode_t *node)
{
    uint32_t hash;
    file_t *prev, *file;

    /*
     * Check whether this file has been opened before.
     * xxx -- files without a cache don't get remembered (no point, and it breaks ports)
     */

    hash = FsHashVnode(node);

    SemDown(mux_closing);
    for (file = fs_closing_last; file != NULL; file = prev)
    {
        prev = file->u.closing.prev;

        if (file->vnode.fsd == node->fsd &&
            file->vnode.id == node->id &&
            file->cache != NULL)
        {
            if (file->u.closing.next != NULL)
                file->u.closing.next->u.closing.prev = file->u.closing.prev;
            if (file->u.closing.prev != NULL)
                file->u.closing.prev->u.closing.next = file->u.closing.next;
            if (fs_closing_first == file)
                fs_closing_first = file->u.closing.next;
            if (fs_closing_last == file)
                fs_closing_last = file->u.closing.prev;
            file->u.closing.next = file->u.closing.prev = NULL;

            file->u.open.refs = 0;
            file->u.open.hash_next = fs_files[hash];
            fs_files[hash] = file;
            break;
        }
    }
    SemUp(mux_closing);

    if (file == NULL)
    {
        SpinAcquire(&sem_files);

        for (file = fs_files[hash]; file != NULL; file = file->u.open.hash_next)
        {
            if (file->vnode.fsd == node->fsd &&
                file->vnode.id == node->id &&
                file->cache != NULL)
            {
                file = FsReferenceFile(file);
                break;
            }
        }

        SpinRelease(&sem_files);
    }

    return file;
}


bool FsInitFile(void)
{
    SlabInit(&slab_file, sizeof(file_t), NULL, NULL);
    mux_closing = SemCreate(1);
    ThrCreateThread(&proc_idle, true, FsFileCloserThread, false, NULL, 0, L"FsFileCloserThread");
    return true;
}
