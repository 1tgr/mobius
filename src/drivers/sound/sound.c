#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/sys.h>
#include <errno.h>
#include "sound.h"

sound_t *sb_init(device_t *dev);
sound_t *wss_init(device_t *dev);
sound_t *solo_init(device_t *dev);

typedef struct sound_dev_t sound_dev_t;
struct sound_dev_t
{
	device_t dev;
	sound_t *snd;
	dword play_end;
	unsigned rate, bits_per_sample, channels;
};

bool sndRequest(device_t* dev, request_t* req)
{
	sound_dev_t *snd = (sound_dev_t*) dev;
	byte *buf;
	dword end;

	switch (req->code)
	{
	case DEV_ISR:
		if (req->params.isr.irq == 0)
		{
			if (snd->play_end && sysUpTime() >= snd->play_end)
			{
				snd->play_end = 0;
				snd->snd->stop_playback(snd->snd, false);
				TRACE0("sndRequest: stop playback\n");
			}
		}
		else
			snd->snd->irq(snd->snd, req->params.isr.irq);

		return true;

	case DEV_REMOVE:
		devRegisterIrq(&snd->dev, 0, false);
		snd->snd->close(snd->snd);
		hndFree(snd);

	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_WRITE:
		end = sysUpTime() + 
			(req->params.write.length * 1000 * 8) / 
			(snd->rate * snd->bits_per_sample);
		req->user_length = req->params.write.length;
		req->params.write.length = 0;
		buf = (byte*) req->params.write.buffer;
		
		while (req->params.write.length < req->user_length)
		{
			snd->snd->write_sample(snd->snd, 
				buf[req->params.write.length], 
				buf[req->params.write.length]);
			req->params.write.length++;
		}

		//snd->play_end = end;
		hndSignal(req->event, true);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t* sndAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	sound_dev_t* snd;
	request_t req;

	snd = hndAlloc(sizeof(sound_dev_t), NULL);
	snd->dev.request = sndRequest;
	snd->dev.driver = drv;
	snd->dev.config = cfg;

	snd->snd = solo_init(&snd->dev);
	if (snd->snd == NULL)
	{
		hndFree(snd);
		return NULL;
	}

	if (!snd->snd->open(snd->snd))
	{
		snd->snd->close(snd->snd);
		hndFree(snd);
		return NULL;
	}

	snd->play_end = 0;
	snd->rate = 22050;
	snd->bits_per_sample = 8;
	snd->channels = 1;

	snd->rate = snd->snd->find_closest_rate(snd->snd, snd->rate);
	if (snd->rate == 0)
	{
		snd->snd->close(snd->snd);
		hndFree(snd);
		return NULL;
	}

	snd->snd->set_fmt(snd->snd, 
		snd->bits_per_sample, 
		snd->channels, 
		snd->rate);
	devRegisterIrq(&snd->dev, 0, true);

	req.code = DEV_WRITE;
	req.params.write.buffer = (void*) 0x1000;
	req.params.write.length = 0x10000;
	devRequestSync(&snd->dev, &req);
	return &snd->dev;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = sndAddDevice;
	return true;
}