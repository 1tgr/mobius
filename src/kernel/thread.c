#include <errno.h>
#include <malloc.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/proc.h>
#include <kernel/handle.h>
#include <kernel/vmm.h>
#include <kernel/sys.h>
#include <os/os.h>

thread_queue_t thr_running[32];
thread_queue_t thr_suspended, thr_sleeping;
dword thr_canrun;
thread_t *thr_first, *thr_last, *current;
dword thr_lastid;
bool thr_needschedule;

//SEMAPHORE(sem_scheduler);

extern tss_t tss;
extern thread_t thr_idle;//, *old_current;
extern descriptor_t _gdt[];

void semAcquire(semaphore_t* sem)
{
	if (sem->owner == NULL ||
		sem->owner == current)
	{
		sem->owner = current;
		sem->locks++;
	}
	else
	{
		while (sem->locks)
			enable();

		sem->owner = current;
		sem->locks++;
	}
}

void semRelease(semaphore_t* sem)
{
	if (sem->owner != current)
	{
		wprintf(L"%S(%d) ", sem->file, sem->line);
		if (sem->owner)
			wprintf(L"owned by %u\n", sem->owner->id);
		else
			wprintf(L"not owned\n");

		assert(sem->owner == current);
	}

	sem->locks--;
	if (sem->locks == 0)
		sem->owner = NULL;
}

bool semTryAcquire(semaphore_t* sem)
{
	if ((sem->owner == NULL ||
		sem->owner == current) &&
		sem->locks == 0)
	{
		semAcquire(sem);
		return true;
	}
	else
		return false;
}

//! Creates a new thread.
/*!
 *	The new thread is created suspended, in the following state:
 *	- For kernel threads (level == 0), CS is set to 0x18 (the flat kernel code 
 *		selector) with RPL == 0
 *	- For user threads (level == 3), CS is set to 0x28 (the flat user code 
 *		selector) with RPL == 3
 *	- DS, ES, GS and SS are set to 0x30 (the flat data selector) with RPL == 
 *		level
 *	- FS is set to 0x40 (the per-thread data selector) with RPL == level
 *	- EIP is set to entry_point
 *	- All general-purpose registers are zeroed
 *	- A 4KB kernel stack is created, and the initial context is set up as if 
 *		the thread had just entered a timer IRQ
 *	- Space for a user stack is allocated out of the process's stack space. 
 *		This is initially 4KB, but it can grow as needed.
 *	- All flags are zero except IF, the interrupt enable flag
 *
 *	Each thread is associated with a particular process. To create a thread as 
 *		part of the kernel, use proc = &proc_idle.
 *
 *	The thread is created suspended (to unsuspend, use thread->suspend--). A
 *		per-thread information structure is allocated in the process's address 
 *		space and its members are filled out.
 *
 *	The new thread is not scheduled immediately.
 *
 *	\param	level	The level of thread to create. This should be less than or
 *		equal to the process's level (i.e. equally or more priviliged).
 *	\param	proc	The process within which the thread will execute.
 *	\param	entry_point	The function where thread execution will start.
 *
 *	\return	The new thread object, or NULL if thread creation failed.
 */

char *sbrk(size_t size);

