#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <errno.h>

/*!
 *  \ingroup	drivers
 *  \defgroup	vgatext	CGA text-mode frame buffer
 *  @{
 */

#define _crtc_base_adr	0x3C0

/* I/O addresses for VGA registers */
#define	VGA_MISC_READ	0x3CC
#define	VGA_GC_INDEX	0x3CE
#define	VGA_GC_DATA	0x3CF
#define	VGA_CRTC_INDEX	0x14 /* relative to 0x3C0 or 0x3A0 */
#define	VGA_CRTC_DATA	0x15 /* relative to 0x3C0 or 0x3A0 */

bool vgatRequest(device_t* dev, request_t* req)
{
	int i;
	void* base;
	dword* params;
	
	i = devFindResource(dev->config, dresMemory, 0);
	if (i == -1)
		base = (void*) 0xb8000;
	else
		base = (void*) dev->config->resources[i].u.memory.base;

	switch (req->code)
	{
	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_WRITE:
		memcpy(base + (dword) req->params.write.pos,
			(void*) req->params.write.buffer,
			req->params.write.length);
		hndSignal(req->event, true);
		return true;

	case DEV_READ:
		memcpy((void*) req->params.read.buffer,
			base + (dword) req->params.read.pos,
			req->params.read.length);
		hndSignal(req->event, true);
		return true;

	case DEV_IOCTL:
		params = (dword*) req->params.ioctl.buffer;
		switch (req->params.ioctl.code)
		{
		case 0:
			/* extra shift because even/odd text mode uses word clocking */

			/* set base address */
			out(_crtc_base_adr + VGA_CRTC_INDEX, 12);
			out(_crtc_base_adr + VGA_CRTC_DATA, params[0] >> 9);
			out(_crtc_base_adr + VGA_CRTC_INDEX, 13);
			out(_crtc_base_adr + VGA_CRTC_DATA, params[0] >> 1);

			/* move cursor */
			out(_crtc_base_adr + VGA_CRTC_INDEX, 14);
			out(_crtc_base_adr + VGA_CRTC_DATA, params[1] >> 9);
			out(_crtc_base_adr + VGA_CRTC_INDEX, 15);
			out(_crtc_base_adr + VGA_CRTC_DATA, params[1] >> 1);

			hndSignal(req->event, true);
			return true;
		}
		
		break;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t* vgatAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	device_t* dev;

	dev = hndAlloc(sizeof(device_t), NULL);
	dev->request = vgatRequest;
	dev->driver = drv;
	dev->config = cfg;
	dev->req_first = dev->req_last = NULL;
	return dev;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = vgatAddDevice;
	return true;
}

//@}