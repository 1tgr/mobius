/* $Id: handle.h,v 1.5 2002/08/04 17:22:39 pavlovskii Exp $ */
#ifndef __KERNEL_HANDLE_H
#define __KERNEL_HANDLE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/kernel.h>

struct thread_t;

/*!
 *	\ingroup	kernel
 *	\defgroup	hnd	Handles
 *	@{
 */

typedef struct thread_queuent_t thread_queuent_t;
struct thread_queuent_t
{
	thread_queuent_t *prev, *next;
	struct thread_t *thr;
};

typedef struct thread_queue_t thread_queue_t;
struct thread_queue_t
{
	thread_queuent_t *first, *last, *current;
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
	void (*free_callback)(void *);
};

handle_t	_HndAlloc(struct process_t *proc, size_t size, uint32_t tag,
					  const char *file, unsigned line);
#define		HndAlloc(proc, size, tag)	_HndAlloc(proc, size, tag, \
												__FILE__, __LINE__)

bool	HndClose(struct process_t *proc, handle_t hnd, uint32_t tag);
void *	HndLock(struct process_t *proc, handle_t hnd, uint32_t tag);
void	HndUnlock(struct process_t *proc, handle_t hnd, uint32_t tag);
handle_hdr_t *	HndGetPtr(struct process_t *proc, handle_t hnd, uint32_t tag);
void	HndSignal(struct process_t *proc, handle_t hnd, uint32_t tag, bool sig);
bool	HndIsSignalled(struct process_t *proc, handle_t hnd, uint32_t tag);

handle_t	HndDuplicate(struct process_t *proc, handle_hdr_t *ptr);
void	_HndInit(handle_hdr_t *ptr, uint32_t tag, const char *file, 
				 unsigned line);
#define	HndInit(ptr, tag)	_HndInit(ptr, tag, __FILE__, __LINE__)
void	HndSignalPtr(handle_hdr_t *ptr, bool sig);
void	HndRemovePtrEntries(struct process_t *proc, handle_hdr_t *ptr);

handle_t	EvtAlloc(struct process_t *proc);
void	EvtSignal(struct process_t *proc, handle_t evt);
bool	EvtIsSignalled(struct process_t *proc, handle_t evt);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif