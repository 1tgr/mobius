/* $Id: thread.h,v 1.13 2002/08/04 17:22:39 pavlovskii Exp $ */
#ifndef __KERNEL_THREAD_H
#define __KERNEL_THREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/kernel.h>

struct context_t;
struct process_t;

/*!
 *    \ingroup    kernel
 *    \defgroup    thr    Threads
 *    @{
 */

typedef struct thread_t thread_t;

#include <kernel/handle.h>
#include <kernel/arch.h>
#include <os/queue.h>

typedef struct thread_apc_t thread_apc_t;
struct thread_apc_t
{
    thread_apc_t *prev, *next;
    void (*fn)(void*);
    void *param;
};

struct thread_t
{
    struct context_t *ctx_last;

    handle_hdr_t hdr;

    thread_t *all_prev, *all_next;
    void *kernel_stack;
    addr_t kernel_stack_phys;
    struct thread_info_t *info;
    addr_t kernel_esp;
    struct process_t *process;
    bool is_kernel;
    unsigned priority;
    /*thread_queue_t *queue;
    thread_t *queue_prev, *queue_next;*/
    unsigned queued;
    unsigned span;
    unsigned id;
    unsigned sleep_end;
    thread_apc_t *apc_first, *apc_last;
    unsigned cputime;
    addr_t user_stack_top;
    void *param;
    queue_t msgqueue;

    bool v86_if;
    addr_t v86_handler;
    bool v86_in_handler;
    context_v86_t v86_context;
};

typedef struct cpu_t cpu_t;
struct cpu_t
{
    cpu_t *this_cpu;
    thread_t *current_thread;
    thread_t thr_idle;
    thread_info_t thr_idle_info;
    void *arch;
};

extern thread_t *thr_first, *thr_last;//, *current;

#ifdef __SMP__
extern cpu_t thr_cpu[];

extern unsigned ArchThisCpu(void);

static inline cpu_t *cpu(void)
{
    return thr_cpu + ArchThisCpu();
}

#define ThrGetCpu(n)    (&thr_cpu[n])

#else
extern cpu_t thr_cpu_single;
#define cpu()           (&thr_cpu_single)
#define ThrGetCpu(n)    (&thr_cpu_single)
#endif

#define current()       (cpu()->current_thread)

thread_t * 
    ThrGetCurrent(void);
thread_t * 
    ThrCreateThread(struct process_t *proc, bool isKernel, 
        void (*entry)(void), bool useParam, void *param, 
        unsigned priority);
void    ThrDeleteThread(thread_t *thr);
struct context_t * 
    ThrGetContext(thread_t* thr);
struct context_t * 
    ThrGetUserContext(thread_t *thr);
bool    ThrRun(thread_t *thr);
void    ThrPause(thread_t *thr);
void    ThrSleep(thread_t *thr, unsigned ms);
bool    ThrWaitHandle(thread_t *thr, handle_t handle, uint32_t tag);
void    ThrInsertQueue(thread_t *thr, thread_queue_t *queue, thread_t *before);
void    ThrRemoveQueue(thread_t *thr, thread_queue_t *queue);
void    ThrRunQueue(thread_queue_t *queue);
bool    ThrAllocateThreadInfo(thread_t *thr);
void    ThrQueueKernelApc(thread_t *thr, void (*fn)(void*), void *param);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif