/* $Id: queue.c,v 1.2 2002/12/18 23:08:19 pavlovskii Exp $ */

#include <os/queue.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <stddef.h>

void *QueueAppend(queue_t *queue, const void *data, size_t size)
{
    void *ptr;

    if (size == 0)
        return queue->data;

    MuxAcquire(queue->lock);

    if (queue->length + size > queue->allocated)
    {
        void *newData;

        queue->allocated = (queue->allocated + PAGE_SIZE) & -PAGE_SIZE;
        newData = VmmAlloc(queue->allocated / PAGE_SIZE, NULL, 
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);

        if (newData == NULL)
        {
            MuxRelease(queue->lock);
            return NULL;
        }
        
        if (queue->data != NULL)
        {
            memcpy(newData, queue->data, queue->length);
            VmmFree(queue->data);
        }

        queue->data = newData;
    }

    //wprintf(L"queueAppend: data = %p length = %d allocated = %d size = %d\n", 
        //queue->data, queue->length, queue->allocated, size);
    ptr = ((uint8_t*) queue->data + queue->length);
    memcpy(ptr, data, size);
    queue->length += size;
    MuxRelease(queue->lock);
    return ptr;
}

bool QueuePopLast(queue_t *queue, void *data, size_t size)
{
    MuxAcquire(queue->lock);
    if (queue->length >= size)
    {
        queue->length -= size;
        if (data)
            memcpy(data, (uint8_t*) queue->data + queue->length, size);
        MuxRelease(queue->lock);
        return true;
    }
    else
    {
        MuxRelease(queue->lock);
        return false;
    }
}

bool QueuePopFirst(queue_t *queue, void *data, size_t size)
{
    MuxAcquire(queue->lock);
    if (queue->length >= size)
    {
        queue->length -= size;
        if (data)
            memcpy(data, (uint8_t*) queue->data, size);
        memcpy(queue->data, (uint8_t*) queue->data + size, queue->length);
        MuxRelease(queue->lock);
        return true;
    }
    else
    {
        MuxRelease(queue->lock);
        return false;
    }
}

void QueueClear(queue_t *queue)
{
    MuxAcquire(queue->lock);
    //vmmFree(queue->data);
    //queue->data = NULL;
    //queue->allocated = 0;
    queue->length = 0;
    MuxRelease(queue->lock);
}

void QueueInit(queue_t *queue)
{
    queue->data = NULL;
    queue->allocated = queue->length = 0;
    queue->lock = MuxCreate();
}
