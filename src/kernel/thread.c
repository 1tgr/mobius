/* $Id: thread.c,v 1.5 2002/01/15 00:13:06 pavlovskii Exp $ */

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

thread_queuent_t *ThrFindInQueue(thread_queue_t *queue, thread_t *thr)
{
	thread_queuent_t *ent;
	for (ent = queue->first; ent; ent = ent->next)
		if (ent->thr == thr)
			return ent;
	return NULL;
}

void ThrInsertQueue(thread_t *thr, thread_queue_t *queue, thread_t *before)
{
	thread_queuent_t *ent, *entb;

	SemAcquire(&queue->sem);

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
	thr->queued++;
	wprintf(L"ThrInsertQueue: thread %u added to queue %p\n",
		thr->id, queue);
	SemRelease(&queue->sem);
}

void ThrRemoveQueue(thread_t *thr, thread_queue_t *queue)
{
	thread_queuent_t *ent;

	SemAcquire(&queue->sem);
	ent = ThrFindInQueue(queue, thr);
	assert(ent != NULL);

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
	thr->queued--;
	/*ent->next = ent->prev = NULL;
	thr->queue = NULL;*/
	wprintf(L"ThrRemoveQueue: thread %u removed from queue %p\n",
		thr->id, queue);
	SemRelease(&queue->sem);
}

void ThrRunQueue(thread_queue_t *queue)
{
	thread_queuent_t *ent, *next;
	thread_t *thr;

	for (ent = queue->first; ent; ent = next)
	{
		next = ent->next;
		thr = ent->thr;
		wprintf(L"ThrRunQueue: running thread %u\n", thr->id);
		SemAcquire(&sc_sem);
		ThrRemoveQueue(thr, queue);
		SemRelease(&sc_sem);
		ThrRun(thr);
	}

	ScNeedSchedule(true);
}

void ScSchedule(void)
{
	thread_t *new;
	uint16_t *scr = PHYSICAL(0xb8000);
	thread_queuent_t *newent;

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
			wprintf(L"ScSchedule: running sleeping thread %u\n", new->id);
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
	
	/*
	 * This works as long as:
	 *	-- all thread stacks sit in the same 4MB page table
	 *	-- the first thread is created by the idle process
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
	/*if (thr->queue)
		ThrRemoveQueue(thr, thr->queue);*/
	if (ThrFindInQueue(thr_priority + thr->priority, thr))
	{
		wprintf(L"ThrDeleteThread: dequeuing running thread\n");
		ThrPause(thr);
	}
	
	wprintf(L"ThrDeleteThread: thread %u queued %u times\n",
		thr->id, thr->queued);
	assert(thr->queued == 0);

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

	SemAcquire(&sc_sem);
	ThrInsertQueue(thr, thr_priority + thr->priority, NULL);
	thr_queue_ready |= 1 << thr->priority;
	SemRelease(&sc_sem);
	return true;
}

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
		SemAcquire(&sc_sem);
		thr_queue_ready &= ~(1 << thr->priority);
		SemRelease(&sc_sem);
	}
}

void ThrSleep(thread_t *thr, unsigned ms)
{
	unsigned end;
	thread_queuent_t *sleep;

	end = sc_uptime + ms;
	for (sleep = thr_sleeping.first; sleep; sleep = sleep->next)
		if (sleep->thr->sleep_end > end)
			break;
	
	ThrPause(thr);
	thr->sleep_end = end;
	SemAcquire(&sc_sem);
	ThrInsertQueue(thr, &thr_sleeping, sleep->thr);
	SemRelease(&sc_sem);
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
	if (ptr->signals)
	{
		wprintf(L"ThrWaitHandle: already signalled...\n");
		ptr->signals--;
		return true;
	}

	ThrPause(thr);
	SemAcquire(&sc_sem);
	ThrInsertQueue(thr, &ptr->waiting, NULL);
	SemRelease(&sc_sem);
	ScNeedSchedule(true);
	return true;
}

void ThrQueueKernelApc(thread_t *thr, void (*fn)(void*), void *param)
{
	assert(thr->kernel_apc == NULL);
	SemAcquire(&sc_sem);
	thr->kernel_apc = fn;
	thr->kernel_apc_param = param;
	ThrInsertQueue(thr, &thr_apc, NULL);
	SemRelease(&sc_sem);
}