thread_t* thrCreate(int level, process_t* proc, const void* entry_point, 
					unsigned priority)
{
	thread_t* thr;
	context_t* ctx;
	thread_info_t* info;
	vm_area_t* area;

	if (priority >= 32)
		return false;
	if (proc == NULL)
		return false;
	if (level > 3)
		return false;

	/* Allocate a handle for the thread structure */
	thr = hndAlloc(sizeof(thread_t), proc);
	if (!thr)
		return NULL;

	//semAcquire(&sem_scheduler);

	memset(thr, 0, sizeof(thread_t));

	/* Allocate a PL0 stack */
	thr->kernel_stack = (void*) sbrk(PAGE_SIZE);
	assert(thr->kernel_stack != NULL);

	thr->kernel_esp = (dword) thr->kernel_stack + PAGE_SIZE - sizeof(context_t);
	//wprintf(L"kernel_esp = %x\n", thr->kernel_esp);

	/* Create the thread's context and initialise its selectors and registers */
	ctx = thrContext(thr);

	if (level == 0)
		ctx->cs = 0x18 | level;
	else
		ctx->cs = 0x28 | level;

	ctx->ds = ctx->es = ctx->gs = ctx->ss = 0x30 | level;
	ctx->fs = 0x40 | level;

	memset(&ctx->regs, 0, sizeof(ctx->regs));

	/* Set up the task frame for a context switch -- simulate a timer IRQ return */
	ctx->intr = 0;
	ctx->error = -1;
	/* Enable interrupts */
	ctx->eflags = 2 | EFLAG_IF | 0x3000;	// xxx - IOPL==3
	ctx->eip = (dword) entry_point;
	ctx->esp = proc->stack_end;
	ctx->kernel_esp = thr->kernel_esp;

	/* Deduct the thread's stack from the process's memory */
	/* xxx - need vmmAlloc here */
	proc->stack_end -= PAGE_SIZE;

	thr->next = NULL;
	thr->prev = thr_last;
	thr->prev_queue = NULL;
	thr->next_queue = NULL;
	thr->id = ++thr_lastid;
	thr->suspend = 1;
	thr->state = THREAD_RUNNING;
	thr->process = proc;
	thr->priority = priority;

	/* Allocate process memory for the user-mode thread structure */
	thr->info = vmmAlloc(proc, 1, NULL, MEM_USER | MEM_COMMIT | MEM_READ | MEM_WRITE);
	assert(thr->info != NULL);
	//wprintf(L"%s(%d): TCB at %x\n", proc->mod_first->name, thr->id, thr->info);

	area = vmmArea(proc, thr->info);
	if (area == NULL)
	{
		wprintf(L"thr->info = %p\n", thr->info);
		assert(area != NULL);
	}

	if (area)
	{
		dword phys;

		phys = memTranslate(proc->page_dir, area->start) & -PAGE_SIZE;
		/* Initialize the fields */
		info = (thread_info_t*) phys;

		//info->except = (exception_registration_t*) 0xffffffff; // no handler
		info->info = (thread_info_t*) thr->info;
		info->tid = thr->id;
		info->pid = 0;
		info->last_error = 0;
		info->process = proc->info;
	}

	if (thr_last)
		thr_last->next = thr;

	if (!thr_first)
		thr_first = thr;

	thr_last = thr;
	thrEnqueue(thr, &thr_suspended);
	//semRelease(&sem_scheduler);
	return thr;
}

//! Creates a thread which will operate in Virtual 8086 mode.
/*!
 *	The IA32 processor architecture allows a task to run in an real-mode 
 *		context under a protected-mode environment. The kernel allows this by
 *		creating a new thread which operates under this mode.
 *
 *	The new thread is created in user mode; however, certain priviliged 
 *		instructions (such as port reads/writes) are emulated by the kernel.
 *		Interrupt calls are handled transparently; the thread uses the real-
 *		mode interrupt vector table at 0000:0000. The exception to this is the 
 *		kernel syscall interrupt, 0x30: this functions the same way as in 
 *		protected mode. Hence a V86 thread can terminate itself with the 
 *		following sequence:
 *
 *	mov	ax, 106h
 *	int	30h
 *
 *	\note	To access system calls with IDs > 0xFFFF, a V86 thread must use
 *		the operand-size override and load eax with the system call ID, not
 *		ax. Initially all registers, including the high words of general-
 *		purpose registers, are zeroed.
 *
 *	The function maps the lower 1MB of memory into the process's address space.
 *	This is necessary for two reasons:
 *	- The V86 thread can only execute out of addresses below 1MB
 *	- The V86 thread can access ordinary real-mode code and data, such as the
 *		BIOS. Note that a real-mode operating system, such as MS-DOS, will
 *		not have been loaded before The Möbius, and will not be present
 *		in memory.
 *
 *	The code passed to the thrCreate86() function is copied into low memory,
 *		at address 4000:0000.
 *
 *	The new thread will execute independently to the calling thread. If 
 *		synchronous operation is needed (whereby the calling thread treats the
 *		V86 thread as a subroutine), the thrWaitHandle() function can be used.
 *
 *	\param	proc	The process within which the thread will execute.
 *	\param	code	An real-mode function to execute as the thread.
 *	\param	code_size	The length, in bytes, of the function.
 *
 *	\return	The new thread object, or NULL if thread creation failed.
 */
thread_t* thrCreate86(process_t* proc, const byte* code, size_t code_size,
					  V86HANDLER handler, unsigned priority)
{
	thread_t* thr;
	context_t* ctx86;
	byte* page;
	vm_area_t* area;

	area = vmmArea(proc, NULL);
	if (area)
	{
		wprintf(L"Real-mode memory block already allocated: size = %x\n", 
			area->pages * PAGE_SIZE);
		//if (area->phys != NULL)
			//return NULL;

		//vmmFree(proc, area);
		area->flags = MEM_READ | MEM_WRITE | MEM_USER | MEM_LITERAL;
	}

	thr = thrCreate(3, proc, NULL, priority);
	assert(thr != NULL);

	ctx86 = thrContext(thr);
	if (area == NULL)
	{
		page = vmmAlloc(proc, 1048576 / PAGE_SIZE, 0, 
			MEM_READ | MEM_WRITE | MEM_USER | MEM_LITERAL);
		assert(page == NULL);	// gotcha! page == start of block == zero
		area = vmmArea(proc, NULL);
	}
	
	vmmCommit(area, NULL, -1);

	//page = vmmAlloc(proc, 1, 0x40000, MEM_READ | MEM_WRITE | MEM_USER | MEM_COMMIT);
	page = (byte*) 0x40000;
	assert(page != NULL);

	memcpy(page, code, code_size);
	ctx86->eip = 0;
	ctx86->cs = ctx86->ss = ((dword) page) >> 4;
	ctx86->ds = ctx86->es = ctx86->fs = ctx86->gs = ctx86->ss = 0x30 | 3;
	ctx86->esp = 0;
	ctx86->eflags = 2 | EFLAG_IF | EFLAG_IOPL3 | EFLAG_VM | EFLAG_TF;
	thr->v86handler = handler;
	
	thrSuspend(thr, false);
	return thr;
}

//! Terminates and deletes a thread.
/*!
 *	The thread handle is signalled and all the data associated with it is 
 *		freed. If it is the current thread, a reschedule takes place.
 *
 *	\param	thr	The thread to be deleted
 */
void thrDelete(thread_t* thr)
{
	if (thr == &thr_idle)
	{
		wprintf(L"Not ending idle thread\n");
		halt(0);
		return;
	}

	//semAcquire(&sem_scheduler);

	while (thr->suspend)
		thrSuspend(thr, false);

	thrDequeue(thr, &thr_running[thr->priority]);

	//wprintf(L"Deleting thread %u\n", thr->id);
	//thr->suspend++;
	if (thr->prev)
		thr->prev->next = thr->next;
	if (thr->next)
		thr->next->prev = thr->prev;
	if (thr == thr_last)
		thr_last = thr->prev;
	if (thr == thr_first)
		thr_first = thr->next;

	vmmFree(vmmArea(thr->process, thr->info));

	//free(thr->kernel_stack);
	//memFree(thr->user_stack, 1);
	thr->state = THREAD_DELETED;
	hndSignal(thr, true);
	
	if (thr == current)
	{
		//wprintf(L"thrDelete: ending current thread (%d)\n", current->id);
		//old_current = NULL;
		thrSchedule();
	}

	hndFree(thr);
	//semRelease(&sem_scheduler);
}

//! Determines whether a thread is ready to continue execution.
/*!
 *	This function is called by thrSchedule() in order to find the next thread
 *		that is ready to execute.
 *
 *	A thread may be unable to execute for any of the following reasons:
 *	- It is the idle thread (thr->id == 0)
 *	- It is suspended (thr->suspend != 0)
 *	- It is sleeping for a specific duration of time (thr->state == 
 *		THREAD_WAIT_TIMEOUT)
 *	- It is waiting for a handle to be signalled (thr->state == 
 *		THR_WAIT_HANDLE)
 *
 *	\param	thr	The thread to be checked
 *	\return	Returns true if the thread can execute, or false otherwise.
 */
