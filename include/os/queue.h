/* $Id: queue.h,v 1.3 2002/09/08 20:47:03 pavlovskii Exp $ */

#ifndef __OS_QUEUE_H
#define __OS_QUEUE_H

#include <sys/types.h>
#include <os/defs.h>

typedef struct queue_t queue_t;
struct queue_t
{
    void *data;
    size_t allocated, length;
#ifdef KERNEL
    spinlock_t lock;
#else
    handle_t lock;
#endif
};

#define QUEUE_DATA(q)           ((q).data)
#define QUEUE_COUNT(q, type)    ((q).length / sizeof(type))

#ifdef __cplusplus
extern "C"
{
#endif

void    *QueueAppend(queue_t *queue, const void *data, size_t size);
bool    QueuePopLast(queue_t *queue, void *data, size_t size);
bool    QueuePopFirst(queue_t *queue, void *data, size_t size);
void    QueueClear(queue_t *queue);
void    QueueInit(queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
