/*
 * $History: thread.c $
 * 
 * *****************  Version 5  *****************
 * User: Tim          Date: 9/03/04    Time: 21:46
 * Updated in $/coreos/kernel
 * Added slab_thread_queuent and slab_thread_apc
 * Fixed formatting
 * 
 * *****************  Version 4  *****************
 * User: Tim          Date: 8/03/04    Time: 20:33
 * Updated in $/coreos/kernel
 * Fixed recursive spinlock on ThrQueueKernelApc
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 7/03/04    Time: 13:07
 * Updated in $/coreos/kernel
 * Replaced spinlocks on individual thread queues with the scheduler
 * spinlock
 * Made some functions non-public
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:03
 * Updated in $/coreos/kernel
 * Allocate multiple sync I/O events for nested FsRead/FsWrite
 * Tidied up
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/arch.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <kernel/handle.h>
#include <kernel/vmm.h>
#include <kernel/memory.h>
#include <kernel/init.h>
#include <kernel/slab.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <os/defs.h>

extern process_t proc_idle;

uint32_t thr_queue_ready;

/*! Running threads: one queue per priority level */
thread_queue_t thr_priority[32];

/*! Sleeping threads */
thread_queue_t thr_sleeping;

/*! Threads that have APCs queued */
thread_queue_t thr_apc;

#ifdef __SMP__
cpu_t thr_cpu[MAX_CPU];
#else
cpu_t thr_cpu_single;
#endif

thread_t *thr_first, *thr_last;
unsigned thr_last_id;
uint8_t *thr_kernel_stack_end = (void*) ADDR_HEAP_VIRT;
spinlock_t thr_kernel_stack_sem;

int sc_switch_enabled;
unsigned sc_uptime, sc_need_schedule;
spinlock_t sc_sem;

static slab_t slab_thread_queuent;
static slab_t slab_thread_apc;

static void ThrCleanup(void *hdr);
static bool ThrIsSignalled(void *hdr);

static handle_class_t thread_class = 
    HANDLE_CLASS('thrd', ThrCleanup, NULL, NULL, ThrIsSignalled);


/*!
 *  \brief  Returns a pointer to the thread that is currently running on this 
 *      CPU
 */
#ifdef __SMP__
thread_t *ThrGetCurrent(void)
{
    return thr_cpu[ArchThisCpu()].current_thread;
}
#else
thread_t *ThrGetCurrent(void)
{
    return thr_cpu_single.current_thread;
}
#endif


/*!
 *  \brief  Finds the \p thread_queuent_t structure that corresponds to
 *      the specified thread in a given queue
 */
static thread_queuent_t *ThrFindInQueue(thread_queue_t *queue, thread_t *thr)
{
    thread_queuent_t *ent;
    for (ent = queue->first; ent; ent = ent->next)
        if (ent->thr == thr)
            return ent;
    return NULL;
}


/*!
 *  \brief  Inserts a thread into a thread queue
 */
static void ThrInsertQueue(thread_t *thr, thread_queue_t *queue, thread_queuent_t *before)
{
    thread_queuent_t *ent;

    SpinAcquire(&sc_sem);
    ent = SlabAlloc(&slab_thread_queuent);
    assert(ent != NULL);
    ent->thr = thr;

    if (before == NULL)
    {
        if (queue->last != NULL)
            queue->last->next = ent;

        ent->prev = queue->last;
        ent->next = NULL;
        queue->last = ent;

        if (queue->first == NULL)
            queue->first = ent;
    }
    else
    {
        ent->next = before;
        ent->prev = before->prev;
        if (before->prev != NULL)
            before->prev->next = ent;
        before->prev = ent;
        if (queue->first == before)
            queue->first = ent;
    }

    KeAtomicInc(&thr->queued);
    TRACE2("ThrInsertQueue: thread %u added to queue %p\n",
        thr->id, queue);

    SpinRelease(&sc_sem);
}


