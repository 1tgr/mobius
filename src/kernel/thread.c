/* $Id: thread.c,v 1.4 2002/01/12 02:16:08 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/arch.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <kernel/handle.h>
#include <kernel/vmm.h>
#include <kernel/memory.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <os/defs.h>

extern process_t proc_idle;

thread_info_t idle_thread_info;
uint32_t thr_queue_ready;

/* running threads: one queue per priority level */
thread_queue_t thr_priority[32];

/* sleeping threads */
thread_queue_t thr_sleeping;

/* threads that have APCs queues */
thread_queue_t thr_apc;

thread_t thr_idle =
{
	NULL,						/* ctx_last */
	{
		0,						/* hdr.locks */
		'thrd',					/* hdr.tag */
		NULL,					/* hdr.locked_by */
		0,						/* hdr.signal */
		{
			NULL,				/* hdr.waiting.first */
			NULL,				/* hdr.waiting.last */
			NULL,				/* hdr.current */
		},
		__FILE__,				/* hdr.file */
		__LINE__,				/* hdr.line */
		0,
	},
	NULL,						/* prev */
	NULL,						/* next */
	NULL,						/* kernel_stack */
	0,							/* kernel_stack_phys */
	&idle_thread_info,			/* info */
	0xdeadbeef,					/* kernel_esp */
	&proc_idle
};

thread_t *current = &thr_idle, 
	*thr_first = &thr_idle, 
	*thr_last = &thr_idle;
unsigned thr_last_id;
uint8_t *thr_kernel_stack_end = (void*) 0xE0000000;
semaphore_t thr_kernel_stack_sem;

int sc_switch_enabled;
unsigned sc_uptime, sc_need_schedule;
semaphore_t sc_sem;

handle_hdr_t *HndGetPtr(struct process_t *proc, handle_t hnd, uint32_t tag);

void ThrInsertQueue(thread_t *thr, thread_queue_t *queue, thread_t *before)
{
	SemAcquire(&queue->sem);

	if (before == NULL)
	{
		if (queue->last)
			queue->last->queue_next = thr;
		thr->queue_prev = queue->last;
		thr->queue_next = NULL;
		queue->last = thr;
	}
	else
	{
		thr->queue_next = before;
		thr->queue_prev = before->queue_prev;
		if (before->queue_prev)
			before->queue_prev->queue_next = thr;
		before->queue_prev = thr;
	}

	if (queue->first == NULL)
		queue->first = thr;

	thr->queue = queue;
	SemRelease(&queue->sem);
}

void ThrRemoveQueue(thread_t *thr, thread_queue_t *queue)
{
	SemAcquire(&queue->sem);
	if (thr->queue_prev)
		thr->queue_prev->queue_next = thr->queue_next;
	if (thr->queue_next)
		thr->queue_next->queue_prev = thr->queue_prev;
	if (thr == queue->first)
		queue->first = thr->queue_next;
	if (thr == queue->last)
		queue->last = thr->queue_prev;

	thr->queue_next = thr->queue_prev = NULL;
	thr->queue = NULL;
	SemRelease(&queue->sem);
}

void ScSchedule(void)
{
	thread_t *new, *next;
	uint16_t *scr = PHYSICAL(0xb8000);

	if (sc_switch_enabled <= 0)
		return;

	sc_need_schedule = 0;
	scr[0] = ~scr[0];

	/* Wake any threads that have finished sleeping */
	while (thr_sleeping.first != NULL)
	{
		new = thr_sleeping.first;
		if (sc_uptime >= new->sleep_end)
		{
			wprintf(L"ScSchedule: running sleeping thread %u\n", new->id);
			ThrRemoveQueue(new, &thr_sleeping);
			new->sleep_end = 0;
			ThrRun(new);
		}
		else
			break;
	}

	/* Run any finishio routines */
	for (new = thr_apc.first; new; new = next)
	{
		next = new->queue_next;
		wprintf(L"ScSchedule: running APC thread %u\n", new->id);
		ThrRemoveQueue(new, &thr_apc);
		ThrRun(new);
	}

	if (thr_queue_ready)
	{
		unsigned i;
		uint32_t mask;

		mask = 1;
		for (i = 0; i < 32; i++, mask <<= 1)
			/*if (thr_queue_ready & mask)*/
			if ((new = thr_priority[i].current) ||
				(new = thr_priority[i].first))
			{
				/*if (thr_priority[i].current == NULL)
					new = thr_priority[i].first;
				else
					new = thr_priority[i].current;

				assert(new != NULL);*/
				thr_priority[i].current = new->queue_next;
				if (thr_priority[i].current == NULL)
					thr_priority[i].current = thr_priority[i].first;

				new->span++;
				if (new->span > 10)
					new->span = 0;
				else
				{
					if (current != new)
						current->span = 0;
					
					current = new;
					scr[79] = 0x7100 | current->id;
					return;
				}
			}
	}

	current = &thr_idle;
	scr[79] = 0x7100 | current->id;
}

void ScEnableSwitch(bool enable)
{
	SemAcquire(&sc_sem);
	if (enable)
		sc_switch_enabled++;
	else if (sc_switch_enabled > 0)
		sc_switch_enabled--;
	SemRelease(&sc_sem);
}

void ScNeedSchedule(bool need)
{
	/*SemAcquire(&sc_sem);*/
	if (need)
		sc_need_schedule++;
	else if (sc_need_schedule > 0)
		sc_need_schedule--;
	/*SemRelease(&sc_sem);*/
}

