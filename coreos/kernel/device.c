/*
 * $History: device.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 7/03/04    Time: 15:27
 * Updated in $/coreos/kernel
 * Split device file system into its own file
 */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/driver.h>

#include <kernel/debug.h>

#include <string.h>
#include <errno.h>
#include <wchar.h>

irq_t *irq_first[16], *irq_last[16];
driver_t *drv_first, *drv_last;


/*!
* \brief    Connects an IRQ line with a device
*
*    The device's \p irq function is called when the specified IRQ occurs
*    \param    irq    The IRQ to connect
*    \param    dev    The device to associate with the IRQ
*    \return     \p true if the IRQ could be registered
*/
bool DevRegisterIrq(uint8_t irq, device_t *dev)
{
    irq_t *i;

    if (irq > 15)
        return false;

    i = malloc(sizeof(irq_t));
    i->dev = dev;
    i->next = NULL;

    if (irq_last[irq] != NULL)
        irq_last[irq]->next = i;
    if (irq_first[irq] == NULL)
        irq_first[irq] = i;
    irq_last[irq] = i;
    return true;
}

/*!
*    \brief    Queues an asynchronous request on a device
*
*    This function does the following:
*    - Sets \p req->result to \p SIOPENDING, indicating to the originator of
*    the request that the request is asynchronous
*    - Creates a copy of the original request structure in kernel space
*    - Ensures that each of the buffer pages are mapped
*    - Locks each of the buffer pages in physical memory
*    - Adds an \p asyncio_t structure to the end of the device's queue
*
*    The physical addresses of each of the pages in the user buffer is stored
*    as an \p addr_t array immediately after the \p asyncio_t structure.
*
*    \param    dev    Device to which the request applies
*    \param    req    Request to be queued
*    \param    size    Size of the \p req structure
*    \param    user_buffer    Buffer in user space to lock
*    \param    user_buffer_length    Length, in bytes, of \p user_buffer
*    \return     A pointer to an \p asyncio_t structure
*/
asyncio_t *DevQueueRequest(device_t *dev, request_t *req, size_t size,
                           page_array_t *pages,
                           size_t user_buffer_length)
{
    asyncio_t *io;
    thread_t *thr;

    if (req != NULL)
    {
        /*
         * It should be possible to pass a NULL request here, as long as the caller set 
         *  req->result.
         */
        req->result = SIOPENDING;
        req->original = NULL;
    }

    io = malloc(sizeof(asyncio_t));
    if (io == NULL)
        return NULL;

    io->pages = MemCopyPageArray(pages);
    if (io->pages == NULL)
    {
        free(io);
        return NULL;
    }

    thr = current();
    io->owner = thr;

    if (false || (addr_t) req < 0x80000000)
    {
        io->req = malloc(size);
        if (io->req == NULL)
        {
            free(io);
            return NULL;
        }

        memcpy(io->req, req, size);
        io->req->original = req;
    }
    else
    {
        io->req = req;
        io->req->original = NULL;
    }

    io->req_size = size;
    io->dev = dev;
    io->length = user_buffer_length;
    io->extra = NULL;

    /* xxx -- SpinAcquire(&thr->sem); */
    io->thread_next = NULL;
    io->thread_prev = thr->aio_last;
    if (thr->aio_last != NULL)
        thr->aio_last->thread_next = io;
    if (thr->aio_first == NULL)
        thr->aio_first = io;
    /* xxx -- SpinRelease(&thr->sem); */

    /* xxx -- SpinAcquire(&dev->sem); */
    LIST_ADD(dev->io, io);
    /* xxx -- SpinRelease(&dev->sem); */
    return io;
}


static void DevFinishIoApc(void *ptr)
{
    asyncio_t *io;

    io = ptr;
    TRACE1("DevFinishIoApc(%p): ", ptr);
    TRACE1("io->req = %p ", io->req);
    TRACE1("io->req->original = %p\n", io->req->original);
    TRACE2("\tresult = %d, size = %u\n", io->req->result, io->req_size);
    if (io->req->original != NULL)
    {
        assert(io->owner->process == current()->process);
        memcpy(io->req->original, io->req, io->req_size);
    }

    if (io->req->original != NULL)
    {
        request_t *req;
        req = io->req;
        io->req = io->req->original;
        free(req);
    }

    IoNotifyCompletion(io->req);

    free(io);
}


