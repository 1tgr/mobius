/* $Id: handle.c,v 1.13 2002/06/14 13:05:35 pavlovskii Exp $ */

#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

#include <kernel/debug.h>

#include <stdlib.h>
#include <stdio.h>

handle_t _HndAlloc(struct process_t *proc, size_t size, uint32_t tag, 
				  const char *file, unsigned line)
{
	handle_hdr_t *ptr;
	ptr = malloc(sizeof(handle_hdr_t) + size);
	memset(ptr, 0, sizeof(handle_hdr_t) + size);
	_HndInit(ptr, tag, file, line);
	return HndDuplicate(proc, ptr);
}

bool HndClose(struct process_t *proc, handle_t hnd, uint32_t tag)
{
	handle_hdr_t *ptr;
	
	/*wprintf(L"HndClose: %lx(%.4S)\n", hnd, &tag);*/
	ptr = HndGetPtr(proc, hnd, tag);
	if (ptr == NULL)
		return false;

	if (proc == NULL)
		proc = current->process;

	proc->handles[hnd] = NULL;

	KeAtomicDec((unsigned*) &ptr->copies);
	if (ptr->copies == 0)
	{
		HndSignalPtr(ptr, 0);
		/*wprintf(L"HndClose(%s, %u): freeing handle\n", proc->exe, hnd);*/
		if (ptr->free_callback)
			ptr->free_callback(ptr);
		else
			free(ptr);
	}

	return true;
}

handle_hdr_t *HndGetPtr(struct process_t *proc, handle_t hnd, uint32_t tag)
{
	handle_hdr_t *ptr;

	if (proc == NULL)
		proc = current->process;

	if (proc->handles == NULL)
	{
		wprintf(L"HndGetPtr(%s, %ld): process handle table not created\n", 
			proc->exe, hnd);
		return NULL;
	}

	if (hnd >= proc->handle_count)
	{
		wprintf(L"HndGetPtr(%s, %lx): invalid handle (> %lx)\n", 
			proc->exe, hnd, proc->handle_count);
                __asm__("int3");
		return NULL;
	}

	ptr = proc->handles[hnd];

	if (ptr == NULL)
	{
		/*wprintf(L"HndGetPtr(%s, %ld): freed handle accessed\n", 
			proc->exe, hnd);*/
		return NULL;
	}

	if (tag != 0 &&
		ptr->tag != tag)
	{
		wprintf(L"HndGetPtr(%s, %ld, %p:%d): tag mismatch (%c%c%c%c/%c%c%c%c)\n", 
			proc->exe, hnd, ptr->file, ptr->line,
			((char*) &ptr->tag)[0], ((char*) &ptr->tag)[1], ((char*) &ptr->tag)[2], ((char*) &ptr->tag)[3],
			((char*) &tag)[0], ((char*) &tag)[1], ((char*) &tag)[2], ((char*) &tag)[3]);
		return NULL;
	}

	return ptr;
}

void *HndLock(struct process_t *proc, handle_t hnd, uint32_t tag)
{
	handle_hdr_t *ptr;

	ptr = HndGetPtr(proc, hnd, tag);
	if (ptr == NULL)
		return NULL;
	/*else if (ptr->locks > 0 && ptr->locked_by != current)
		return NULL;*/
	else
	{
		KeAtomicInc(&ptr->locks);
		ptr->locked_by = current;
		return (void*) (ptr + 1);
	}
}

void HndUnlock(struct process_t *proc, handle_t hnd, uint32_t tag)
{
	handle_hdr_t *ptr;

	ptr = HndGetPtr(proc, hnd, tag);
	if (ptr != NULL && ptr->locks > 0)
	{
		KeAtomicDec(&ptr->locks);

		if (ptr->locks == 0)
			ptr->locked_by = NULL;
	}
}

void HndSignal(struct process_t *proc, handle_t hnd, uint32_t tag, bool sig)
{
	handle_hdr_t *ptr;

	/*if (hnd != 7)
		wprintf(L"HndSignal: %lx(%.4S)\n", hnd, &tag);*/
	ptr = HndGetPtr(proc, hnd, tag);
	if (ptr != NULL)
		HndSignalPtr(ptr, sig);
}

bool HndIsSignalled(struct process_t *proc, handle_t hnd, uint32_t tag)
{
	handle_hdr_t *ptr;

	ptr = HndGetPtr(proc, hnd, tag);
	if (ptr != NULL)
		return ptr->signals > 0;
	else
		return false;
}

void _HndInit(handle_hdr_t *ptr, uint32_t tag, const char *file, unsigned line)
{
	ptr->tag = tag;
	ptr->file = file;
	ptr->line = line;
}

void HndRemovePtrEntries(struct process_t *proc, handle_hdr_t *ptr)
{
	handle_t hnd;

	if (proc == NULL)
		proc = current->process;

	assert(proc->handle_count == 0 || proc->handles != NULL);
	for (hnd = 0; hnd < proc->handle_count; hnd++)
		if (proc->handles[hnd] == ptr)
		{
			KeAtomicDec((unsigned*) &ptr->copies);
			proc->handles[hnd] = NULL;
		}
}

handle_t HndDuplicate(process_t *proc, handle_hdr_t *ptr)
{
	if (proc == NULL)
		proc = current->process;

	/*wprintf(L"HndDuplicate: handles = %p handle_count = %u\n", 
		proc->handles, proc->handle_count);*/

	KeAtomicInc(&proc->handle_count);
	if (proc->handle_count > proc->handle_allocated)
	{
		proc->handle_allocated += 16;
		proc->handles = realloc(proc->handles, proc->handle_allocated * sizeof(void*));
		if (proc->handles == NULL)
		{
			wprintf(L"handle_count = %u, handle_allocated = %u\n", 
				proc->handle_count, proc->handle_allocated);
			assert(proc->handles != NULL);
		}
	}

	proc->handles[proc->handle_count - 1] = ptr;
	KeAtomicInc((unsigned*) &ptr->copies);

	return proc->handle_count - 1;
}

void HndSignalPtr(handle_hdr_t *ptr, bool sig)
{
	if (sig)
		KeAtomicInc((unsigned*) &ptr->signals);
	else
		KeAtomicDec((unsigned*) &ptr->signals);

	if (ptr->signals > 0 &&
		ptr->waiting.first != NULL)
	{
		TRACE2("HndSignalPtr(%S:%d): resuming threads: ", ptr->file, ptr->line);
		ThrRunQueue(&ptr->waiting);
		/*for (thr = ptr->waiting.first; thr; thr = next)
		{
			wprintf(L"%u ", thr->id);
			next = thr->queue_next;
			ThrRemoveQueue(thr, &ptr->waiting);
			ThrRun(thr);
		}*/

		TRACE0("\n");
		assert(ptr->waiting.first == NULL);
		KeAtomicDec((unsigned*) &ptr->signals);
	}
}

handle_t EvtAlloc(process_t *proc)
{
	return HndAlloc(proc, 0, 'evnt');
}

void EvtSignal(process_t *proc, handle_t evt)
{
	HndSignal(proc, evt, 0, true);
}

bool EvtIsSignalled(process_t *proc, handle_t evt)
{
	return HndIsSignalled(proc, evt, 0);
}
