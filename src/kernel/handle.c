#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

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

bool HndFree(struct process_t *proc, handle_t hnd, uint32_t tag)
{
	handle_hdr_t *ptr;
	
	/*wprintf(L"HndFree: %lx(%.4S)\n", hnd, &tag);*/
	ptr = HndGetPtr(proc, hnd, tag);
	if (ptr == NULL)
		return false;

	if (proc == NULL)
		proc = current->process;

	proc->handles[hnd] = NULL;

	ptr->copies--;
	if (ptr->copies == 0)
	{
		HndFreePtr(ptr);
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
		wprintf(L"HndGetPtr(%ld): process handle table not created\n", hnd);
		return NULL;
	}

	if (hnd >= proc->handle_count)
	{
		wprintf(L"HndGetPtr(%lx): invalid handle (> %lx)\n", 
			hnd, proc->handle_count);
		return NULL;
	}

	ptr = proc->handles[hnd];

	if (ptr == NULL)
	{
		wprintf(L"HndGetPtr(%ld): freed handle accessed\n", hnd);
		return NULL;
	}

	if (tag != 0 &&
		ptr->tag != tag)
	{
		wprintf(L"HndGetPtr(%ld): tag mismatch (%.4S/%.4S)\n", 
			hnd, &ptr->tag, &tag);
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
	else if (ptr->locks > 0 && ptr->locked_by != current)
		return NULL;
	else
	{
		ptr->locks++;
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
		ptr->locks--;

		if (ptr->locks == 0)
			ptr->locked_by = NULL;
	}
}

void HndSignal(struct process_t *proc, handle_t hnd, uint32_t tag, bool sig)
{
	handle_hdr_t *ptr;

	/*wprintf(L"HndSignal: %lx(%.4S)\n", hnd, &tag);*/
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

void HndFreePtr(handle_hdr_t *ptr)
{
	HndSignalPtr(ptr, 0);
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
			ptr->copies--;
			proc->handles[hnd] = NULL;
		}
}

handle_t HndDuplicate(process_t *proc, handle_hdr_t *ptr)
{
	if (proc == NULL)
		proc = current->process;

	proc->handle_count++;
	proc->handles = realloc(proc->handles, proc->handle_count * sizeof(void*));
	proc->handles[proc->handle_count - 1] = ptr;
	ptr->copies++;

	return proc->handle_count - 1;
}

void HndSignalPtr(handle_hdr_t *ptr, bool sig)
{
	thread_t *thr, *next;

	if (sig)
		ptr->signals++;
	else
		ptr->signals--;

	if (ptr->signals > 0 &&
		ptr->waiting.first != NULL)
	{
		wprintf(L"HndSignalPtr(%S:%d): resuming threads: ", ptr->file, ptr->line);
		for (thr = ptr->waiting.first; thr; thr = next)
		{
			wprintf(L"%u ", thr->id);
			next = thr->queue_next;
			ThrRemoveQueue(thr, &ptr->waiting);
			ThrRun(thr);
		}

		wprintf(L"\n");
		assert(ptr->waiting.first == NULL);
		ptr->signals--;
	}
}