bool ThrAllocateThreadInfo(thread_t *thr)
{
	assert(thr->process == current->process);
	
	thr->info = VmmAlloc(PAGE_ALIGN_UP(sizeof(thread_info_t)) / PAGE_SIZE,
		NULL,
		3 | MEM_READ | MEM_WRITE | MEM_ZERO | MEM_COMMIT);
	wprintf(L"ThrAllocateThreadInfo: %p\n", thr->info);
	if (thr->info == NULL)
		return false;

	thr->info->info = thr->info;
	thr->info->id = thr->id;
	thr->info->process = thr->process->info;
	return true;
}

thread_t *ThrCreateThread(process_t *proc, bool isKernel, void (*entry)(void*), 
						  bool useParam, void *param, unsigned priority)
{
	thread_t *thr;
	addr_t stack;

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

	SemAcquire(&thr_kernel_stack_sem);
	/* thr->kernel_stack points to the lower end of the stack */
	thr->kernel_stack = thr_kernel_stack_end - PAGE_SIZE * 2;
	thr_kernel_stack_end -= PAGE_SIZE * 3;

	/*
	 * This works as long as:
	 *	-- all thread stacks sit in the same 4MB page table
	 *	-- the first thread is created by the idle process
	 */

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
	SemRelease(&thr_kernel_stack_sem);

	if (isKernel)
		stack = (addr_t) thr->kernel_stack;
	else
	{
		stack = proc->stack_end;
		proc->stack_end -= PAGE_SIZE;
	}

	ArchSetupContext(thr, (addr_t) entry, isKernel, useParam, (addr_t) param, 
		stack);
	
	thr->process = proc;
	thr->priority = priority;
	thr->id = ++thr_last_id;

	wprintf(L"thread %u: kernel stack at %p = %x\n",
		thr->id, thr->kernel_stack, thr->kernel_stack_phys);

	if (proc != current->process)
		thr->info = NULL;
	else
		ThrAllocateThreadInfo(thr);

	SemAcquire(&sc_sem);

	if (thr_last != NULL)
		thr_last->all_next = thr;
	thr->all_prev = thr_last;
	thr->all_next = NULL;
	thr_last = thr;
	if (thr_first == NULL)
		thr_first = thr;

	SemRelease(&sc_sem);

	/*wprintf(L"Created thread %s/%u\n", proc->exe, thr->id);*/
	HndDuplicate(proc, &thr->hdr);
	ThrRun(thr);
	return NULL;
}

void ThrDeleteThread(thread_t *thr)
{
	if (thr->queue)
		ThrRemoveQueue(thr, thr->queue);

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

	HndRemovePtrEntries(thr->process, &thr->hdr);
	free(thr);

	if (thr == current)
		ScNeedSchedule(true);
}

/*!	\brief Returns the thread context structure for the specified thread.
 *
 *	While in kernel mode, each thread has its own context structure which
 *		describes the user-mode state of the processor at the time it entered
 *		kernel mode. Thus this structure uniquely identifies the state of a 
 *		thread -- context switching is possible by switching the processor's 
 *		state between the different threads' context structures.
 *
 *	The thread's context is held in the thread's kernel stack and was placed
 *		there by the initial interrupt handle code, immediately before passing
 *		control to the isr() function.
 *
 *	\param	thr	The thread for which the context is requested
 *	\return	The thread context structure for the specified thread.
 */
context_t* ThrGetContext(thread_t* thr)
{
	return (context_t*) (thr->kernel_esp - 4);
}

bool ThrRun(thread_t *thr)
{
	if (thr->priority > _countof(thr_priority))
		return false;

	ThrInsertQueue(thr, thr_priority + thr->priority, NULL);
	
	SemAcquire(&sc_sem);
	thr_queue_ready |= 1 << thr->priority;
	SemRelease(&sc_sem);
	return true;
}

void ThrPause(thread_t *thr)
{
	if (thr->queue == thr_priority + thr->priority &&
		thr_priority[thr->priority].first == NULL)
	{
		SemAcquire(&sc_sem);
		thr_queue_ready &= ~(1 << thr->priority);
		SemRelease(&sc_sem);
	}

	if (thr->queue != NULL)
		ThrRemoveQueue(thr, thr->queue);
}

void ThrSleep(thread_t *thr, unsigned ms)
{
	unsigned end;
	thread_t *sleep;

	end = sc_uptime + ms;
	for (sleep = thr_sleeping.first; sleep; sleep = sleep->queue_next)
		if (sleep->sleep_end > end)
			break;
	
	ThrPause(thr);
	thr->sleep_end = end;
	
	ThrInsertQueue(thr, &thr_sleeping, sleep);
	/* wprintf(L"ThrSleep: thread %d sleeping until %u\n", thr->id, thr->sleep_end); */
	ScNeedSchedule(true);
}

bool ThrWaitHandle(thread_t *thr, handle_t handle, uint32_t tag)
{
	handle_hdr_t *ptr;

	ptr = HndGetPtr(thr->process, handle, tag);
	if (ptr == NULL)
		return false;

	wprintf(L"ThrWaitHandle: waiting on %s:%S:%d(%lx)\n", 
		current->process->exe, ptr->file, ptr->line, handle);
	ThrPause(thr);
	ThrInsertQueue(thr, &ptr->waiting, NULL);
	ScNeedSchedule(true);
	return true;
}

void ThrQueueKernelApc(thread_t *thr, void (*fn)(void*), void *param)
{
	assert(thr->kernel_apc == NULL);
	thr->kernel_apc = fn;
	thr->kernel_apc_param = param;
	ThrInsertQueue(thr, &thr_apc, NULL);
}