/* $Id: io.c,v 1.1 2002/01/09 01:23:39 pavlovskii Exp $ */
#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/io.h>
#include <kernel/arch.h>
#include <kernel/thread.h>

#include <kernel/debug.h>

#include <stdio.h>

/* Note: IoOpenDevice and IoCloseDevice are in device.c */

void IoNotifyCompletion(device_t *dev, request_t *req)
{
	request_t temp;

	if (req->from != NULL)
	{
		temp.code = IO_FINISH;
		temp.result = 0;
		temp.event = NULL;
		temp.original = req;
		temp.from = dev;
		assert(req->from->vtbl->request != NULL);
		req->from->vtbl->request(req->from, &temp);
		req->from = NULL;
	}
}

bool IoRequest(device_t *from, device_t *dev, request_t *req)
{
	bool ret;

	TRACE1("\t\tIoRequest: dev = %p\n", dev);
	req->event = NULL;
	req->result = 0;
	req->from = from;

	if (dev == NULL)
	{
		wprintf(L"IoRequest fails on NULL device\n");
		return false;
	}

	if (dev->vtbl == NULL)
	{
		wprintf(L"IoRequest fails on NULL vtbl\n");
		return false;
	}

	if (dev->vtbl->request == NULL)
	{
		wprintf(L"IoRequest fails on NULL request function\n");
		return false;
	}

	ret = dev->vtbl->request(dev, req);
	if (ret && req->event == NULL)
	{
		/*
		 * Operation completed synchronously. 
		 * There might be some device expecting completion notification.
		 */
		/*assert(false && "Caller expected async io but device completed immediately");*/
		IoNotifyCompletion(dev, req);
	}

	return ret;
}

bool IoRequestSync(device_t *dev, request_t *req)
{
	TRACE1("\tIoRequestSync: dev = %p\n", dev);
	if (IoRequest(NULL, dev, req))
	{
		/*assert(req->event == NULL);*/
		if (req->event)
		{
			if (!EvtIsSignalled(NULL, req->event))
			{
				semaphore_t temp;
				SemInit(&temp);
				SemAcquire(&temp);
				enable();
					
				if (/*true || */current == &thr_idle)
				{
					TRACE0("IoRequestSync: busy-waiting\n");
					while (!EvtIsSignalled(NULL, req->event))
						ArchProcessorIdle();
				}
				else
				{
					wprintf(L"IoRequestSync: doing proper wait\n");
					ThrWaitHandle(current, req->event, 'evnt');
					
					while (!EvtIsSignalled(NULL, req->event))
					{
						ArchProcessorIdle();
						wprintf(L"X");
					}

					/*assert(false && "Reached this bit");*/
				}

				SemRelease(&temp);
			}

			EvtFree(NULL, req->event);
		}

		return true;
	}
	else
		return false;
}

size_t IoReadSync(device_t *dev, uint64_t ofs, void *buf, size_t size)
{
	request_dev_t req;
	req.header.code = DEV_READ;
	req.params.dev_read.offset = ofs;
	req.params.dev_read.buffer = buf;
	req.params.dev_read.length = size;

	TRACE1("IoReadSync: dev = %p\n", dev);
	if (IoRequestSync(dev, &req.header))
		return req.params.dev_read.length;
	else
		return (size_t) -1;
}
