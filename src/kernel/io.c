/* $Id: io.c,v 1.4 2002/01/15 00:12:58 pavlovskii Exp $ */
#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/io.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <errno.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <stdio.h>

/* Note: IoOpenDevice and IoCloseDevice are in device.c */

void IoNotifyCompletion(request_t *req)
{
	/*request_t temp;*/

	if (req->from != NULL)
	{
		/*temp.code = IO_FINISH;
		temp.result = 0;
		temp.event = NULL;
		temp.original = req;
		temp.from = dev;
		assert(req->from->vtbl->request != NULL);
		req->from->vtbl->request(req->from, &temp);
		req->from = NULL;*/
		assert(req->from->vtbl->finishio != NULL);
		req->from->vtbl->finishio(req->from, (req->original == NULL) ? 
			req : req->original);
		req->from = NULL;
	}
}

bool IoRequest(device_t *from, device_t *dev, request_t *req)
{
	bool ret;

	TRACE1("\t\tIoRequest: dev = %p\n", dev);
	/*req->event = NULL;*/
	req->result = 0;
	req->from = from;
	req->original = NULL;

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
	if (ret && /*req->event == NULL*/req->result != SIOPENDING)
	{
		/*
		 * Operation completed synchronously. 
		 * There might be some device expecting completion notification.
		 */
		IoNotifyCompletion(req);
	}

	return ret;
}

int mal_verify(int fullcheck);

typedef struct syncrequest_t syncrequest_t;
struct syncrequest_t
{
	device_t dev;
	semaphore_t lock;
	bool is_completed;
};

void IoSyncRequestFinishIo(device_t *dev, request_t *req)
{
	syncrequest_t *sync;
	sync = (syncrequest_t*) dev;
	TRACE1("IoSyncRequestFinish: sync = %p\n", sync);
	SemAcquire(&sync->lock);
	sync->is_completed = true;
	SemRelease(&sync->lock);
}

static const device_vtbl_t syncrequest_vtbl =
{
	NULL,
	NULL,
	IoSyncRequestFinishIo
};

bool IoRequestSync(device_t *dev, request_t *req)
{
	syncrequest_t sync;

	memset(&sync, 0, sizeof(sync));
	sync.dev.vtbl = &syncrequest_vtbl;
	SemInit(&sync.lock);
	TRACE2("\tIoRequestSync: dev = %p, sync = %p\n", dev, &sync);
	if (IoRequest(&sync.dev, dev, req))
	{
		/*assert(req->event == NULL);*/
		if (/*req->event*/req->result == SIOPENDING)
		{
			/*mal_verify(1);*/
			if (!sync.is_completed /*!EvtIsSignalled(NULL, req->event)*/)
			{
				semaphore_t temp;
				SemInit(&temp);
				SemAcquire(&temp);
				ScEnableSwitch(false);
				enable();

				if (true || current == &thr_idle)
				{
					wprintf(L"IoRequestSync: busy-waiting\n");
					while (!sync.is_completed/*!EvtIsSignalled(NULL, req->event)*/)
						ArchProcessorIdle();
				}
#if 0
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
#endif

				ScEnableSwitch(true);
				SemRelease(&temp);
			}

			/*EvtFree(NULL, req->event);*/
			/*mal_verify(1);*/
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
