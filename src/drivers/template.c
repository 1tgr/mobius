/* $Id: template.c,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>

#include <errno.h>

bool DevRequest(device_t* dev, request_t* req)
{
	switch (req->code)
	{
	case DEV_REMOVE:
		free(dev);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t* DevAddDevice(driver_t *drv, const wchar_t *name, device_config_t *cfg)
{
	device_t* dev;

	dev = malloc(sizeof(device_t));
	dev->request = DevRequest;
	dev->driver = drv;
	return dev;
}

bool DrvInit(driver_t *drv)
{
	drv->add_device = DevAddDevice;
	return true;
}