/*!
*    \brief    Notifies the kernel that the specified asynchronous request has
*    been completed
*
*    This function does the following:
*    - Unlocks each of the pages in the original user buffer
*    - Removes the \p asyncio_t structure from the device's queue
*    - Updates the \p result field of the original request
*
*    It then queues an APC which:
*    - Copies the copy of the request structure (which may have been updated
*    by the driver between \p DevQueueRequest and \p DevFinishIo) into the
*    original request structure
*    - Calls the originator's I/O completion function, if appropriate
*    - Frees the \p asyncio_t structure
*
*    The APC is guaranteed to execute in the context of the request originator.
*    If \p DevFinishIo is called from the same context as the originator then
*    the APC routine is called directly.
*
*    \param    dev    Device where the asynchronous request is queued
*    \param    io    Asynchronous request structure
*    \param    result      Final result of the operation
*
*/
void DevFinishIo(device_t *dev, asyncio_t *io, status_t result)
{
    thread_t *thr;

    MemDeletePageArray(io->pages);
    io->pages = NULL;

    /* xxx -- SpinAcquire(&dev->sem); */
    LIST_REMOVE(dev->io, io);
    /* xxx -- SpinRelease(&dev->sem); */

    thr = io->owner;
    /* xxx -- SpinAcquire(&thr->sem); */
    if (io->thread_next != NULL)
        io->thread_next->thread_prev = io->thread_prev;
    if (io->thread_prev != NULL)
        io->thread_prev->thread_next = io->thread_next;
    if (thr->aio_first == io)
        thr->aio_first = io->thread_next;
    if (thr->aio_last == io)
        thr->aio_last = io->thread_prev;
    io->thread_next = io->thread_prev = NULL;
    /* xxx -- SpinRelease(&thr->sem); */

    io->req->result = result;

    if (io->owner->process == current()->process)
    {
        TRACE0("Not queueing APC\n");
        DevFinishIoApc(io);
    }
    else
    {
        TRACE0("Queueing APC\n");
        ThrQueueKernelApc(io->owner, DevFinishIoApc, io);
    }
}


/*!
 *  \brief  Cancels an I/O request
 *
 *  I/O requests can be cancelled if the originator is no longer interested in
 *  receiving the results of the operation. For example, when a thread exits,
 *  \p ThrExitThread calls \p DevCancelIo for each pending I/O request.
 *
 *  This function calls the device's \p cancelio routine if available; 
 *  otherwise, \p DevFinishIo is called with \p ECANCELLED status.
 *
 *  \param  io  Asynchronous request to cancel
 *  \return \p true if the request could be cancelled, \p false otherwise
 */
bool DevCancelIo(asyncio_t *io)
{
    device_t *dev;

    dev = io->dev;
    if (dev->vtbl->cancelio == NULL)
    {
        DevFinishIo(dev, io, ECANCELLED);
        return true;
    }
    else
        return dev->vtbl->cancelio(dev, io);
}


/*!
*    \brief    Finds the n'th IRQ resource
*
*    \param    cfg    Pointer to the device's configuration list
*    \param    n    Index of the IRQ to find; use 0 for the first IRQ, 1 for
*      the second, etc.
*    \param    dflt    Default value to return if the specified IRQ could not
*      be found
*    \return     The requested IRQ
*/
uint8_t DevCfgFindIrq(const dev_config_t *cfg, unsigned n, uint8_t dflt)
{
    unsigned i, j;
    dev_resource_t *res;

    res = cfg->resources;
    for (i = j = 0; i < cfg->num_resources; i++)
        if (res[i].cls == resIrq)
        {
            if (j == n)
                return res[i].u.irq;

            j++;
        }

        return dflt;
}


/*!
*    \brief    Finds the n'th memory resource
*
*    \param    cfg    Pointer to the device's configuration list
*    \param    n    Index of the memory range to find; use 0 for the first 
*          memory range, 1 for the second, etc.
*    \param    dflt    Default value to return if the specified memory range 
*          could not be found
*    \return     The requested memory base address
*/
dev_resource_t *DevCfgFindMemory(const dev_config_t *cfg, unsigned n)
{
    unsigned i, j;
    dev_resource_t *res;

    res = cfg->resources;
    for (i = j = 0; i < cfg->num_resources; i++)
        if (res[i].cls == resMemory)
        {
            if (j == n)
                return res + i;

            j++;
        }

        return NULL;
}


extern driver_t rd_driver, port_driver, vfs_driver, tarfs_driver, devfs_driver;
#ifdef WIN32
extern driver_t win32fs_driver;
#endif


static status_t DevDoLoadDriver(const wchar_t *name, driver_t **driver)
{
    if (_wcsicmp(name, L"ramfs") == 0)
        *driver = &rd_driver;
    else if (_wcsicmp(name, L"portfs") == 0)
        *driver = &port_driver;
    else if (_wcsicmp(name, L"devfs") == 0)
        *driver = &devfs_driver;
    else if (_wcsicmp(name, L"vfs") == 0)
        *driver = &vfs_driver;
    else if (_wcsicmp(name, L"tarfs") == 0)
        *driver = &tarfs_driver;
#ifdef WIN32
    else if (_wcsicmp(name, L"win32fs") == 0)
        *driver = &win32fs_driver;
#endif
    else
    {
        wchar_t temp[50];
        module_t *mod;
        driver_t *drv;
        bool (*DrvInit)(driver_t *drv);

        swprintf(temp, L"%s.drv", name);

        mod = PeLoad(&proc_idle, temp, 0);
        if (mod == NULL)
        {
            *driver = NULL;
            return errno;
        }

        FOREACH(drv, drv)
            if (drv->mod == mod)
            {
                *driver = drv;
                return 0;
            }

        drv = malloc(sizeof(driver_t));
        if (drv == NULL)
        {
            PeUnload(&proc_idle, mod);
            *driver = NULL;
            return errno;
        }

        drv->mod = mod;
        drv->add_device = NULL;
        drv->mount_fs = NULL;

        DrvInit = (void*) mod->entry;
        if (DrvInit == NULL || !DrvInit(drv))
        {
            PeUnload(&proc_idle, mod);
            free(drv);
            *driver = NULL;
            return errno;
        }

        LIST_ADD(drv, drv);

        *driver = drv;
    }

    return 0;
}


