/* $Id: sound.c,v 1.1.1.1 2002/12/21 09:49:01 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <kernel/proc.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "sound.h"

sound_t *SbInit(device_t *dev, dev_config_t *cfg);
sound_t *EsInit(device_t *dev, dev_config_t *cfg);

typedef struct sound_dev_t sound_dev_t;
struct sound_dev_t
{
    device_t *device;
    sound_t *sound;
    dev_config_t *cfg;
};

static void SndStartIo(sound_dev_t *snd)
{
    asyncio_t *io, *next;
    status_t ret;

    io = snd->device->io_first;
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
            DevFinishIo(snd->device, io, ret);
            io = next;
        }
        else
            break;
    }
}

void SndFinishedBuffer(device_t *device)
{
    sound_dev_t *snd;
    asyncio_t *io;

    snd = device->cookie;
    io = snd->device->io_first;
    assert(io != NULL);

    DevFinishIo(snd->device, io, 0);
    if (snd->device->io_first != NULL)
        SndStartIo(snd);
}


static void SndDeleteDevice(device_t *device)
{
	sound_dev_t *snd;
	snd = device->cookie;
	snd->sound->vtbl->SndClose(snd->sound);
    free(snd->cfg);
    free(snd);
}


static status_t SndRequest(device_t *device, request_t *req)
{
    sound_dev_t *snd;
    request_dev_t *req_dev;
    bool was_idle;

    snd = device->cookie;
    req_dev = (request_dev_t*) req;
    switch (req->code)
    {
    case DEV_WRITE:
        was_idle = device->io_first == NULL;
        DevQueueRequest(device, req, 
            sizeof(request_dev_t),
            req_dev->params.dev_write.pages,
            req_dev->params.dev_write.length);
        if (was_idle)
            SndStartIo(snd);
        return SIOPENDING;
    }

    return ENOTIMPL;
}

static bool SndIsr(device_t *device, uint8_t irq)
{
    sound_dev_t *snd;
    snd = device->cookie;
    return snd->sound->vtbl->SndIsr(snd->sound, irq);
}

static const device_vtbl_t sound_vtbl = 
{
	SndDeleteDevice,
    SndRequest,
    SndIsr,
    NULL,
};

typedef sound_t *(*INITFUNC)(device_t*, dev_config_t*);

void SndAddDevice(driver_t *drv, const wchar_t *name, dev_config_t *cfg)
{
    sound_dev_t *dev;
    module_t *mod;
    INITFUNC Init;
    wchar_t *file;

    dev = malloc(sizeof(sound_dev_t));
    if (dev == NULL)
        return;

    memset(dev, 0, sizeof(sound_dev_t));

    if (cfg->bus_type == DEV_BUS_PCI &&
		((pci_businfo_t*) cfg->businfo)->vendor_id == 0x1274 && 
		((pci_businfo_t*) cfg->businfo)->device_id == 0x1371)
        file = L"/System/Boot/es1371.kll";
    else
    {
        file = L"/System/Boot/sb.kll";

		if (cfg->device_class == 0)
			cfg->device_class = 0x0401;
    }

	mod = PeLoad(NULL, file, 0);
    if (mod == NULL)
    {
        free(dev);
        return;
    }

	dev->device = DevAddDevice(drv, &sound_vtbl, 0, name, cfg, dev);

    Init = (INITFUNC) mod->entry;
    wprintf(L"sound: entry point at %p\n", Init);
    dev->sound = Init(dev->device, cfg);
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
}

bool DrvInit(driver_t *drv)
{
    drv->add_device = SndAddDevice;
    return true;
}
