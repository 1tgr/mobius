/*
 * $History: handle.c $
 * 
 * *****************  Version 5  *****************
 * User: Tim          Date: 13/03/04   Time: 22:41
 * Updated in $/coreos/kernel
 * Removed troublesome per-class handle list
 * 
 * *****************  Version 4  *****************
 * User: Tim          Date: 8/03/04    Time: 20:31
 * Updated in $/coreos/kernel
 * Added HndDelete, the counterpart of HndInit
 * Tidied up
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 7/03/04    Time: 13:04
 * Updated in $/coreos/kernel
 * Tidied up
 * Replaced (wrong) call to free with call to HndCleanup
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:04
 * Updated in $/coreos/kernel
 * Removed dangerous KeYield from EvtSignal
 * Improved handle table reallocation
 * Added history block
 */

#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

#include <kernel/debug.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>


typedef struct hnd_export_t hnd_export_t;
struct hnd_export_t
{
    hnd_export_t *prev, *next;
    wchar_t *name;
    handle_hdr_t *handle;
};

static hnd_export_t *export_first, *export_last;
static spinlock_t sem_export;


void _HndInit(handle_hdr_t *hdr, handle_class_t *cls, const char *file, unsigned line)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->cls = cls;
    hdr->file = file;
    hdr->line = line;
}


void HndDelete(handle_hdr_t *hdr)
{
}


void HndAcquire(handle_hdr_t *hdr)
{
    SpinAcquire(&hdr->lock);
}


void HndRelease(handle_hdr_t *hdr)
{
    SpinRelease(&hdr->lock);
}


handle_hdr_t *HndCopy(handle_hdr_t *hdr)
{
    KeAtomicInc((int*) &hdr->copies);
    return hdr;
}


void HndCleanup(void *hdr)
{
    HndDelete(hdr);
    free(hdr);
}


void HndClose(handle_hdr_t *hdr)
{
    HndAcquire(hdr);

    if (hdr->copies == 0)
    {
        HndRelease(hdr);
        if ((hdr->cls->free) != NULL)
            (hdr->cls->free)(hdr);
    }
    else
    {
        hdr->copies--;
        HndRelease(hdr);
    }
}


handle_hdr_t *HndRef(struct process_t *proc, handle_t hnd, uint32_t tag)
{
    handle_hdr_t *ptr;

    if (proc == NULL)
        proc = current()->process;

    if (proc->handles == NULL)
    {
        wprintf(L"HndAcquireRef(%s, %ld): process handle table not created\n", 
            proc->exe, hnd);
        return NULL;
    }

    if (hnd >= proc->handle_count)
    {
        wprintf(L"HndAcquireRef(%s, %lx): invalid handle (> %lx)\n", 
            proc->exe, hnd, proc->handle_count);
        assert(hnd < proc->handle_count);
        return NULL;
    }

    ptr = HANDLE_ENTRY_TO_POINTER(proc->handles[hnd]);

    if (ptr == NULL)
    {
        wprintf(L"HndGetPtr(%s, %ld): freed handle accessed\n", 
            proc->exe, hnd);
        assert(ptr != NULL);
        return NULL;
    }

    if (tag != 0 &&
        ptr->cls->tag != tag)
    {
        wprintf(L"HndGetPtr(%s, %ld, %p:%d): tag mismatch (%c%c%c%c/%c%c%c%c)\n", 
            proc->exe, hnd, ptr->file, ptr->line,
            ((char*) &ptr->cls->tag)[0], ((char*) &ptr->cls->tag)[1], ((char*) &ptr->cls->tag)[2], ((char*) &ptr->cls->tag)[3],
            ((char*) &tag)[0], ((char*) &tag)[1], ((char*) &tag)[2], ((char*) &tag)[3]);
        assert(tag == 0 || ptr->cls->tag == tag);
        return NULL;
    }

    return ptr;
}

handle_t HndDuplicate(handle_hdr_t *ptr, process_t *proc)
{
    unsigned index;

    if (proc == NULL)
        proc = current()->process;

    assert(HANDLE_ENTRY_TO_POINTER(ptr) == ptr);

    index = KeAtomicInc(&proc->handle_count);
    if (proc->handle_count > proc->handle_allocated)
    {
        unsigned i;

        while (proc->handle_allocated < proc->handle_count)
            proc->handle_allocated += 16;

        proc->handles = realloc(proc->handles, proc->handle_allocated * sizeof(void*));
        if (proc->handles == NULL)
        {
            wprintf(L"handle_count = %u, handle_allocated = %u\n", 
                proc->handle_count, proc->handle_allocated);
            assert(proc->handles != NULL);
        }

        for (i = proc->handle_count; i < proc->handle_allocated; i++)
            proc->handles[i] = NULL;
    }

    proc->handles[index] = ptr;
    return index;
}


bool HndIsSignalled(handle_hdr_t *hdr)
{
    return hdr->cls->is_signalled != NULL &&
        hdr->cls->is_signalled(hdr);
}


void HndCompleteWait(handle_hdr_t *hdr, bool release_all)
{
    thread_t *thr;

    HndAcquire(hdr);
    if (HndIsSignalled(hdr) &&
        hdr->waiting.first != NULL)
    {
        if (release_all)
        {
            while (hdr->waiting.first != NULL)
            {
                if (hdr->cls->after_wait != NULL)
                    hdr->cls->after_wait(hdr);
                thr = hdr->waiting.first->thr;
                HndRelease(hdr);
                ThrRemoveQueue(thr, &hdr->waiting);
                ThrEvaluateWait(thr, hdr);
                HndAcquire(hdr);
            }

            assert(hdr->waiting.first == NULL);
        }
        else
        {
            if (hdr->waiting.first != NULL)
            {
                if (hdr->cls->after_wait != NULL)
                    hdr->cls->after_wait(hdr);
                thr = hdr->waiting.first->thr;
                HndRelease(hdr);
                ThrRemoveQueue(thr, &hdr->waiting);
                ThrEvaluateWait(thr, hdr);
                HndAcquire(hdr);
            }
        }
    }

    HndRelease(hdr);
}


