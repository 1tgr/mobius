/* $Id: driver.h,v 1.3 2002/01/02 21:15:22 pavlovskii Exp $ */
#ifndef __KERNEL_DRIVER_H
#define __KERNEL_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/handle.h>
#include <os/device.h>

typedef struct device_t device_t;

typedef struct asyncio_t asyncio_t;
struct asyncio_t
{
	asyncio_t *prev, *next;

	/** Thread that owns this request */
	struct thread_t *owner;
	/** Copy of the original request structure */
	request_t *req;
	/** Device to which this request applies */
	device_t *dev;
	/** Size, in bytes, of the user buffer */
	size_t length;
	/** Offset of the start of the user buffer from the previous page boundary */
	unsigned mod_buffer_start;
	/** Physical page addresses follow this structure */
	/*addr_t buffer[];*/
};

typedef struct device_resource_t device_resource_t;
struct device_resource_t
{
	enum
	{
		resMemory,
		resIo,
		resIrq
	} type;

	union
	{
		struct
		{
			addr_t start;
			size_t bytes;
		} memory;

		struct
		{
			uint16_t start;
			unsigned count;
		} io;

		uint8_t irq;
	} u;
};

typedef struct device_config_t device_config_t;
struct device_config_t
{
	device_t *parent;
	unsigned resource_count;
};

#define DEV_CFG_RESOURCES(cfg)	((device_resource_t*) ((cfg) + 1))

typedef struct driver_t driver_t;
struct driver_t
{
	struct module_t *mod;
	driver_t *prev, *next;
	device_t * (*add_device)(driver_t *driver, const wchar_t *name, 
		device_config_t *config);
	device_t * (*mount_fs)(driver_t *driver, const wchar_t *path, 
		device_t *dev);
};

struct device_t
{
	bool (*request)(device_t *dev, request_t *req);
	bool (*isr)(device_t *dev, uint8_t irq);
	void (*finishio)(device_t *dev, asyncio_t *io);
	driver_t *driver;
	device_config_t *cfg;
	asyncio_t *io_first, *io_last;
};

typedef struct irq_t irq_t;
struct irq_t
{
	irq_t *next;
	device_t *dev;
};

bool	DevRequest(device_t *dev, request_t *req);
bool	DevRequestSync(device_t *dev, request_t *req);
driver_t *	DevInstallNewDriver(const wchar_t *name);
device_t *DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
						   device_config_t *cfg);
void	DevUnloadDriver(driver_t *driver);
device_t *	DevOpen(const wchar_t *name);
void	DevClose(device_t *dev);
size_t	DevRead(device_t *dev, uint64_t ofs, void *buf, size_t size);

bool	DevRegisterIrq(uint8_t irq, device_t *dev);
bool	DevAddDevice(device_t *dev, const wchar_t *name, 
					 device_config_t *cfg);
asyncio_t*	DevQueueRequest(device_t *dev, request_t *req, size_t size,
							void *user_buffer,
							size_t user_buffer_length);
void	DevFinishIo(device_t *dev, asyncio_t *io);
uint8_t	DevCfgFindIrq(const device_config_t *cfg, unsigned n, uint8_t dflt);

#ifdef __cplusplus
}
#endif

#endif