/* $Id: queue.c,v 1.1.1.1 2002/12/21 09:50:27 pavlovskii Exp $ */

#include <os/queue.h>
#include <os/syscall.h>
#include <os/defs.h>

#include <stddef.h>
#include <string.h>

void *QueueAppend(queue_t *queue, const void *data, size_t size)
{
    void *ptr;

    if (size == 0)
        return queue->data;

    SemDown(queue->lock);

    if (queue->length + size > queue->allocated)
    {
        void *newData;

        queue->allocated = (queue->allocated + PAGE_SIZE) & -PAGE_SIZE;
        newData = VmmAlloc(queue->allocated / PAGE_SIZE, NULL, 
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);

        if (newData == NULL)
        {
            SemUp(queue->lock);
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
    SemUp(queue->lock);
    return ptr;
}

bool QueuePopLast(queue_t *queue, void *data, size_t size)
{
    SemDown(queue->lock);
    if (queue->length >= size)
    {
        queue->length -= size;
        if (data)
            memcpy(data, (uint8_t*) queue->data + queue->length, size);
        SemUp(queue->lock);
        return true;
    }
    else
    {
        SemUp(queue->lock);
        return false;
    }
}

bool QueuePopFirst(queue_t *queue, void *data, size_t size)
{
    SemDown(queue->lock);
    if (queue->length >= size)
    {
        queue->length -= size;
        if (data)
            memcpy(data, (uint8_t*) queue->data, size);
        memcpy(queue->data, (uint8_t*) queue->data + size, queue->length);
        SemUp(queue->lock);
        return true;
    }
    else
    {
        SemUp(queue->lock);
        return false;
    }
}

void QueueClear(queue_t *queue)
{
    SemDown(queue->lock);
    //vmmFree(queue->data);
    //queue->data = NULL;
    //queue->allocated = 0;
    queue->length = 0;
    SemUp(queue->lock);
}

void QueueInit(queue_t *queue)
{
    queue->data = NULL;
    queue->allocated = queue->length = 0;
    queue->lock = SemCreate(1);
}