/*!
 *  \brief  Removes a thread from a thread queue
 */
void ThrRemoveQueue(thread_t *thr, thread_queue_t *queue)
{
    thread_queuent_t *ent;

    SpinAcquire(&sc_sem);
    ent = ThrFindInQueue(queue, thr);
    if (ent == NULL)
    {
        SpinRelease(&sc_sem);
        return;
    }

    if (ent->prev)
        ent->prev->next = ent->next;
    if (ent->next)
        ent->next->prev = ent->prev;
    if (queue->first == ent)
        queue->first = ent->next;
    if (queue->last == ent)
        queue->last = ent->prev;

    SlabFree(&slab_thread_queuent, ent);
    KeAtomicDec(&thr->queued);
    TRACE2("ThrRemoveQueue: thread %u removed from queue %p\n",
        thr->id, queue);

    SpinRelease(&sc_sem);
}

/*!
 *  \brief  Places the specified thread onto the run queue for the 
 *      appropriate priority
 */
static bool ThrRun(thread_t *thr)
{
    assert(thr->priority < _countof(thr_priority));

    ThrInsertQueue(thr, thr_priority + thr->priority, NULL);
    SpinAcquire(&sc_sem);
    thr_queue_ready |= 1 << thr->priority;
    SpinRelease(&sc_sem);
    return true;
}


/*!
 *    \brief        Runs a queue of threads
 *
 *    The threads on the queue are removed from the queue and added to their 
 *    respective priority queues, to be scheduled next time the scheduler is 
 *    called.
 *
 *    \param        queue         Queue to run
 */
static void ThrRunQueue(thread_queue_t *queue)
{
    thread_queuent_t *ent, *next;
    thread_t *thr;

    SpinAcquire(&sc_sem);
    while (queue->first != NULL)
    {
        ent = queue->first;
        next = ent->next;
        thr = ent->thr;
        TRACE1("ThrRunQueue: running thread %u\n", thr->id);
        SpinRelease(&sc_sem);
        ThrRemoveQueue(thr, queue);
        ThrRun(thr);
        SpinAcquire(&sc_sem);
    }

    ScNeedSchedule(true);
    SpinRelease(&sc_sem);
}

void ThrEvaluateWait(thread_t *thr, handle_hdr_t *reason)
{
    unsigned i, handles_set;
    int last_unset;

    handles_set = 0;
    last_unset = -1;
    for (i = 0; i < thr->num_wait_handles; i++)
    {
        if (thr->wait_handles[i] == reason)
        {
            HndClose(reason);
            thr->wait_handles[i] = NULL;
        }

        if (thr->wait_handles[i] == NULL)
            last_unset = i;
        else
            handles_set++;
    }

    //if (thr->num_wait_handles > 1)
        //wprintf(L"ThrEvaluateWait: handles_set = %u, num_wait_handles = %u: ",
            //handles_set, thr->num_wait_handles);

    if ((thr->wait_all  && handles_set == 0) ||
        (!thr->wait_all && handles_set < thr->num_wait_handles))
    {
        //if (thr->num_wait_handles > 1)
            //wprintf(L"running, last_unset = %d\n", last_unset);

        if (thr->wait_result != NULL)
            *thr->wait_result = last_unset;

        thr->wait_result = NULL;
        thr->num_wait_handles = 0;
        ThrRun(thr);
        ScNeedSchedule(true);
    }
    //else if (thr->num_wait_handles > 1)
        //wprintf(L"not running\n");
}

/*!
 *  \brief  Chooses a new thread to run
 */