bool HndSetInheritable(process_t *proc, handle_t hnd, bool inheritable)
{
    if (proc == NULL)
        proc = current()->process;

    if (hnd >= proc->handle_count ||
        proc->handles[hnd] == NULL)
        return false;

    if (inheritable)
        proc->handles[hnd] = (void*) ((addr_t) HANDLE_ENTRY_TO_POINTER(proc->handles[hnd]) | HND_FLAG_INHERITABLE);
    else
        proc->handles[hnd] = HANDLE_ENTRY_TO_POINTER(proc->handles[hnd]);

    return true;
}


void HndInheritHandles(process_t *src, process_t *dest)
{
    unsigned i, j;
    handle_hdr_t *ptr;

    if (dest->handle_allocated < src->handle_allocated)
    {
        dest->handles = realloc(dest->handles, src->handle_allocated * sizeof(void*));
        dest->handle_allocated = src->handle_allocated;
    }

    for (i = 0; i < src->handle_count; i++)
    {
        ptr = HANDLE_ENTRY_TO_POINTER(src->handles[i]);
        if (ptr != NULL &&
            HANDLE_ENTRY_TO_FLAGS(src->handles[i]) & HND_FLAG_INHERITABLE)
        {
            if (i >= dest->handle_count)
            {
                for (j = dest->handle_count; j <= i; j++)
                    dest->handles[j] = NULL;
                dest->handle_count = i + 1;
            }

            wprintf(L"HndInheritHandles: duplicated handle %u\n", i);
            assert(dest->handles[i] == NULL);
            dest->handles[i] = HndCopy(ptr);
        }
    }
}


bool HndExport(handle_hdr_t *hdr, const wchar_t *name)
{
    hnd_export_t *export;

    if (name == NULL)
    {
        hnd_export_t *next;
        bool found;

        found = false;
        SpinAcquire(&sem_export);
        export = export_first;
        while (export != NULL)
        {
            next = export->next;
            if (export->handle == hdr)
            {
                LIST_REMOVE(export, export);
                free(export->name);
                free(export);
                export = export_first;
                SpinRelease(&sem_export);

                HndClose(hdr);
                found = true;

                SpinAcquire(&sem_export);
            }

            export = next;
        }

        SpinRelease(&sem_export);
        return found;
    }
    else
    {
        export = malloc(sizeof(*export));
        if (export == NULL)
            return false;

        export->name = _wcsdup(name);
        export->handle = HndCopy(hdr);

        SpinAcquire(&sem_export);
        LIST_ADD(export, export);
        SpinRelease(&sem_export);

        return true;
    }
}

/*!
 *  \brief  Opens an exported handle
 *
 *  \param  name    Name of the handle to open
 *
 *  \return \p true if the area could be opened, \p false otherwise
 */
handle_hdr_t *HndOpen(const wchar_t *name)
{
    hnd_export_t *export;
    handle_hdr_t *hdr;

    SpinAcquire(&sem_export);

    for (export = export_first; export != NULL; export = export->next)
    {
        assert(export->name != NULL);
        if (_wcsicmp(export->name, name) == 0)
            break;
    }

    if (export == NULL)
    {
        errno = ENOTFOUND;
        SpinRelease(&sem_export);
        return 0;
    }

    hdr = HndCopy(export->handle);
    SpinRelease(&sem_export);

    return hdr;
}


static void EvtAfterWait(void *hdr)
{
    event_t *evt;
    evt = hdr;
    evt->is_signalled = false;
}


bool EvtIsSignalled(void *hdr)
{
    event_t *evt;
    evt = hdr;
    return evt->is_signalled;
}


event_t *EvtCreate(bool is_signalled)
{
    static handle_class_t event_class = 
        HANDLE_CLASS('evnt', HndCleanup, NULL, EvtAfterWait, EvtIsSignalled);
    event_t *evt;

    evt = malloc(sizeof(*evt));
    HndInit(&evt->hdr, &event_class);
    evt->is_signalled = is_signalled;
    return evt;
}


void EvtSignal(event_t *evt, bool is_signalled)
{
    evt->is_signalled = is_signalled;
    if (is_signalled)
        HndCompleteWait(&evt->hdr, true);
}


static void SemAfterWait(void *hdr)
{
    semaphore_t *sem;
    sem = hdr;
    KeAtomicDec(&sem->count);
}


static bool SemIsSignalled(void *hdr)
{
    semaphore_t *sem;
    sem = hdr;
    return sem->count > 0;
}


semaphore_t *SemCreate(int count)
{
    static handle_class_t semaphore_class = 
        HANDLE_CLASS('sema', HndCleanup, NULL, SemAfterWait, SemIsSignalled);
    semaphore_t *sem;

    sem = malloc(sizeof(*sem));
    if (sem == NULL)
        return NULL;

    HndInit(&sem->hdr, &semaphore_class);
    sem->count = count;
    return sem;
}


bool SemUp(semaphore_t *sem)
{
    KeAtomicInc(&sem->count);
    HndCompleteWait(&sem->hdr, false);
    return true;
}


bool SemDown(semaphore_t *sem)
{
    if (!ThrWaitHandle(current(), &sem->hdr))
        return false;

    KeYield();
    return true;
}
