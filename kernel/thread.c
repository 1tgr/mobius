/* $Id: thread.c,v 1.2 2003/06/05 21:56:51 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/arch.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <kernel/handle.h>
#include <kernel/vmm.h>
#include <kernel/memory.h>
#include <kernel/init.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <stdlib.h>
#include <stdio.h>
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
uint8_t *thr_kernel_stack_end = (void*) 0xE0000000;
spinlock_t thr_kernel_stack_sem;

int sc_switch_enabled;
unsigned sc_uptime, sc_need_schedule;
spinlock_t sc_sem;

handle_hdr_t *HndGetPtr(struct process_t *proc, handle_t hnd, uint32_t tag);

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
thread_queuent_t *ThrFindInQueue(thread_queue_t *queue, thread_t *thr)
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
void ThrInsertQueue(thread_t *thr, thread_queue_t *queue, thread_t *before)
{
    thread_queuent_t *ent, *entb;

    SpinAcquire(&queue->sem);

    /*assert(thr->queue == NULL);*/
    ent = malloc(sizeof(thread_queuent_t));
    ent->thr = thr;

    if (before)
    {
        entb = ThrFindInQueue(queue, before);
        assert(entb != NULL);
    }
    else
        entb = NULL;

    if (entb == NULL)
    {
        if (queue->last)
            /*queue->last->queue_next = thr;*/
            queue->last->next = ent;

        /*thr->queue_prev = queue->last;
        thr->queue_next = NULL;
        queue->last = thr;*/
        ent->prev = queue->last;
        ent->next = NULL;
        queue->last = ent;
    }
    else
    {
        /*thr->queue_next = before;
        thr->queue_prev = before->queue_prev;
        if (before->queue_prev)
            before->queue_prev->queue_next = thr;
        before->queue_prev = thr;*/

        ent->next = entb;
        ent->prev = entb->prev;
        if (entb->prev)
            entb->prev->next = ent;
        entb->prev = ent;
    }

    if (queue->first == NULL)
        queue->first = ent;

    /*thr->queue = queue;*/
    KeAtomicInc(&thr->queued);
    TRACE2("ThrInsertQueue: thread %u added to queue %p\n",
        thr->id, queue);
    SpinRelease(&queue->sem);
}

/*!
 *  \brief  Removes a thread from a thread queue
 */