#if 0
bool thrIsReady(thread_t* thr)
{
	if (thr->suspend || thr->id == 0)
		return false;
	else if (thr->state == THREAD_WAIT_TIMEOUT)
	{
		if (uptime >= thr->wait.time)
		{
			//wprintf(L"%d: sleep ended\n", thr->id);
			thr->state = THREAD_RUNNING;
			return true;
		}
		else
		{
			//wprintf(L"%d: sleeping\r", thr->id);
			return false;
		}
	}
	else if (thr->state == THREAD_WAIT_HANDLE)
	{
		bool wait_end = false;
		unsigned i, first = -1;

		if (thr->wait.handle.wait_all)
		{
			wait_end = true;

			/*
			 * Keep searching until we find a non-signalled handle;
			 *	if we found a signalled handle, the wait can end.
			 */

			for (i = 0; i < thr->wait.handle.num_handles; i++)
				if (thr->wait.handle.handles[i] != NULL &&
					!hndIsSignalled(thr->wait.handle.handles[i]))
				{
					/* This handle is not signalled: the wait cannot end */
					wait_end = false;
					break;
				}
				else if (first == -1)
				{
					//wprintf(L"WaitAND: Handle %d ready\n", i);

					/* Save the index of the first signalled handle */
					first = i;
				}
		}
		else
		{
			wait_end = false;

			for (i = 0; i < thr->wait.handle.num_handles; i++)
				if (thr->wait.handle.handles[i] != NULL &&
					hndIsSignalled(thr->wait.handle.handles[i]))
				{
					//wprintf(L"WaitOR: Handle %d ready\n", i);
					wait_end = true;
					first = i;
					break;
				}
		}

		if (wait_end)
		{
			//context_t* ctx = thrContext(thr);
			//wprintf(L"[%d] wait ended on handle %u = %p\n", 
				//thr->id, first, thr->wait.handle.handles[first]);

			/* Reset all handles */
			for (i = 0; i < thr->wait.handle.num_handles; i++)
				hndSignal(thr->wait.handle.handles[i], false);

			/* Clear the thread's wait state */
			hndFree(thr->wait.handle.handles);
			thr->wait.handle.handles = NULL;
			thr->wait.handle.num_handles = 0;

			thr->state = THREAD_RUNNING;
			//ctx->regs.eax = first;
			return true;
		}
		else
		{
			//wprintf(L"%d: waiting\r", thr->id);
			return false;
		}
	}
	else
	{
		//wprintf(L"%d: running\r", thr->id);
		return true;
	}
}
#endif

//! Schedules a new thread for execution.
/*!
 *	This function is called on the timer IRQ by the isr() function. It finds
 *		the next thread ready for execution; if none are found it schedules the 
 *		idle thread.
 *
 *	It is possible to the kernel to be in a mixed context after this function 
 *		returns. current may point to any thread, but the full context switch 
 *		will not take place until the timer IRQ handler returns, causing the 
 *		new process's page directory to be loaded. However, the thread 
 *		information block (starting at FS:[0]) \e is loaded.
 *
 *	If the current thread is suspended then a reschedule will not take place.
 *		Therefore, it is possible for a thread which is currently in some 
 *		critical section of code (such as a kernel device driver) to prevent
 *		a context switch from taking place while still keeping interrupts
 *		enabled.
 */
void thrSchedule()
{
	//bool end = false;
	dword mask;
	int i;
	thread_t *thr, *next;

	if (current->suspend /*|| !semTryAcquire(&sem_scheduler)*/)
		return;

	/*
	//wprintf(L"%x\r", tss.esp0);
	*((word*) 0xb8000) = 0x7120;
	do
	{
		// *((word*) 0xb809c) = 0x7100 | (current->id + '0');

		//for (i = 0; i < 2500; i++)
			//;

		current = current->next;
		if (!current)
		{
			if (end)
			{
				current = &thr_idle;
				break;
			}

			end = true;
			current = thr_first;
		}
	} while (!thrIsReady(current));
	*/

	for (thr = thr_sleeping.first; thr; thr = next)
	{
		next = thr->next_queue;

		if (uptime >= thr->wait.time)
		{
			thrDequeue(thr, &thr_sleeping);
			thrRun(thr);
		}
	}

	if (thr_canrun == 0)
		current = &thr_idle;
	else
	{
		mask = thr_canrun;
		i = 0;

		while ((mask & 1) == 0)
		{
			i++;
			mask >>= 1;
		}

		if (thr_running[i].current == NULL)
			thr_running[i].current = thr_running[i].first;

		assert(thr_running[i].current != NULL);

		current = thr_running[i].current;
		thr_running[i].current = thr_running[i].current->next_queue;
		if (thr_running[i].current == NULL)
			thr_running[i].current = thr_running[i].first;
	}
	
	assert(current->info != NULL);
	i386_set_descriptor(_gdt + 8, (addr_t) current->info, 1, 
		ACS_DATA | ACS_DPL_3, ATTR_BIG | ATTR_GRANULARITY);

	*((word*) 0xb8000) = 0x7100 | (current->id + '0');

	//semRelease(&sem_scheduler);
}