void ScSchedule(void)
{
    thread_t *new;
    thread_queuent_t *newent;
    cpu_t *c;

    if (sc_switch_enabled <= 0)
        return;

    SpinAcquire(&sc_sem);

    c = cpu();
    sc_need_schedule = 0;

    /* Wake any threads that have finished sleeping */
    while (thr_sleeping.first != NULL)
    {
        new = thr_sleeping.first->thr;
        if (sc_uptime >= new->sleep_end)
        {
            SpinRelease(&sc_sem);
            ThrRemoveQueue(new, &thr_sleeping);
            ThrRun(new);
            SpinAcquire(&sc_sem);
            new->sleep_end = 0;
        }
        else
            break;
    }

    SpinRelease(&sc_sem);

    /* Run any finishio routines */
    ThrRunQueue(&thr_apc);

    SpinAcquire(&sc_sem);
    
    if (thr_queue_ready)
    {
        unsigned i;
        uint32_t mask;

        mask = 1;
        for (i = 0; i < 32; i++, mask <<= 1)
        {
            newent = thr_priority[i].first;
            if (newent != NULL)
            {
                if (newent->next != NULL)
                    newent->next->prev = newent->prev;
                if (newent->prev != NULL)
                    newent->prev->next = newent->next;
                if (thr_priority[i].first == newent)
                    thr_priority[i].first = newent->next;
                if (thr_priority[i].last == newent)
                    thr_priority[i].last = newent->prev;
                newent->next = newent->prev = NULL;

                if (thr_priority[i].last != NULL)
                    thr_priority[i].last->next = newent;
                newent->prev = thr_priority[i].last;
                newent->next = NULL;
                thr_priority[i].last = newent;
                if (thr_priority[i].first == NULL)
                    thr_priority[i].first = newent;

                new = newent->thr;
                KeAtomicInc(&new->span);
                if (new->span > 5)
                    new->span = 0;
                else
                {
                    if (c->current_thread != new)
                        c->current_thread->span = 0;
                    
                    c->current_thread = new;
                    SpinRelease(&sc_sem);
                    return;
                }
            }
        }
    }

    c->current_thread = &c->thr_idle;

    SpinRelease(&sc_sem);
}

/*!
 *    \brief        Enables or disables the scheduler
 *
 *    The scheduler will only be called if its enable count is greater than zero;
 *    this function either increments or decrements the scheduler enable count.
 *
 *    \param        enable          \p true to enable the scheduler; \p false to disable it
 */
void ScEnableSwitch(bool enable)
{
    SpinAcquire(&sc_sem);
    if (enable)
        KeAtomicInc(&sc_switch_enabled);
    else
        KeAtomicDec(&sc_switch_enabled);
    SpinRelease(&sc_sem);
}

void ScNeedSchedule(bool need)
{
    if (need)
        KeAtomicInc(&sc_need_schedule);
    else if (sc_need_schedule > 0)
        KeAtomicDec(&sc_need_schedule);
}

/*!
 *  \brief  Allocates a \p thread_info_t structure for the specified thread
 */
bool ThrAllocateThreadInfo(thread_t *thr)
{
    unsigned i;
    context_t *ctx;

    assert(thr->process == current()->process);

    if (thr->is_kernel)
        thr->info = malloc(sizeof(thread_info_t));
    else
        thr->info = VmmAlloc(PAGE_ALIGN_UP(sizeof(thread_info_t)) / PAGE_SIZE,
            NULL,
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE | VM_MEM_ZERO);

    if (thr->info == NULL)
        return false;

    ctx = ThrGetContext(thr);
    if (thr->is_kernel)
        ctx->eip = (uint32_t) thr->entry;
    else
        ctx->eip = (uint32_t) thr->process->info->thread_entry;

    thr->info->info = thr->info;
    thr->info->id = thr->id;
    thr->info->process = thr->process->info;
    thr->info->entry = (int (*)(void*)) thr->entry;
    thr->info->param = thr->param;
    thr->info->exception_handler = NULL;

    for (i = 0; i < _countof(thr->info->sync_io_events); i++)
    {
        event_t *sync_io_event;

        sync_io_event = EvtCreate(false);
        if (sync_io_event == NULL)
        {
            free(thr->info);
            thr->info = NULL;
            return false;
        }

        thr->info->sync_io_events[i] = HndDuplicate(&sync_io_event->hdr, thr->process);
    }

    thr->info->next_sync_io_event = 0;
    return true;
}


