#ifndef __THREAD_H
#define __THREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>

#define THREAD_RUNNING		0
#define THREAD_DELETED		1
#define THREAD_WAIT_TIMEOUT	2
#define THREAD_WAIT_HANDLE	3
#define THREAD_WAIT_OBJECT	4

struct context_t;
typedef bool (CDECL *V86HANDLER)(struct context_t*);

typedef struct thread_t thread_t;
struct thread_t
{
	struct process_t *process;
	dword kernel_esp;			// must be thread_t+4
	dword id;
	
	thread_t *prev, *next, *prev_queue,  *next_queue;
	unsigned priority;

	dword *kernel_stack, *user_stack;
	dword suspend;
	byte state;
	union
	{
		dword time;

		struct
		{
			void** handles;
			unsigned num_handles;
			bool wait_all;
		} handle;
	} wait;

	thread_info_t* info;
	V86HANDLER v86handler;
};

typedef struct thread_queue_t thread_queue_t;
struct thread_queue_t
{
	thread_t *first, *last, *current;
};

//CASSERT(offsetof(thread_t, process) == 0);
//CASSERT(offsetof(thread_t, kernel_esp) == 4);

thread_t*	thrCreate(int level, process_t* proc, const void* entry_point,
					  unsigned priority);
thread_t*	thrCreate86(process_t* proc, const byte* code, size_t code_size, 
						V86HANDLER handler, unsigned priority);
void		thrDelete(thread_t* thr);
void		thrSchedule();
context_t*	thrContext(thread_t* thr);
void		thrSleep(thread_t* thr, dword ms);
bool		thrWaitFinished(void** hnd, unsigned num_handles, bool wait_all);
void		thrWaitHandle(thread_t* thr, void** hnd, unsigned num_handles, 
						  bool wait_all);
void		thrCall(thread_t* thr, void* addr, void* params, 
					size_t sizeof_params);
thread_t*	thrCurrent();
bool		thrDequeue(thread_t* thr, thread_queue_t* queue);
void		thrEnqueue(thread_t* thr, thread_queue_t* queue);
void		thrRun(thread_t* thr);
void		thrSuspend(thread_t* thr, bool suspend);

extern thread_t DLLIMPORT *thr_first, *thr_last, *current;

#ifdef __cplusplus
}
#endif

#endif