//! Returns the thread context structure for the specified thread.
/*!
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
context_t* thrContext(thread_t* thr)
{
	return (context_t*) (thr->kernel_esp - 4);
}

//! Places the specified thread in a sleeping state until the specified interval has elapsed.
/*!
 *	If the thread is currently executing then a reschedule will take place.
 *
 *	\param	thr	The thread to be slept
 *	\param	ms	The duration of the sleep, in milliseconds
 */
void thrSleep(thread_t* thr, dword ms)
{
	thr->wait.time = uptime + ms;
	thr->state = THREAD_WAIT_TIMEOUT;
	thrDequeue(thr, &thr_running[thr->priority]);
	thrEnqueue(thr, &thr_sleeping);
	if (thr == current)
		thr_needschedule = true;
}

bool thrWaitFinished(void** hnd, unsigned num_handles, bool wait_all)
{
	bool any_waiting = false,
		any_signalled = false;
	unsigned i;

	for (i = 0; i < num_handles; i++)
	{
		if (hndIsSignalled(hnd[i]))
		{
			hndSignal(hnd[i], false);
			hnd[i] = NULL;
			any_signalled = true;
		}

		if (hnd[i] != NULL)
			any_waiting = true;
	}

	if ((wait_all && !any_waiting) || (!wait_all && any_signalled))
		return true;
	else
		return false;
}

//! Causes the specified thread to be paused until the specified handle is signalled.
/*!
 *	If the thread is currently executing then a reschedule will take place.
 *
 *	\param	thr	The thread which will wait
 *	\param	hnd	The handle to wait for
 */
void thrWaitHandle(thread_t* thr, void** hnd, unsigned num_handles, bool wait_all)
{
	context_t* ctx = thrContext(thr);
	int i;
	void** temp;

	//wprintf(L"thrWaitHandle(%p, %S(%d))\n", thr, handle->file, handle->line);
	temp = hndAlloc(sizeof(void*) * num_handles, NULL);
	memcpy(temp, hnd, sizeof(void*) * num_handles);

	if (thr != current ||
		(ctx->intr == 0x30 && ctx->regs.eax == 0x0101))
	{
		//_cputws(L"User wait\n");

		if (thrWaitFinished(temp, num_handles, wait_all))
		{
			handle_t *h;
			h = hndHandle(hnd[0]);
			//wprintf(L"%d: %S(%d) already signalled\n", 
				//thr->id, h->file, h->line);
			hndFree(temp);
			return;
		}

		for (i = 0; i < num_handles; i++)
		{
			handle_t *h = hndHandle(hnd[i]);

			if (hnd[i])
			{
				//wprintf(L"Moving thread %d from %u queue to handle %S(%d) queue\n",
					//thr->id, thr->priority, h->file, h->line);
				thrDequeue(thr, &thr_running[thr->priority]);
				thrEnqueue(thr, &h->queue);
			}
		}

		thr->wait.handle.handles = temp;
		thr->wait.handle.num_handles = num_handles;
		thr->wait.handle.wait_all = wait_all;
		thr->state = THREAD_WAIT_HANDLE;
		thrSchedule();
	}
	else
	{
		//unsigned i;
		//bool wait_end = false;
		//dword flags;

		/*
		 * xxx - this is a very kludgy way of determining whether we are being
		 *	called from kernel or user mode
		 * thrWaitHandle is being effectively emulated by a busy-wait
		 */

		wprintf(L"thrWaitHandle: current = %d, intr = %x\n",
			current->id, ctx->intr);

		while (!thrWaitFinished(temp, num_handles, wait_all))
			enable();

		hndFree(temp);

		/*
		asm("int3");
		wprintf(L"%d: start wait\n", thr->id);
		//while (!hndIsSignalled(hnd))
			//enable();
		
		asm("pushfl\n"
			"pop %0" : "=g" (flags));
		thrSuspend(current, true);

		while (!wait_end)
		{
			if (wait_all)
			{
				wait_end = true;

				for (i = 0; i < num_handles; i++)
					if (!hndIsSignalled(hnd[i]))
					{
						wait_end = false;
						break;
					}
			}
			else
			{
				wait_end = false;

				for (i = 0; i < num_handles; i++)
					if (hndIsSignalled(hnd[i]))
					{
						wait_end = true;
						break;
					}
			}

			enable();
		}

		thrSuspend(current, false);
		asm("push %0\n"
			"popfl" : : "g" (flags));
		//wprintf(L"%d: signalled\n", thr->id);*/
	}
}

