#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <sys/error.h>

bool vgatRequest(device_t* dev, request_t* req)
{
	req->result = E_NOTIMPL;
	return false;
}

device_t* vgatAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	device_t* dev;

	dev = hndAlloc(sizeof(device_t), NULL);
	dev->request = vgatRequest;
	dev->driver = drv;
	return dev;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = vgatAddDevice;
	return true;
}