typedef struct load_request_t load_request_t;
struct load_request_t
{
    load_request_t *prev, *next;
    unsigned action;
    status_t status;
    event_t *evt_done;

    union
    {
        struct
        {
            wchar_t *name;
            driver_t *driver;
        } load_driver;

        struct
        {
            driver_t *driver;
            wchar_t *name;
            dev_config_t *config;
        } add_device;
    } u;
};

#define LOAD_ACTION_LOAD_DRIVER    0
#define LOAD_ACTION_ADD_DEVICE    1

static load_request_t *load_first, *load_last;
static semaphore_t *mux_load;
static semaphore_t *sem_load;

static void DevLoaderThread(void)
{
    load_request_t *load;
    event_t *evt_done;

    while (true)
    {
        wprintf(L"DevLoaderThread: waiting for request\n");
        SemDown(sem_load);
        wprintf(L"DevLoaderThread: got request\n");

        SemDown(mux_load);
        load = load_first;
        LIST_REMOVE(load, load);
        SemUp(mux_load);

        evt_done = (event_t*) HndCopy(&load->evt_done->hdr);

        switch (load->action)
        {
        case LOAD_ACTION_LOAD_DRIVER:
            load->status = DevDoLoadDriver(load->u.load_driver.name, 
                &load->u.load_driver.driver);
            break;

        case LOAD_ACTION_ADD_DEVICE:
            load->u.add_device.driver->add_device(load->u.add_device.driver,
                load->u.add_device.name,
                load->u.add_device.config);
            load->status = 0;
            break;

        default:
            wprintf(L"DevLoaderThread: invalid action: %u\n", load->action);
            assert(load->action == LOAD_ACTION_LOAD_DRIVER ||
                load->action == LOAD_ACTION_ADD_DEVICE);
            break;
        }

        EvtSignal(evt_done, true);
        HndClose(&evt_done->hdr);
    }
}


static void DevCallLoader(load_request_t *load)
{
    event_t *evt_done;

    evt_done = EvtCreate(false);
    load->evt_done = evt_done;

    SemDown(mux_load);
    LIST_ADD(load, load);
    SemUp(mux_load);

    wprintf(L"DevCallLoader: submitting request\n");
    SemUp(sem_load);
    ThrWaitHandle(current(), &evt_done->hdr);
    KeYield();

    HndClose(&evt_done->hdr);
}


driver_t *DevInstallNewDriver(const wchar_t *name)
{
    load_request_t load = { 0 };

    load.action = LOAD_ACTION_LOAD_DRIVER;
    load.u.load_driver.name = _wcsdup(name);
    DevCallLoader(&load);
    free(load.u.load_driver.name);

    if (load.u.load_driver.driver == NULL)
    {
        errno = load.status;
        return NULL;
    }
    else
        return load.u.load_driver.driver;
}

/*!
*    \brief    Unloads a driver loaded by \p DevInstallNewDriver
*
*    \param    driver      Driver to unload
*/
void DevUnloadDriver(driver_t *driver)
{
    if (driver->mod != NULL)
        PeUnload(&proc_idle, driver->mod);
    /*free(driver);*/
}


/*!
 *  \brief  Installs a new device
 *
 *  This function will ask the specified driver for the given device;
 *  the driver is loaded if not already present.
 *
 *  \param  driver      Name of the device driver
 *  \param  name        Name of the new device
 *  \param  cfg         Configuration list to assign to the new device
 *  \param  profile_key Path to the device's profile key
 *  \return \p true if the driver could be loaded (or was already present),
 *      \p false otherwise
 */
bool DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
                      dev_config_t *cfg)
{
    driver_t *drv;

    drv = DevInstallNewDriver(driver);
    if (drv == NULL)
    {
        wprintf(L"%s.%s: driver not loaded\n", driver, name);
        return false;
    }

    if (drv->add_device != NULL)
    {
        load_request_t load = { 0 };
        load.action = LOAD_ACTION_ADD_DEVICE;
        load.u.add_device.driver = drv;
        load.u.add_device.name = _wcsdup(name);
        load.u.add_device.config = cfg;
        DevCallLoader(&load);
        free(load.u.add_device.name);
    }

    return true;
}


bool DevInit(void)
{
    mux_load = SemCreate(1);
    sem_load = SemCreate(0);
    ThrCreateThread(&proc_idle, true, DevLoaderThread, false, NULL, 16, L"DevLoaderThread");
    return true;
}