//! Calls a function in the context of the specified thread.
/*!
 *	Pushes the parameters provided onto the thread's stack. The function
 *		will be called as the thread returns to user mode.
 *
 *	\param	addr	A pointer to the function to be executed. To avoid 
 *		stack corruption, this function must be declared __stdcall.
 *	\param	params	The parameters to be passed to the function. On the IA32
 *		architecture, these should be multiples of 32 bits in width and
 *		aligned on 32-bit boundaries.
 *	\param	sizeof_params	The size, in bytes, of the parameters pointed to
 *		by params. This should be a multiple of 32 bits on the IA32 
 *		architecture.
 */
void thrCall(thread_t* thr, void* addr, void* params, size_t sizeof_params)
{
	context_t* ctx = thrContext(thr);
	
	if (current->process->level > 0)
	{
		assert(ctx->esp > 0 && ctx->esp <= 0x80000000);
		ctx->esp -= sizeof_params;
		memcpy((void*) ctx->esp, params, sizeof_params);
		ctx->esp -= 4;
		*((dword*) ctx->esp) = ctx->eip;
		ctx->eip = (dword) addr;
	}
	//else
		//i386_docall(addr, params, sizeof_params);
}

thread_t* thrCurrent()
{
	return current;
}

bool thrDequeue(thread_t* thr, thread_queue_t* queue)
{
	thread_t *found;

	for (found = queue->first; found; found = found->next_queue)
		if (found == thr)
			break;

	if (found == NULL)
		return false;

	if (thr->prev_queue)
		thr->prev_queue->next_queue = thr->next_queue;
	if (thr->next_queue)
		thr->next_queue->prev_queue = thr->prev_queue;
	if (thr == queue->first)
		queue->first = thr->next_queue;
	if (thr == queue->last)
		queue->last = thr->prev_queue;
	thr->next_queue = thr->prev_queue = NULL;

	if (queue->current == thr)
	{
		queue->current = thr->next_queue;

		if (queue->current == NULL)
			queue->current = queue->first;
	}

	if (queue == &thr_running[thr->priority])
		if (thr_running[thr->priority].first == NULL)
			thr_canrun &= ~(1 << thr->priority);

	return true;
}

void thrEnqueue(thread_t* thr, thread_queue_t* queue)
{
	assert(thr->next_queue == NULL);
	assert(thr->prev_queue == NULL);

	if (queue->last)
		queue->last->next_queue = thr;
	thr->prev_queue = queue->last;
	thr->next_queue = NULL;
	queue->last = thr;
	if (queue->first == NULL)
		queue->first = thr;

	if (queue->current == NULL)
		queue->current = thr;
}

void thrRun(thread_t* thr)
{
	assert(thr->priority < countof(thr_running));
	//wprintf(L"Moving thread %d onto run queue\n", thr->id);
	thrEnqueue(thr, &thr_running[thr->priority]);
	thr_canrun |= 1 << thr->priority;
	thr->state = THREAD_RUNNING;
	thr_needschedule = true;
}

void thrSuspend(thread_t* thr, bool suspend)
{
	if (suspend)
		thr->suspend++;
	else
		thr->suspend--;

	if (suspend && thr->suspend == 1)
	{
		wprintf(L"Suspending thread %d\n", thr->id);
		thrDequeue(thr, &thr_running[thr->priority]);
		thrEnqueue(thr, &thr_suspended);
	}
	else if (!suspend && thr->suspend == 0)
	{
		wprintf(L"Unsuspending thread %d\n", thr->id);
		thrDequeue(thr, &thr_suspended);
		thrRun(thr);
	}
}

bool thrSetPriority(thread_t* thr, unsigned priority)
{
	if (priority >= 32)
		return false;

	if (thr->suspend || thr->wait.handle.num_handles)
		thr->priority = priority;
	else
	{
		thrDequeue(thr, &thr_running[thr->priority]);
		thr->priority = priority;
		thrRun(thr);
	}

	return true;
}