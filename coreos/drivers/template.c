/* $Id: template.c,v 1.1.1.1 2002/12/21 09:48:38 pavlovskii Exp $ */

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