void ThrRemoveQueue(thread_t *thr, thread_queue_t *queue)
{
    thread_queuent_t *ent;

    SpinAcquire(&queue->sem);
    ent = ThrFindInQueue(queue, thr);
    if (ent == NULL)
    {
        SpinRelease(&queue->sem);
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
    if (queue->current == ent)
        queue->current = NULL;

    free(ent);
    KeAtomicDec(&thr->queued);
    /*ent->next = ent->prev = NULL;
    thr->queue = NULL;*/
    TRACE2("ThrRemoveQueue: thread %u removed from queue %p\n",
        thr->id, queue);
    SpinRelease(&queue->sem);
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
void ThrRunQueue(thread_queue_t *queue)
{
    thread_queuent_t *ent, *next;
    thread_t *thr;

    for (ent = queue->first; ent; ent = next)
    {
        next = ent->next;
        thr = ent->thr;
        TRACE1("ThrRunQueue: running thread %u\n", thr->id);
        SpinAcquire(&sc_sem);
        ThrRemoveQueue(thr, queue);
        SpinRelease(&sc_sem);
        ThrRun(thr);
    }

    ScNeedSchedule(true);
}

/*!
 *  \brief  Chooses a new thread to run
 */
void ScSchedule(void)
{
    thread_t *new;
    uint16_t *scr = PHYSICAL(0xb8000);
    thread_queuent_t *newent;
    cpu_t *c;

    c = cpu();
    if (sc_switch_enabled <= 0)
        return;

    sc_need_schedule = 0;
    scr[0] = ~scr[0];

    /* Wake any threads that have finished sleeping */
    while (thr_sleeping.first != NULL)
    {
        new = thr_sleeping.first->thr;
        if (sc_uptime >= new->sleep_end)
        {
            ThrRemoveQueue(new, &thr_sleeping);
            new->sleep_end = 0;
            ThrRun(new);
        }
        else
            break;
    }

    /* Run any finishio routines */
    ThrRunQueue(&thr_apc);
    
    if (thr_queue_ready)
    {
        unsigned i;
        uint32_t mask;

        mask = 1;
        for (i = 0; i < 32; i++, mask <<= 1)
            /*if (thr_queue_ready & mask)*/
            if ((newent = thr_priority[i].current) ||
                (newent = thr_priority[i].first))
            {
                /*if (thr_priority[i].current == NULL)
                    new = thr_priority[i].first;
                else
                    new = thr_priority[i].current;

                assert(new != NULL);*/
                thr_priority[i].current = newent->next;
                if (thr_priority[i].current == NULL)
                    thr_priority[i].current = thr_priority[i].first;

                new = newent->thr;
                KeAtomicInc(&new->span);
                if (new->span > 5)
                    new->span = 0;
                else
                {
                    if (c->current_thread != new)
                        c->current_thread->span = 0;
                    
                    c->current_thread = new;
                    scr[79] = 0x7100 | ('0' + c->current_thread->id);
                    return;
                }
            }
    }

    c->current_thread = &c->thr_idle;
    scr[79 - 1 - ArchThisCpu()] = 0x7100 | c->current_thread->id;
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
    /*SpinAcquire(&sc_sem);*/
    if (need)
        KeAtomicInc(&sc_need_schedule);
    else if (sc_need_schedule > 0)
        KeAtomicDec(&sc_need_schedule);
    /*SpinRelease(&sc_sem);*/
}

/*!
 *  \brief  Allocates a \p thread_info_t structure for the specified thread
 */
bool ThrAllocateThreadInfo(thread_t *thr)
{
    assert(thr->process == current()->process);

    if (thr->is_kernel)
        thr->info = malloc(sizeof(thread_info_t));
    else
        thr->info = VmmAlloc(PAGE_ALIGN_UP(sizeof(thread_info_t)) / PAGE_SIZE,
            NULL,
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE | VM_MEM_ZERO);

    //wprintf(L"ThrAllocateThreadInfo(%s/%u): %p\n", thr->process->exe, thr->id, thr->info);
    if (thr->info == NULL)
        return false;

    thr->info->info = thr->info;
    thr->info->id = thr->id;
    thr->info->process = thr->process->info;
    thr->info->param = thr->param;
    thr->info->exception_handler = NULL;
    return true;
}

/*!
 *  \brief  Creates and runs a new thread in the specified process
 */
thread_t *ThrCreateThread(process_t *proc, bool isKernel, void (*entry)(void), 
                          bool useParam, void *param, unsigned priority)
{
    thread_t *thr;
    addr_t stack;

    if (proc == NULL)
        proc = &proc_idle;

    thr = malloc(sizeof(thread_t));
    if (thr == NULL)
        return NULL;

    memset(thr, 0, sizeof(*thr));
    HndInit(&thr->hdr, 'thrd');

    /*thr->kernel_stack = sbrk(PAGE_SIZE);
    if (thr->kernel_stack == NULL)
    {
        ThrDeleteThread(thr);
        return NULL;
    }*/

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
    MemMap((addr_t) thr->kernel_stack, 
        thr->kernel_stack_phys, 
        (addr_t) thr->kernel_stack + PAGE_SIZE,
        PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);

    thr->kernel_stack_phys = MemAlloc();
    MemMap((addr_t) thr->kernel_stack + PAGE_SIZE, 
        thr->kernel_stack_phys, 
        (addr_t) thr->kernel_stack + PAGE_SIZE * 2,
        PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);

    /* Unmap the thread's kernel stack lower guard page */
    MemMap((addr_t) thr->kernel_stack - PAGE_SIZE,
        NULL,
        (addr_t) thr->kernel_stack,
        0);
#endif

    thr_kernel_stack_end -= PAGE_SIZE * 3;
    SpinRelease(&thr_kernel_stack_sem);

    if (isKernel)
    {
        stack = (addr_t) thr->kernel_stack;
        //wprintf(L"ThrCreateThread(kernel): stack at %x\n", stack);
    }
    else
    {
        proc->stack_end -= 0x100000;

        if (proc == current()->process)
        {
            stack = (addr_t) VmmAlloc(0x100000 / PAGE_SIZE, proc->stack_end, 
                VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
            //wprintf(L"ThrCreateThread(user): user stack at %x\n", stack);
            stack += 0x100000;
        }
        else
            stack = proc->stack_end + 0x100000;

        /*stack = proc->stack_end;
        proc->stack_end -= PAGE_SIZE;*/
    }

    ArchSetupContext(thr, (addr_t) entry, isKernel, useParam, (addr_t) param, 
        stack);
    
    thr->process = proc;
    thr->priority = priority;
    KeAtomicInc(&thr_last_id);
    thr->id = thr_last_id;
    thr->param = param;
    thr->is_kernel = isKernel;

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

    wprintf(L"Created thread %s/%u\n", proc->exe, thr->id);
    HndDuplicate(proc, &thr->hdr);
    ThrRun(thr);
    return thr;
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
    /*if (thr->queue)
        ThrRemoveQueue(thr, thr->queue);*/
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
    MemFree(thr->kernel_stack_phys);
    MemMap((addr_t) thr->kernel_stack - PAGE_SIZE, 
        0, 
        (addr_t) thr->kernel_stack,
        0);

    HndSignalPtr(&thr->hdr, true);
    KeAtomicDec((unsigned*) &thr->hdr.copies);
    if (thr->hdr.copies == 0)
    {
        TRACE1("ThrDeleteThread: freeing thread %u\n", thr->id);
        HndRemovePtrEntries(thr->process, &thr->hdr);
        free(thr);
    }
    else
        TRACE2("ThrDeleteThread: thread %u still has %u refs\n",
            thr->id, thr->hdr.copies);

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
 *  \brief  Places the specified thread onto the run queue for the 
 *      appropriate priority
 */
bool ThrRun(thread_t *thr)
{
    if (thr->priority > _countof(thr_priority))
        return false;

    SpinAcquire(&sc_sem);
    ThrInsertQueue(thr, thr_priority + thr->priority, NULL);
    thr_queue_ready |= 1 << thr->priority;
    SpinRelease(&sc_sem);
    //ScNeedSchedule(true);
    return true;
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

    SpinAcquire(&sc_sem);
    end = sc_uptime + ms;
    for (sleep = thr_sleeping.first; sleep; sleep = sleep->next)
        if (sleep->thr->sleep_end > end)
            break;
    SpinRelease(&sc_sem);

    ThrPause(thr);

    SpinAcquire(&sc_sem);
    thr->sleep_end = end;

    if (sleep)
        ThrInsertQueue(thr, &thr_sleeping, sleep->thr);
    else
        ThrInsertQueue(thr, &thr_sleeping, NULL);

    SpinRelease(&sc_sem);
    /* wprintf(L"ThrSleep: thread %d sleeping until %u\n", thr->id, thr->sleep_end); */
    ScNeedSchedule(true);
}

/*!
 *  \brief  Causes a thread to suspend execution until the specified handle 
 *      object is signalled
 */
bool ThrWaitHandle(thread_t *thr, handle_t handle, uint32_t tag)
{
    handle_hdr_t *ptr;

    ptr = HndGetPtr(thr->process, handle, tag);
    if (ptr == NULL)
        return false;

    TRACE4("ThrWaitHandle: waiting on %s:%S:%d(%lx)\n", 
        current()->process->exe, ptr->file, ptr->line, handle);
    if (ptr->signals)
    {
        /*wprintf(L"ThrWaitHandle: already signalled...\n");*/
        KeAtomicDec((unsigned*) &ptr->signals);
        return true;
    }

    ThrPause(thr);
    SpinAcquire(&sc_sem);
    ThrInsertQueue(thr, &ptr->waiting, NULL);
    SpinRelease(&sc_sem);
    ScNeedSchedule(true);
    return true;
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
    SpinAcquire(&sc_sem);
    apc = malloc(sizeof(thread_apc_t));
    apc->fn = fn;
    apc->param = param;
    LIST_ADD(thr->apc, apc);
    ThrInsertQueue(thr, &thr_apc, NULL);
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
        HndInit(&c->thr_idle.hdr, 'thrd');
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

    return true;
}
