/* $Id: sound.c,v 1.4 2002/09/01 16:24:40 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <kernel/proc.h>
#include <errno.h>
#include <stdio.h>
#include "sound.h"

sound_t *SbInit(device_t *dev, device_config_t *cfg);
sound_t *EsInit(device_t *dev, device_config_t *cfg);

typedef struct sound_dev_t sound_dev_t;
struct sound_dev_t
{
    device_t dev;
    sound_t *sound;
    device_config_t *cfg;
};

static void SndStartIo(sound_dev_t *snd)
{
    asyncio_t *io, *next;
    status_t ret;

    io = snd->dev.io_first;
    while (io != NULL)
    {
        assert(io->req->code == DEV_READ || io->req->code == DEV_WRITE);
        next = io->next;
        ret = snd->sound->vtbl->SndStartBuffer(snd->sound, 
            io->pages, 
            io->length,
            io->req->code == DEV_READ);

        if (ret != 0)
        {
            wprintf(L"SndStartIo: failed (%d)\n", ret);
            DevFinishIo(&snd->dev, io, ret);
            io = next;
        }
        else
            break;
    }
}

void SndFinishedBuffer(device_t *dev)
{
    sound_dev_t *snd;
    asyncio_t *io;

    snd = (sound_dev_t*) dev;
    io = snd->dev.io_first;
    assert(io != NULL);

    DevFinishIo(&snd->dev, io, 0);
    if (snd->dev.io_first != NULL)
        SndStartIo(snd);
}

static bool SndRequest(device_t *dev, request_t *req)
{
    sound_dev_t *snd;
    request_dev_t *req_dev;
    bool was_idle;

    snd = (sound_dev_t*) dev;
    req_dev = (request_dev_t*) req;
    switch (req->code)
    {
    case DEV_REMOVE:
        snd->sound->vtbl->SndClose(snd->sound);
        free(snd->cfg);
        free(snd);
        return true;

    case DEV_WRITE:
        was_idle = dev->io_first == NULL;
        DevQueueRequest(dev, req, 
            sizeof(request_dev_t),
            req_dev->params.dev_write.pages,
            req_dev->params.dev_write.length);
        if (was_idle)
            SndStartIo(snd);
        return true;
    }

    req->result = ENOTIMPL;
    return false;
}

static bool SndIsr(device_t *dev, uint8_t irq)
{
    sound_dev_t *snd;
    snd = (sound_dev_t*) dev;
    return snd->sound->vtbl->SndIsr(snd->sound, irq);
}

static const device_vtbl_t sound_vtbl = 
{
    SndRequest,
    SndIsr,
    NULL,
};

typedef sound_t *(*INITFUNC)(device_t*, device_config_t*);

void SndAddDevice(driver_t *drv, const wchar_t *name, device_config_t *cfg)
{
    sound_dev_t *dev;
    module_t *mod;
    INITFUNC Init;
    wchar_t *file;

    dev = malloc(sizeof(sound_dev_t));
    if (dev == NULL)
        return;

    memset(dev, 0, sizeof(sound_dev_t));
    dev->dev.vtbl = &sound_vtbl;

    if (cfg != NULL && cfg->vendor_id == 0x1274 && cfg->device_id == 0x1371)
        file = L"/System/Boot/es1371.kll";
    else
    {
        file = L"/System/Boot/sb.kll";
        dev->cfg = malloc(sizeof(device_config_t));
        if (dev->cfg == NULL)
        {
            free(dev);
            return;
        }

        memset(dev->cfg, 0, sizeof(device_config_t));
        dev->cfg->device_class = 0x0401;
        cfg = dev->cfg;
    }

    mod = PeLoad(NULL, file, 0);
    if (mod == NULL)
    {
        free(dev);
        return;
    }

    //dev->sound = EsInit(&dev->dev, cfg);
    Init = (INITFUNC) mod->entry;
    wprintf(L"sound: entry point at %p\n", Init);
    dev->sound = Init(&dev->dev, cfg);
    if (dev->sound == NULL)
    {
        free(dev);
        return;
    }

    if (!dev->sound->vtbl->SndOpen(dev->sound))
    {
        dev->sound->vtbl->SndClose(dev->sound);
        free(dev);
        return;
    }

    DevAddDevice(&dev->dev, name, cfg);
}

bool DrvInit(driver_t *drv)
{
    drv->add_device = SndAddDevice;
    return true;
}