static void ThrCleanup(void *hdr)
{
    free(ENCLOSING_STRUCT(hdr, thread_t, hdr));
}


static bool ThrIsSignalled(void *hdr)
{
    thread_t *thr;
    thr = ENCLOSING_STRUCT(hdr, thread_t, hdr);
    return thr->has_exited;
}


/*!
 *  \brief  Creates and runs a new thread in the specified process
 */
thread_t *ThrCreateThread(process_t *proc, bool isKernel, void (*entry)(void), 
                          bool useParam, void *param, unsigned priority, 
                          const wchar_t *name)
{
    thread_t *thr;
    handle_hdr_t *hnd_copy;
    event_t *vmm_io_event;
    addr_t stack;

    if (proc == NULL)
        proc = &proc_idle;

    thr = malloc(sizeof(thread_t));
    if (thr == NULL)
        return NULL;

    memset(thr, 0, sizeof(*thr));

    HndInit(&thr->hdr, &thread_class);

    SpinAcquire(&thr_kernel_stack_sem);
    
    /*
     * This works as long as:
     *          -- all thread stacks sit in the same 4MB page table
     *          -- the first thread is created by the idle process
     */

#if 0
    /* One stack page, two guard pages */
    /* thr->kernel_stack points to the lower end of the stack */
    thr->kernel_stack = thr_kernel_stack_end - PAGE_SIZE * 2;
    
    /* Unmap the thread's kernel stack upper guard page */
    MemMap((addr_t) thr->kernel_stack + PAGE_SIZE,
        NULL,
        (addr_t) thr->kernel_stack + PAGE_SIZE * 2,
        0);
    /* Map the thread's kernel stack */
    thr->kernel_stack_phys = MemAlloc();
    MemMap((addr_t) thr->kernel_stack, 
        thr->kernel_stack_phys, 
        (addr_t) thr->kernel_stack + PAGE_SIZE,
        PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);
    /* Unmap the thread's kernel stack lower guard page */
    MemMap((addr_t) thr->kernel_stack - PAGE_SIZE,
        NULL,
        (addr_t) thr->kernel_stack,
        0);
#else
    /* Two stack pages, one guard page */
    /* thr->kernel_stack points to the lower end of the stack */
    thr->kernel_stack = thr_kernel_stack_end - PAGE_SIZE * 2;
    
    /* Map the thread's kernel stack */
    thr->kernel_stack_phys = MemAlloc();
    MemMapRange(thr->kernel_stack, 
        thr->kernel_stack_phys, 
        (uint8_t*) thr->kernel_stack + PAGE_SIZE,
        PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);

    thr->kernel_stack_phys = MemAlloc();
    MemMapRange(thr->kernel_stack + PAGE_SIZE, 
        thr->kernel_stack_phys, 
        (uint8_t*) thr->kernel_stack + PAGE_SIZE * 2,
        PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);

    /* Unmap the thread's kernel stack lower guard page */
    MemMapRange((uint8_t*) thr->kernel_stack - PAGE_SIZE,
        NULL,
        thr->kernel_stack,
        0);
#endif

    thr_kernel_stack_end -= PAGE_SIZE * 3;
    SpinRelease(&thr_kernel_stack_sem);

    if (isKernel)
    {
        stack = (addr_t) thr->kernel_stack;
    }
    else
    {
        proc->stack_end -= 0x100000;

        if (proc == current()->process)
        {
            stack = (addr_t) VmmAlloc(0x100000 / PAGE_SIZE, proc->stack_end, 
                VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
            stack += 0x100000;
        }
        else
            stack = proc->stack_end + 0x100000;
    }

    ArchSetupContext(thr, (addr_t) entry, isKernel, useParam, (addr_t) param, stack);

    thr->process = proc;
    thr->priority = priority;
    KeAtomicInc(&thr_last_id);
    thr->id = thr_last_id;
    thr->entry = entry;
    thr->param = param;
    thr->is_kernel = isKernel;
    thr->name = _wcsdup(name);

    vmm_io_event = EvtCreate(false);
    thr->vmm_io_event = HndDuplicate(&vmm_io_event->hdr, proc);

    TRACE3("thread %u: kernel stack at %p = %x\n",
        thr->id, thr->kernel_stack, thr->kernel_stack_phys);

    if (proc != current()->process)
        thr->info = NULL;
    else
        ThrAllocateThreadInfo(thr);

    SpinAcquire(&sc_sem);

    if (thr_last != NULL)
        thr_last->all_next = thr;
    thr->all_prev = thr_last;
    thr->all_next = NULL;
    thr_last = thr;
    if (thr_first == NULL)
        thr_first = thr;

    SpinRelease(&sc_sem);

    wprintf(L"Created thread %s/%u (%s)\n", proc->exe, thr->id, thr->name);
    ThrRun(thr);

    hnd_copy = HndCopy(&thr->hdr);
    return ENCLOSING_STRUCT(hnd_copy, thread_t, hdr);
}


void ThrExitThread(int code)
{
    wprintf(L"Thread %u exited with code %d\n", current()->id, code);
    ThrDeleteThread(current());
    ScNeedSchedule(true);
}


bool ThrRaiseException(thread_t *thr)
{
    context_t *ctx, *ctx_current;

    if (thr->info == NULL ||
        thr->info->exception_handler == NULL)
        return false;

    thr->exceptions = realloc(thr->exceptions, 
        sizeof(context_t) * (thr->num_exceptions + 1));

    ctx = thr->exceptions + thr->num_exceptions++;
    ctx_current = ThrGetUserContext(thr);
    *ctx = *ctx_current;

    thr->info->exception_info = *ctx_current;
    ctx_current->eip = (uint32_t) thr->info->exception_handler;
    wprintf(L"ThrRaiseException: revectoring to %x\n", ctx_current->eip);
    return true;
}


bool ThrContinue(thread_t *thr, const context_t *ctx)
{
    if (thr->num_exceptions == 0)
        return false;

    thr->num_exceptions--;
    if (thr->num_exceptions == 0)
    {
        free(thr->exceptions);
        thr->exceptions = NULL;
    }

    wprintf(L"ThrContinue: continuing at %x\n", ctx->eip);
    *ThrGetUserContext(thr) = *ctx;
    return true;
}


/*!
 *  \brief  Suspends and frees the specified thread
 */
void ThrDeleteThread(thread_t *thr)
{
    asyncio_t *aio, *next;

    for (aio = thr->aio_first; aio != NULL; aio = next)
    {
        next = aio->thread_next;
        wprintf(L"ThrDeleteThread(%u): request %p (aio = %p) still queued\n",
            thr->id, aio->req, aio);
        DevCancelIo(aio);
    }

    ThrRemoveQueue(thr, &thr_apc);
    ThrRemoveQueue(thr, &thr_sleeping);
    if (ThrFindInQueue(thr_priority + thr->priority, thr))
    {
        TRACE0("ThrDeleteThread: dequeuing running thread\n");
        ThrPause(thr);
    }

    /* xxx -- should be OK to ignore this since we have cancelled any pending
     *  I/O on the thread */
    /*TRACE2("ThrDeleteThread: thread %u queued %u times\n",
        thr->id, thr->queued);
    assert(thr->queued == 0);*/

    if (thr_priority[thr->priority].first == NULL)
        thr_queue_ready &= ~(1 << thr->priority);

    if (thr->all_prev)
        thr->all_prev->all_next = thr->all_next;
    if (thr->all_next)
        thr->all_next->all_prev = thr->all_prev;
    if (thr == thr_last)
        thr_last = thr->all_prev;
    if (thr == thr_first)
        thr_first = thr->all_next;

    /*free(thr->kernel_stack);*/

    /* xxx -- this can't hope to work! */
    /*MemFree(thr->kernel_stack_phys);
    MemMapRange((uint8_t*) thr->kernel_stack - PAGE_SIZE, 
        0, 
        thr->kernel_stack,
        0);*/

    thr->has_exited = true;
    HndCompleteWait(&thr->hdr, true);
    HndClose(&thr->hdr);

    if (thr == current())
        ScNeedSchedule(true);
}

/*!    \brief Returns the thread context structure for the specified thread.
 *
 *    While in kernel mode, each thread has its own context structure which
 *          describes the user-mode state of the processor at the time it entered
 *          kernel mode. Thus this structure uniquely identifies the state of a 
 *          thread -- context switching is possible by switching the processor's 
 *          state between the different threads' context structures.
 *
 *    The thread's context is held in the thread's kernel stack and was placed
 *          there by the initial interrupt handle code, immediately before passing
 *          control to the isr() function.
 *
 *    \param        thr    The thread for which the context is requested
 *    \return         The thread context structure for the specified thread.
 */
context_t* ThrGetContext(thread_t* thr)
{
    return (context_t*) (thr->kernel_esp - 4);
}

/*!
 *  \brief  Returns the user-mode context structure for the specified thread
 */
context_t *ThrGetUserContext(thread_t *thr)
{
    context_t *ctx;
    for (ctx = thr->ctx_last; 
        ctx != NULL && (ctx->cs & 3) != 3; 
        ctx = ctx->ctx_prev)
        ;
    return ctx;
}


/*!
 *  \brief  Removes the specified thread from its priority's run queue
 */
void ThrPause(thread_t *thr)
{
    thread_queue_t *queue;

    queue = thr_priority + thr->priority;
    /*if (thr->queue != NULL)*/
        ThrRemoveQueue(thr, queue);

    if (/*thr->queue == thr_priority + thr->priority &&
        thr_priority[thr->priority].first == NULL*/
        queue->first == NULL)
    {
        SpinAcquire(&sc_sem);
        thr_queue_ready &= ~(1 << thr->priority);
        SpinRelease(&sc_sem);
    }
}

/*!
 *  \brief  Causes the a thread to suspend execution for the given length of 
 *      time
 */
void ThrSleep(thread_t *thr, unsigned ms)
{
    unsigned end;
    thread_queuent_t *sleep;

    ThrPause(thr);

    SpinAcquire(&sc_sem);

    end = sc_uptime + ms;
    for (sleep = thr_sleeping.first; sleep != NULL; sleep = sleep->next)
        if (sleep->thr->sleep_end > end)
            break;

    thr->sleep_end = end;
    SpinRelease(&sc_sem);

    ThrInsertQueue(thr, &thr_sleeping, sleep);
    ScNeedSchedule(true);
}

/*!
 *  \brief  Causes a thread to suspend execution until the specified handle 
 *      object is signalled
 */
bool ThrWaitHandle(thread_t *thr, handle_hdr_t *handle)
{
    return ThrWaitMultiple(thr, &handle, 1, true, NULL);
}

bool ThrWaitMultiple(thread_t *thr, handle_hdr_t * const*handles, unsigned num_handles, bool wait_all, int *result)
{
    handle_hdr_t *hdr;
    unsigned i, handles_set, last_set;

    if (num_handles > _countof(thr->wait_handles))
    {
        errno = EMFILE;
        return false;
    }

    handles_set = 0;
    for (i = 0; i < num_handles; i++)
    {
        thr->wait_handles[i] = hdr = HndCopy(handles[i]);

        if (hdr->cls->before_wait != NULL)
            hdr->cls->before_wait(hdr);

        if (HndIsSignalled(hdr))
        {
            if (hdr->cls->after_wait != NULL)
                hdr->cls->after_wait(hdr);
            last_set = i;
        }
        else
        {
            //if (num_handles > 1)
                //wprintf(L"ThrWaitMultiple(%u): waiting on %s:%S:%d(%lx)\n", 
                    //i, current()->process->exe, hdr->file, hdr->line, handles[i]);
            handles_set++;
        }
    }

    if ((wait_all  && handles_set == 0) ||
        (!wait_all && handles_set < num_handles))
    {
        for (i = 0; i < num_handles; i++)
            HndClose(handles[i]);

        //if (num_handles > 1)
            //wprintf(L"ThrWaitMultiple: immediate finish %u\n", last_set);

        if (result != NULL)
            *result = last_set;
        return true;
    }
    else
    {
        thr->num_wait_handles = num_handles;
        thr->wait_all = wait_all;
        thr->wait_result = result;
        for (i = 0; i < num_handles; i++)
            ThrInsertQueue(thr, &thr->wait_handles[i]->waiting, NULL);

        ThrPause(thr);
        ScNeedSchedule(true);

        if (result != NULL)
            *result = -42;
        return true;
    }
}

/*!
 *    \brief        Queues an asynchronous procedure call for the specified thread
 *
 *    This function adds the function provided to the thread's APC queue;
 *    the thread's APC queue will be run next time the scheduler runs.
 *
 *    APCs run in the same context as the thread they belong to.
 *
 *    \param        thr    Thread in which to queue the APC
 *    \param        fn    APC function to call
 *    \param        param         Parameter for the APC function
 */
void ThrQueueKernelApc(thread_t *thr, void (*fn)(void*), void *param)
{
    thread_apc_t *apc;

    apc = SlabAlloc(&slab_thread_apc);
    apc->fn = fn;
    apc->param = param;

    SpinAcquire(&sc_sem);
    LIST_ADD(thr->apc, apc);
    SpinRelease(&sc_sem);

    ThrInsertQueue(thr, &thr_apc, NULL);
}


void ThrRunApcs(thread_t *thr)
{
    thread_apc_t *apc;

    SpinAcquire(&sc_sem);
    while (thr->apc_first != NULL)
    {
        apc = thr->apc_first;
        LIST_REMOVE(thr->apc, apc);
        SpinRelease(&sc_sem);

        apc->fn(apc->param);
        SlabFree(&slab_thread_apc, apc);
        ThrPause(thr);

        SpinAcquire(&sc_sem);
    }

    SpinRelease(&sc_sem);
}


/*!
 *  \brief  Initialises the scheduler
 */
bool ThrInit(void)
{
    cpu_t *c;

#ifdef __SMP__
    unsigned i;

    for (c = thr_cpu, i = 0; i < kernel_startup.num_cpus; c++, i++)
#else
    c = &thr_cpu_single;
#endif
    {
        HndInit(&c->thr_idle.hdr, &thread_class);
        c->current_thread = &c->thr_idle;
        c->thr_idle.info = &c->thr_idle_info;
        c->thr_idle.process = &proc_idle;

#ifdef __SMP__
        if (i == 0)
#endif
            thr_first = &c->thr_idle;

#ifdef __SMP__
        if (i == kernel_startup.num_cpus - 1)
#endif
            thr_last = &c->thr_idle;

#ifdef __SMP__
        if (i > 0)
            c->thr_idle.all_prev = &thr_cpu[i - 1].thr_idle;
        if (i < kernel_startup.num_cpus - 1)
            c->thr_idle.all_next = &thr_cpu[i + 1].thr_idle;
#endif
    }

    SlabInit(&slab_thread_queuent, sizeof(thread_queuent_t), NULL, NULL);
    SlabInit(&slab_thread_apc, sizeof(thread_apc_t), NULL, NULL);
    return true;
}
