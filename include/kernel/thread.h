#ifndef __THREAD_H
#define __THREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>
#include <kernel/handle.h>

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
	thread_t *prev, *next;
	dword *kernel_stack, *user_stack/*, *v86_stack*/;
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

		//IUnknown* object;
	} wait;
	thread_info_t* info;
	V86HANDLER v86handler;
};

//CASSERT(offsetof(thread_t, process) == 0);
//CASSERT(offsetof(thread_t, kernel_esp) == 4);

thread_t* thrCreate(int level, process_t* proc, const void* entry_point);
thread_t* thrCreate86(process_t* proc, const byte* code, size_t code_size, V86HANDLER handler);
void thrDelete(thread_t* thr);
void thrSchedule();
context_t* thrContext(thread_t* thr);
void thrSleep(thread_t* thr, dword ms);
void thrWaitHandle(thread_t* thr, void** hnd, unsigned num_handles, bool wait_all);
void thrCall(thread_t* thr, void* addr, void* params, size_t sizeof_params);
thread_t* thrCurrent();

extern thread_t DLLIMPORT *thr_first, *thr_last, *current;

typedef struct semaphore_t semaphore_t;
struct semaphore_t
{
	thread_t* owner;
	int locks;
};

static inline void semInit(semaphore_t* sem)
{
	sem->owner = NULL;
	sem->locks = 0;
}

void semAcquire(semaphore_t* sem);
void semRelease(semaphore_t* sem);

#ifdef __cplusplus
}
#endif

#endif