#include <malloc.h>
#include <stdio.h>
#include <kernel/kernel.h>
#include <kernel/handle.h>
#include <kernel/proc.h>
#include <kernel/thread.h>

handle_t *hnd_first, *hnd_last;
extern byte scode[];

//! Implements the hndAlloc() macro.
/*!
 *	hndAlloc() is implemented as a macro. It calls this function, passing the 
 *		name of the file and the line from which it was called. This 
 *		information can later be used to debug the kernel.
 *
 *	This function allocates a block of kernel memory and associates it with
 *		the specified process. Use this function instead of malloc() if 
 *		possible.
 *
 *	The memory will be freed when the process exits. At exit, if there are any 
 *		handles still allocated, the locations of their allocation will be
 *		printed for debugging purposes.
 *
 *	Handles not allocated to a specific process should be allocted to 
 *		&proc_idle.
 *
 *	\param	size	The size of the block to be allocated
 *	\param	proc	The process in which the block is to be allocated
 *	\param	file	The name of the current source file (usually __FILE__)
 *	\param	line	The line from which this function is called (usually 
 *		__LINE__)
 *	\return	A pointer to the start of the block, or NULL if the block could not
 *		be allocated.
 */
void* _hndAlloc(size_t size, struct process_t* proc, const char* file, int line)
{
	handle_t* hnd = malloc(sizeof(handle_t) + size);

	hnd->next = NULL;
	if (hnd_last)
		hnd_last->next = hnd;
	if (!hnd_first)
		hnd_first = hnd;
	hnd_last = hnd;
	
	if (proc == NULL)
		proc = current->process;

	hnd->refs = 0;
	hnd->signal = false;
	hnd->process = proc;
	hnd->file = file;
	hnd->line = line;
	hnd->queue.first = hnd->queue.last = hnd->queue.current = NULL;
	return hnd + 1;
}

//! Increments the reference count on a handle
/*!
 *	Handles maintain a reference count internally: they are only freed when 
 *		their count reaches zero. Use hndAddRef() to increment this count
 *		manually (such as when making a copy of the pointer, in the same or
 *		a different process); use hndFree() to decrement it and potentially
 *		free the block entirely.
 *
 *	\param	buf	A pointer to the handle, returned by hndAlloc()
 *	\return	The new reference count on the handle
 */
int hndAddRef(void* buf)
{
	return ++hndHandle(buf)->refs;
}

//! Decrements the reference count on a handle, and potentially frees the block
/*!
 *	See the documentation for hndAddRef(). The handle is only freed when its
 *		reference count reaches zero.
 *
 *	\param	buf	A pointer to the handle, returned by hndAlloc()
 *	\return	The new reference count on the handle; this is zero if the block
 *		was freed.
 */
int hndFree(void* buf)
{
	handle_t *hnd, *prev;

	if (buf == NULL)
		return 0;

	hnd = hndHandle(buf);
	assert((addr_t) hnd >= (addr_t) &scode);

	if (hnd->refs == 0)
	{
		assert(hnd->queue.first == NULL);

		for (prev = hnd_first; prev && prev->next != hnd; prev = prev->next)
			;

		if (prev)
			prev->next = hnd->next;
		if (hnd == hnd_last)
			hnd_last = prev;
		if (hnd == hnd_first)
			hnd_first = hnd->next;
		
		free(hnd);
		return 0;
	}
	else
		return --hnd->refs;
}

//!	Sets the signal on a handle
/*!
 *	Handles double as signallable objects. This function either sets or resets
 *		a handle's signal. Any threads waiting on the handle will be woken next
 *		time thrSchedule() is called.
 *
 *	\param	buf	A pointer to the handle to be signalled
 *	\param	signal	The new state of the signal
 */
void hndSignal(void* buf, bool signal)
{
	thread_t *thr, *next;
	handle_t *hnd;

	if (buf != NULL)
	{
		hnd = hndHandle(buf);

		hnd->signal = signal;

		if (signal)
		{
			//if (hnd->queue.first)
				//wprintf(L"Signalled handle %S(%d)\n", hnd->file, hnd->line);
		
			for (thr = hnd->queue.first; thr; thr = next)
			{
				//wprintf(L"hndSignal: Checking thread %d...", thr->id);
				next = thr->next_queue;
				thrDequeue(thr, &hnd->queue);

				if (thrWaitFinished(thr->wait.handle.handles, 
					thr->wait.handle.num_handles, 
					thr->wait.handle.wait_all))
				{
					//wprintf(L"running\n");
					hndFree(thr->wait.handle.handles);
					thr->wait.handle.handles = NULL;
					thr->wait.handle.num_handles = 0;
					//wprintf(L"%d: wait on %S(%d) finished\n",
						//thr->id, hnd->file, hnd->line);
					thrRun(thr);
				}
				//else
					//wprintf(L"not running\n");
			}
		}
	}
}

//!	Returns the state of a handle's signal
/*!
 *	See the documentation of hndSignal().
 *	\param	buf	A pointer to the handle to be tested
 */
bool hndIsSignalled(void* buf)
{
	if (buf == NULL)
		return false;
	else
		return hndHandle(buf)->signal;
}

//!	Iterates through the handles allocated to the specified process, and prints the locations of their allocations.
/*!
 *	This function is useful as a debugging checkpoint. It is called, for 
 *		instance, by the process termination code, which ensures that
 *		all handles allocated to a process are freed.
 */
void hndEnum(process_t* proc)
{
	handle_t* hnd;
	int i = 0;

	for (hnd = hnd_first; hnd; hnd = hnd->next)
		if (proc == NULL || hnd->process == proc)
		{
			wprintf(L"%S(%d)\n", hnd->file, hnd->line);
			i++;
		}

	if (i)
		wprintf(L"%d handle(s)\n", i);
}