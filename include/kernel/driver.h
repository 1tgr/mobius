/* $Id: driver.h,v 1.9 2002/01/09 01:23:39 pavlovskii Exp $ */
#ifndef __KERNEL_DRIVER_H
#define __KERNEL_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/handle.h>
#include <os/device.h>

#ifndef __DEVICE_T_DEFINED
typedef struct device_t device_t;
#define __DEVICE_T_DEFINED
#endif

typedef struct asyncio_t asyncio_t;
struct asyncio_t
{
	asyncio_t *prev, *next;

	/** Thread that owns this request */
	struct thread_t *owner;
	/** Copy of the original request structure */
	request_t *req;
	/** Size of the original request structure */
	size_t req_size;
	/** Device to which this request applies */
	device_t *dev;
	/** Spare pointer to device-specific information */
	void *extra;
	/** Size, in bytes, of the user buffer */
	size_t length;
	/** Size, in pages, of the user buffer */
	size_t length_pages;
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

typedef struct IDeviceVtbl IDeviceVtbl;
struct IDeviceVtbl
{
	bool (*request)(device_t *dev, request_t *req);
	bool (*isr)(device_t *dev, uint8_t irq);
};

#ifdef __cplusplus
struct __attribute__((com_interface)) device_t
{
	virtual bool request(request_t *req) = 0;
	virtual bool isr(uint8_t irq) = 0;

	driver_t *driver;
	device_config_t *cfg;
	asyncio_t *io_first, *io_last;
};
#else
struct device_t
{
	driver_t *driver;
	device_config_t *cfg;
	asyncio_t *io_first, *io_last;
	const IDeviceVtbl *vtbl;
};
#endif

typedef struct irq_t irq_t;
struct irq_t
{
	irq_t *next;
	device_t *dev;
};

/* Kernel device driver helper routines */
driver_t *	DevInstallNewDriver(const wchar_t *name);
device_t *DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
						   device_config_t *cfg);
void	DevUnloadDriver(driver_t *driver);

bool	DevRegisterIrq(uint8_t irq, device_t *dev);
bool	DevAddDevice(device_t *dev, const wchar_t *name, 
					 device_config_t *cfg);
asyncio_t*	DevQueueRequest(device_t *dev, request_t *req, size_t size,
							void *user_buffer,
							size_t user_buffer_length);
void	DevFinishIo(device_t *dev, asyncio_t *io, status_t result);
uint8_t	DevCfgFindIrq(const device_config_t *cfg, unsigned n, uint8_t dflt);
void *	DevMapBuffer(asyncio_t *io);

void	MemUnmapTemp(void);
#define DevUnmapBuffer	MemUnmapTemp

/* Some extra codes user apps aren't supposed to know about */
#define	IO_FINISH		REQUEST_CODE(0, 0, 'i', 'f')

#ifdef __cplusplus
}
#endif

#endif