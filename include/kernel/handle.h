/* $Id: handle.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
#ifndef __KERNEL_HANDLE_H
#define __KERNEL_HANDLE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/kernel.h>

struct thread_t;

typedef struct thread_queue_t thread_queue_t;
struct thread_queue_t
{
	struct thread_t *first, *last, *current;
	semaphore_t sem;
};

struct process_t;

typedef struct handle_hdr_t handle_hdr_t;
struct handle_hdr_t
{
	unsigned locks;
	uint32_t tag;
	struct thread_t *locked_by;
	uint32_t signals;
	thread_queue_t waiting;
	const char *file;
	unsigned line;
	uint32_t copies;
};

handle_t	_HndAlloc(struct process_t *proc, size_t size, uint32_t tag,
					  const char *file, unsigned line);
#define		HndAlloc(proc, size, tag)	_HndAlloc(proc, size, tag, \
												__FILE__, __LINE__)

bool	HndFree(struct process_t *proc, handle_t hnd, uint32_t tag);
void *	HndLock(struct process_t *proc, handle_t hnd, uint32_t tag);
void	HndUnlock(struct process_t *proc, handle_t hnd, uint32_t tag);
handle_hdr_t *	HndGetPtr(struct process_t *proc, handle_t hnd, uint32_t tag);
void	HndSignal(struct process_t *proc, handle_t hnd, uint32_t tag, bool sig);
bool	HndIsSignalled(struct process_t *proc, handle_t hnd, uint32_t tag);

handle_t	HndDuplicate(struct process_t *proc, handle_hdr_t *ptr);
void	_HndInit(handle_hdr_t *ptr, uint32_t tag, const char *file, 
				 unsigned line);
#define	HndInit(ptr, tag)	_HndInit(ptr, tag, __FILE__, __LINE__)
void	HndFreePtr(handle_hdr_t *ptr);
void	HndSignalPtr(handle_hdr_t *ptr, bool sig);
void	HndRemovePtrEntries(struct process_t *proc, handle_hdr_t *ptr);

#define EvtAlloc(proc)			HndAlloc(proc, 0, 'evnt')
#define EvtSignal(proc, evt)	HndSignal(proc, evt, 'evnt', true)
#define EvtFree(proc, evt)		HndFree(proc, evt, 'evnt')
#define EvtIsSignalled(proc, evt)	HndIsSignalled(proc, evt, 'evnt')

#ifdef __cplusplus
}
#endif

#endif