#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <errno.h>

bool devRequest(device_t* dev, request_t* req)
{
	switch (req->code)
	{
	case DEV_REMOVE:
		hndFree(dev);

	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t* devAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	device_t* dev;

	dev = hndAlloc(sizeof(device_t), NULL);
	dev->request = devRequest;
	dev->driver = drv;
	return dev;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = vgatAddDevice;
	return true;
}