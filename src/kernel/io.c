/* $Id: io.c,v 1.13 2002/08/17 19:13:32 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/io.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/fs.h>

#include <os/syscall.h>
#include <errno.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <stdio.h>

/* Note: IoOpenDevice and IoCloseDevice are in device.c */

void IoNotifyCompletion(request_t *req)
{
    io_callback_t cb;
    /*request_t temp;*/

    cb = req->callback;
    switch (cb.type)
    {
    case IO_CALLBACK_NONE:
        break;

    case IO_CALLBACK_DEVICE:
        assert(req->callback.u.dev != (void*) 0x31337);
        assert(req->callback.u.dev->vtbl->finishio != NULL);
        req->callback.u.dev = (void*) 0x31337;
        req->callback.type = IO_CALLBACK_NONE;
        cb.u.dev->vtbl->finishio(cb.u.dev, 
            (req->original == NULL) ? req : req->original);
        break;

    case IO_CALLBACK_FSD:
        assert(req->callback.u.fsd != (void*) 0xdeadbeef);
        assert(req->callback.u.fsd->vtbl->finishio != NULL);
        req->callback.u.fsd = (void*) 0xdeadbeef;
        req->callback.type = IO_CALLBACK_NONE;
        cb.u.fsd->vtbl->finishio(cb.u.fsd, 
            (req->original == NULL) ? req : req->original);
        break;

    case IO_CALLBACK_FUNCTION:
        assert(req->callback.u.function != (void*) 0xcafebabe);
        assert(req->callback.u.function != NULL);
        req->callback.u.function = (void*) 0xcafebabe;
        req->callback.type = IO_CALLBACK_NONE;
        cb.u.function((req->original == NULL) ? req : req->original);
        break;
    }
}

bool IoRequest(const io_callback_t *cb, device_t *dev, request_t *req)
{
    bool ret;

    /*TRACE1("\t\tIoRequest: dev = %p\n", dev);*/
    /*req->event = NULL;*/
    req->result = 0;
    //req->from = from;

    if (cb == NULL)
        req->callback.type = IO_CALLBACK_NONE;
    else
        req->callback = *cb;

    req->original = NULL;

    /*if (dev == NULL)
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
    }*/

    assert(dev != NULL);
    assert(dev->vtbl != NULL);
    assert(dev->vtbl->request != NULL);

    ret = dev->vtbl->request(dev, req);
    if (ret && /*req->event == NULL*/
        req->result != SIOPENDING &&
        req->callback.type != IO_CALLBACK_NONE)
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
    spinlock_t lock;
    //handle_t event;
    bool is_completed;
};

void IoSyncRequestFinishIo(device_t *dev, request_t *req)
{
    syncrequest_t *sync;
    sync = (syncrequest_t*) dev;
    TRACE1("IoSyncRequestFinish: sync = %p\n", sync);
    SpinAcquire(&sync->lock);
    //EvtSignal(NULL, sync->event);
    sync->is_completed = true;
    SpinRelease(&sync->lock);
}

static const device_vtbl_t syncrequest_vtbl =
{
    NULL,
    NULL,
    IoSyncRequestFinishIo
};

bool IoRequestSync(device_t *dev, request_t *req)
{
    io_callback_t cb;
    syncrequest_t sync;

    memset(&sync, 0, sizeof(sync));
    sync.dev.vtbl = &syncrequest_vtbl;
    SpinInit(&sync.lock);
    /*TRACE2("\tIoRequestSync: dev = %p, sync = %p\n", dev, &sync);*/
    cb.type = IO_CALLBACK_DEVICE;
    cb.u.dev = &sync.dev;
    if (IoRequest(&cb, dev, req))
    {
        /*assert(req->event == NULL);*/
        if (/*req->event*/req->result == SIOPENDING)
        {
            /*mal_verify(1);*/
            if (!sync.is_completed)
            {
                spinlock_t temp;
                SpinInit(&temp);
                SpinAcquire(&temp);
                ScEnableSwitch(false);
                enable();

                if (true || current() == &cpu()->thr_idle)
                {
                    TRACE0("IoRequestSync: busy-waiting\n");
                    while (!sync.is_completed)
                        /*ArchProcessorIdle();*/
                        KeYield();
                }
#if 0
                else
                {
                    wprintf(L"IoRequestSync: doing proper wait\n");
                    ThrWaitHandle(current(), sync.event, 'evnt');
                    __asm__("int $0x30" : : "a" (SYS_SysYield), "c" (0), "d" (0));

                    /*while (!EvtIsSignalled(NULL, sync.event))
                    {
                        ArchProcessorIdle();
                        wprintf(L"X");
                    }*/

                    /*assert(false && "Reached this bit");*/
                }
#endif

                ScEnableSwitch(true);
                SpinRelease(&temp);
            }

            /*EvtFree(NULL, req->event);*/
            /*mal_verify(1);*/
        }

        return true;
    }
    else
        return false;
}

size_t IoReadPhysicalSync(device_t *dev, uint64_t ofs, page_array_t *array, size_t size)
{
    request_dev_t req;
    size_t ret;

    req.header.code = DEV_READ;
    req.params.dev_read.offset = ofs;
    req.params.dev_read.pages = array;
    req.params.dev_read.length = size;

    /*TRACE1("IoReadSync: dev = %p\n", dev);*/
    if (IoRequestSync(dev, &req.header))
        ret = req.params.dev_read.length;
    else
        ret = (size_t) -1;

    return ret;
}

size_t IoReadSync(device_t *dev, uint64_t ofs, void *buf, size_t size)
{
    size_t ret;

    if (dev->flags & DEVICE_IO_DIRECT)
    {
        request_dev_t req;

        req.header.code = DEV_READ_DIRECT;
        req.params.dev_read_direct.offset = ofs;
        req.params.dev_read_direct.buffer = buf;
        req.params.dev_read_direct.length = size;

        /*TRACE1("IoReadSync: dev = %p\n", dev);*/
        if (IoRequestSync(dev, &req.header))
            ret = req.params.dev_read_direct.length;
        else
            ret = (size_t) -1;
    }
    else
    {
        page_array_t *array;

        array = MemCreatePageArray(buf, size);
        if (array == NULL)
            return (size_t) -1;

        ret = IoReadPhysicalSync(dev, ofs, array, size);

        MemDeletePageArray(array);
    }

    return ret;
}
