/* $Header: /include/kernel/handle.h 4     13/03/04 22:47 Tim $ */
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
 *  \ingroup    kernel
 *  \defgroup   hnd Handles
 *  @{
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
    thread_queuent_t *first, *last;
};


typedef struct handle_hdr_t handle_hdr_t;

typedef struct handle_class_t handle_class_t;
struct handle_class_t
{
    spinlock_t lock;
    uint32_t tag;
    void (*free)(void*);
    bool (*before_wait)(void*);
    void (*after_wait)(void*);
    bool (*is_signalled)(void*);
};

#define HANDLE_CLASS(tag, free, before_wait, after_wait, is_signalled) \
    { { 0 }, tag, free, before_wait, after_wait, is_signalled }
#define HANDLE_CLASS_FREE(tag) \
    HANDLE_CLASS(tag, HndCleanup, NULL, NULL, NULL)

struct process_t;

struct handle_hdr_t
{
    spinlock_t lock;
    handle_class_t *cls;
    thread_queue_t waiting;
    const char *file;
    unsigned line;
    uint32_t copies;
};

#define HND_FLAG_INHERITABLE    1
#define HANDLE_ENTRY_TO_POINTER(e)  ((handle_hdr_t*) (((addr_t) e) & ~1))
#define HANDLE_ENTRY_TO_FLAGS(e)    ((unsigned) (((addr_t) e) & 1))

void    _HndInit(handle_hdr_t *hdr, handle_class_t *cls, const char *file, unsigned line);
#define HndInit(hdr, cls) _HndInit(hdr, cls, __FILE__, __LINE__)
void    HndDelete(handle_hdr_t *hdr);

void    HndAcquire(handle_hdr_t *hdr);
void    HndRelease(handle_hdr_t *hdr);

handle_hdr_t *HndCopy(handle_hdr_t *hdr);
void    HndClose(handle_hdr_t *hdr);
void    HndCleanup(void *hdr);

handle_hdr_t *HndRef(struct process_t *proc, handle_t hnd, uint32_t tag);
handle_t HndDuplicate(handle_hdr_t *hdr, struct process_t *proc);

void    HndInheritHandles(struct process_t *src, struct process_t *dest);
bool    HndSetInheritable(struct process_t *proc, handle_t hndl, bool inheritable);
void    HndCompleteWait(handle_hdr_t *hdr, bool release_all);
bool    HndIsSignalled(handle_hdr_t *hdr);
handle_hdr_t *HndOpen(const wchar_t *name);
bool    HndExport(handle_hdr_t *hdr, const wchar_t *name);


typedef struct event_t event_t;
struct event_t
{
    handle_hdr_t hdr;
    bool is_signalled;
};


typedef struct semaphore_t semaphore_t;
struct semaphore_t
{
    handle_hdr_t hdr;
    int count;
};

event_t *EvtCreate(bool is_signalled);
void    EvtSignal(event_t *evt, bool is_signalled);

semaphore_t *SemCreate(int count);
bool	SemUp(semaphore_t *sem);
bool	SemDown(semaphore_t *sem);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif
