/* $Id: queue.c,v 1.1 2002/04/03 23:53:05 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <os/queue.h>
#include <stdlib.h>

void *QueueAppend(queue_t *queue, const void *data, size_t size)
{
    void *ptr;

    if (size == 0)
	return queue->data;

    SemAcquire(&queue->lock);

    if (queue->length + size > queue->allocated)
    {
	void *newData;

	queue->allocated = (queue->allocated + PAGE_SIZE) & -PAGE_SIZE;
	newData = malloc(queue->allocated);

	if (newData == NULL)
	{
	    SemRelease(&queue->lock);
	    return NULL;
	}
	
	if (queue->data != NULL)
	{
	    memcpy(newData, queue->data, queue->length);
	    free(queue->data);
	}

	queue->data = newData;
    }

    //wprintf(L"queueAppend: data = %p length = %d allocated = %d size = %d\n", 
	//queue->data, queue->length, queue->allocated, size);
    ptr = ((uint8_t*) queue->data + queue->length);
    memcpy(ptr, data, size);
    queue->length += size;
    SemRelease(&queue->lock);
    return ptr;
}

bool QueuePopLast(queue_t *queue, void *data, size_t size)
{
    SemAcquire(&queue->lock);
    if (queue->length >= size)
    {
	queue->length -= size;
	if (data)
	    memcpy(data, (uint8_t*) queue->data + queue->length, size);
	SemRelease(&queue->lock);
	return true;
    }
    else
    {
	SemRelease(&queue->lock);
	return false;
    }
}

bool QueuePopFirst(queue_t *queue, void *data, size_t size)
{
    SemAcquire(&queue->lock);
    if (queue->length >= size)
    {
	queue->length -= size;
	if (data)
	    memcpy(data, (uint8_t*) queue->data, size);
	memcpy(queue->data, (uint8_t*) queue->data + size, queue->length);
	SemRelease(&queue->lock);
	return true;
    }
    else
    {
	SemRelease(&queue->lock);
	return false;
    }
}

void QueueClear(queue_t *queue)
{
    SemAcquire(&queue->lock);
    free(queue->data);
    queue->data = NULL;
    queue->allocated = 0;
    queue->length = 0;
    SemRelease(&queue->lock);
}

void QueueInit(queue_t *queue)
{
    queue->data = NULL;
    queue->allocated = queue->length = 0;
    SemInit(&queue->lock);
}
