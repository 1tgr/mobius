#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/handle.h>
#include <kernel/sys.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/config.h>
#include <kernel/ramdisk.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>

extern process_t proc_idle;

typedef struct devlink_t devlink_t;
struct devlink_t
{
	devlink_t *prev, *next;
	device_config_t* cfg;
	device_t* dev;
};

typedef struct devlookup_t devlookup_t;
struct devlookup_t
{
	devlookup_t *next;
	wchar_t name_mask[20];
	word vendor_id;
	word device_id;
	dword subsystem;
	wchar_t to_filename[MAX_PATH];
};

devlink_t *dev_first, *dev_last;

static devlookup_t *dlu_first, *dlu_scan;
static int line_count;

typedef struct devfile_t devfile_t;
struct devfile_t
{
	file_t file;
	device_t* dev;
};

bool devFsRequest(device_t* dev, request_t* req);

device_t vfs_devices =
{
	NULL,
	devFsRequest,
	NULL, NULL,
	NULL
};

bool devFsRequest(device_t* dev, request_t* req)
{
	devfile_t* fd;
	const wchar_t* name;

	assert(dev == &vfs_devices);
	switch (req->code)
	{
	case FS_OPEN:
		assert(req->params.fs_open.name[0] == '/');

		fd = hndAlloc(sizeof(devfile_t), NULL);
		assert(fd != NULL);
		fd->file.fsd = dev;
		fd->file.pos = 0;

		name = req->params.fs_open.name + 1;
		fd->dev = devOpen(name, NULL);
		//wprintf(L"devFsRequest: FS_OPEN(%s), dev = %p\n", name, dev);
		
		if (fd->dev == NULL)
		{
			req->params.fs_open.fd = NULL;
			hndFree(fd);
			return false;
		}

		req->params.fs_open.fd = &fd->file;
		hndSignal(req->event, true);
		return true;

	case FS_CLOSE:
		fd = (devfile_t*) req->params.fs_close.fd;
		assert(fd != NULL);
		if (!devClose(fd->dev))
		{
			req->result = EINVALID;
			return false;
		}
		
		hndFree(fd);
		hndSignal(req->event, true);
		return true;

	case FS_READ:
		fd = (devfile_t*) req->params.fs_read.fd;
		assert(fd != NULL);

		//wprintf(L"devFsRequest: FS_READ, dev = %p\n", fd->dev);
		req->result = devReadSync(fd->dev, 
			fd->file.pos,
			(void*) req->params.fs_read.buffer,
			&req->params.fs_read.length);
		fd->file.pos += req->params.fs_read.length;
		hndSignal(req->event, true);
		return req->result == 0;

	case FS_WRITE:
		fd = (devfile_t*) req->params.fs_write.fd;
		assert(fd != NULL);

		//wprintf(L"devFsRequest: FS_WRITE, dev = %p\n", fd->dev);
		req->result = devWriteSync(fd->dev, 
			fd->file.pos,
			(const void*) req->params.fs_write.buffer,
			&req->params.fs_write.length);
		fd->file.pos += req->params.fs_write.length;
		hndSignal(req->event, true);
		return req->result == 0;
	}

	req->result = ENOTIMPL;
	return false;
}

void devPrintConfigLine(hashelem_t* elem)
{
	if (elem->data == NULL)
		return;

	if (wcsicmp(elem->str, (const wchar_t*) L"driver") == 0)
		wcscpy(dlu_scan->to_filename, elem->data);
	else if (wcsicmp(elem->str, (const wchar_t*) L"mask") == 0)
		wcscpy(dlu_scan->name_mask, elem->data);
	else if (wcsicmp(elem->str, (const wchar_t*) L"vendor") == 0)
		dlu_scan->vendor_id = wcstol(elem->data, NULL, 16);
	else if (wcsicmp(elem->str, (const wchar_t*) L"device") == 0)
		dlu_scan->device_id = wcstol(elem->data, NULL, 16);
	else if (wcsicmp(elem->str, (const wchar_t*) L"subsystem") == 0)
		dlu_scan->subsystem = wcstol(elem->data, NULL, 16);

	line_count++;
}

void devInit()
{
	char *ptr, *base;
	hashtable_t* table;
	size_t length;
	devlookup_t *dlu, *dlu_last;

	base = ptr = ramOpen(L"drivers.cfg");
	if (!ptr)
		return;

	length = ramFileLength(L"drivers.cfg");
	dlu_last = NULL;
	while (ptr < base + length)
	{
		table = cfgParseStrLine((const char**) &ptr);

		dlu = malloc(sizeof(devlookup_t));
		
		dlu->to_filename[0] = 0;
		wcscpy(dlu->name_mask, L"*");
		dlu->vendor_id = dlu->device_id = 0xffff;
		dlu->subsystem = 0xffffffff;
		
		line_count = 0;
		dlu_scan = dlu;
		hashList(table, devPrintConfigLine);
		dlu_scan = NULL;

		if (line_count)
		{
			//wprintf(L"%s/%04x/%04x/%08x => %s\n", 
				//dlu->name_mask, dlu->vendor_id, dlu->device_id, dlu->subsystem, dlu->to_filename);
			dlu->next = NULL;
			if (dlu_first == NULL)
				dlu_first = dlu;
			if (dlu_last)
				dlu_last->next = dlu;
			dlu_last = dlu;
		}
		else
			free(dlu);

		cfgDeleteTable(table);
	}
}

void devCleanup()
{
	devlink_t *link, *next;

	for (link = dev_first; link; link = next)
	{
		next = link->next;
		devRemove(link->dev);

		if (link->cfg)
		{
			hndFree(link->cfg->resources);
			hndFree(link->cfg);
		}

		free(link);
	}
}

driver_t* devInstallNewDevice(const wchar_t* name, device_config_t* cfg)
{
	wchar_t temp[16];
	driver_t* drv;
	module_t* mod;
	bool (STDCALL *drvInit)(driver_t*);
	devlookup_t* dlu;

	/*if (cfg->vendor_id != 0xffff)
		wprintf(L"%s: %04x/%04x/%08x\n", name, 
			cfg->vendor_id, cfg->device_id, cfg->subsystem);*/

	temp[0] = 0;
	for (dlu = dlu_first; dlu; dlu = dlu->next)
	{
		//wprintf(L"%s = %s? %d\n", dev_default[i].name_mask, name,
			//!match(dev_default[i].name_mask, name));

		if ((dlu->vendor_id == 0xffff && match(dlu->name_mask, name) == 0) ||
			(cfg != NULL &&
			 (cfg->vendor_id != 0xffff) &&
			 (cfg->device_id == dlu->device_id) &&
			 (cfg->vendor_id == dlu->vendor_id) &&
			 (cfg->subsystem == dlu->subsystem)))
		{
			wcscpy(temp, dlu->to_filename);

			if (cfg)
				wprintf(L"Matched %s/%04x/%04x/%04x to %s\n",
					name, cfg->vendor_id, cfg->device_id, cfg->subsystem, temp);
			else
				wprintf(L"Matched %s to %s\n", name, temp);
			break;
		}
	}

	if (temp[0] == 0)
	{
		if (cfg)
			wprintf(L"No match for %04x/%04x/%04x\n",
				cfg->vendor_id, cfg->device_id, cfg->subsystem, temp);
		else
			wprintf(L"No match for %s\n", name);

		return NULL;
	}

	mod = peLoad(&proc_idle, temp, 0);
	if (!mod)
		return NULL;
	
	drv = hndAlloc(sizeof(driver_t), NULL);
	assert(drv != NULL);
	memset(drv, 0, sizeof(driver_t));
	drv->mod = mod;
	drvInit = (void*) mod->entry;
	if (!drvInit || !drvInit(drv))
	{
		wprintf(L"drvInit failed\n");
		return NULL;
	}

	return drv;
}

//! Retrieves the device_t structure for the specified device
/*!
 *	The device will receive a DEV_OPEN request before function returns.
 *	\param	name	The name of the device to open
 *	\param	params	Any additional parameters to pass to this instance of the 
 *		device
 *	\return	A pointer to the original device_t structure passed when the driver
 *		called devRegister().
 */
device_t* devOpen(const wchar_t* name, const wchar_t* params)
{
	devlink_t* link;
	request_t req;

	//wprintf(L"devOpen(%p, %p)\n", name, params);
	link = sysOpen(name);
	if (link && link->dev)
	{
		req.code = DEV_OPEN;
		req.params.open.params = params;
		if (devRequestSync(link->dev, &req) == 0)
			return link->dev;
		else
			return NULL;
	}
	else
		return NULL;
}

//! Closes a device opened by devOpen()
/*!
 *	This function issues the DEV_CLOSE request to the device.
 *	\param	dev	The device to close
 *	\return	true if the close request completed successfully.
 */
bool devClose(device_t* dev)
{
	request_t req;

	if (!dev)
		return false;

	req.code = DEV_CLOSE;
	return devRequestSync(dev, &req) == 0;
}

//! Removes a device from the device manager
/*!
 *	This function issues the DEV_REMOVE request to the device.
 *	The driver itself is responsible for freeing all the resources 
 *		allocated for the device, including the device_t structure
 *		itself.
 *	\param	dev	The device to remove
 *	\return	true if the remove request completed successfully.
 */
bool devRemove(device_t* dev)
{
	request_t req;

	if (!dev)
		return false;

	req.code = DEV_REMOVE;
	return devRequestSync(dev, &req) == 0;
}

//! Issues a request to a device
/*!
 *	The device's request function is called and it is passed the req structure.
 *	req is not copied, so if the request is queued by the device, the memory
 *		must remain valid until the request completes.
 *	If the function succeeds, req->event will contain a handle to an event
 *		which will be signalled when the request completes. The driver is free
 *		to signal this before it the function returns, or it could be signalled
 *		when the driver determines that its hardware has completed the request.
 *	\param	dev	The device to which the request is to be issued
 *	\param	req	The request to pass to the driver
 *	\return	The error code returned by the driver, or zero if no error 
 *		occurred. The return value is equal to the result member of the 
 *		request structure provided.
 */
status_t devRequest(device_t* dev, request_t* req)
{
	if (dev && dev->request)
	{
		//wprintf(L"devRequest: %c%c\n", req->code / 256, req->code % 256);
		req->result = 0;
		req->event = hndAlloc(0, NULL);
		//req->cks = 0xdeadbeef;
		assert(req->event != NULL);
		
		req->next = NULL;
		req->queued = 0;
		
		if (dev->request(dev, req))
		{
			//assert(req->cks == 0xdeadbeef);
			return req->result;
		}
		else
		{
			//wprintf(L"devRequest failed (%p): %x\n", dev->request, req->result);
			hndFree(req->event);
			req->event = NULL;
			//assert(req->cks == 0xdeadbeef);
			if (req->result == 0)
				req->result = EINVALID;
			return req->result;
		}
	}
	else
	{
		req->result = EINVALID;
		return req->result;
	}
}

//! Issues a synchronous request to a device
/*!
 *	This function is identical to devRequest(), except that it always waits
 *		for the request to complete before returning.
 *	\param	dev	The device to which the request is to be issued
 *	\param	req	The request to pass to the driver
 *	\return	The error code returned by the driver, or zero if no error 
 *		occurred. The return value is equal to the result member of the 
 *		request structure provided.
 */
status_t devRequestSync(device_t* dev, request_t* req)
{
	//bool wasntSignalled = false;

	if (devRequest(dev, req) == 0)
	{
		if (req->result == 0)
		{
			assert(req->event != NULL);

			/*if (!hndIsSignalled(req->event))
			{
				wprintf(L"devRequestSync(%p): event not signalled...",
					dev->request);
				wasntSignalled = true;
			}*/

			while (!hndIsSignalled(req->event))
				asm("sti");
			
			//if (wasntSignalled)
				//wprintf(L"done\n");

			hndFree(req->event);
			return 0;
		}
		else
			return req->result;
	}
	else
		return req->result;
}

//! Connects an IRQ line to a kernel device
/*!
 *	Currently, each IRQ can have only one associated device.
 *	To de-register an IRQ, use a NULL device pointer.
 *	When the IRQ is triggered, the device's request function will be
 *		called with the DEV_ISR code.
 *	\param	dev	The device to which the specified IRQ is to be connected
 *	\param	irq	The IRQ to connect.
 *	\param	install	true to install the IRQ in the device manager's IRQ chain, 
 *		false to remove it.
 *	\return	true if the operation completed successfully.
 */
bool devRegisterIrq(device_t* dev, byte irq, bool install)
{
	irq_t *pirq, *next;
	dword crit;

	if (irq >= countof(irq_last))
		return false;

	crit = critb();
	if (install)
	{
		pirq = malloc(sizeof(irq_t));
		pirq->dev = dev;
		pirq->next = NULL;
		pirq->prev = irq_last[irq];
		if (irq_last[irq])
			irq_last[irq]->next = pirq;
		if (irq_first[irq] == NULL)
			irq_first[irq] = pirq;
		irq_last[irq] = pirq;
	}
	else
	{
		pirq = irq_first[irq];
		while (pirq)
		{
			next = pirq->next;

			if (pirq->dev == dev)
			{
				if (pirq->prev)
					pirq->prev->next = pirq->next;
				if (pirq->next)
					pirq->next->prev = pirq->prev;
				free(pirq);
			}

			pirq = next;
		}
	}
	
	crite(crit);
	return true;
}

//! Registers a device with the device manager
/*!
 *  The device manager keeps an internal list of devices. This function
 *	adds entries to that list.
 *  This function is used under two circumstances:
 *  \li	Case 1: to add a device for which a device_t structure has already been 
 *	allocated,
 *  \li	Case 2: to install a device from an unknown driver given a set of 
 *	configuration information.
 *
 *  To use case 1, allocate a device_t structure and (optionally) a 
 *	device_config_t structure, and pass them to devRegister() (along with 
 *	a name) to create an entry in the device manager's namespace.
 *	This is generally used to bypass the PnP and explicitly create a
 *	device link.
 *
 *  To use case 2, provide a name and a valid device_config_t structure,
 *	and pass them to devRegister(). Use NULL for dev. The device manager
 *	will use the name and configuration information to match a driver to
 *	this device. This method is generally used by devices which enumerate 
 *	one or more other devices; for example, the PCI bus driver needs to
 *	add several child devices, for which it knows the configuration but
 *	doesn't know the name of the driver to use.
 *
 *  If you adhere to the PnP standard your driver's add_device routine will get
 *	called automatically when the relevant bus driver calls devRegister().
 *	You should only need to use devRegister() if you add further devices
 *	not already added by another device.
 *
 *  \param  name    The name (ID) of the device to add
 *  \param  dev	    The device object to add. If this is NULL, the device 
 *	manager will attempt to load a driver based on the device 
 *	configuration provided (which must not be NULL).
 *  \param  cfg	    The configuration of the device to be added. This may only
 *	be NULL if dev is not NULL.
 *
 *  \return true if the device could be added (and if loading the driver 
 *	succeeded, if necessary).
 */
bool devRegister(const wchar_t* name, device_t* dev, device_config_t* cfg)
{
	devlink_t* link;
	driver_t* drv;

	link = sysOpen(name);
	if (link == NULL)
	{
		link = malloc(sizeof(devlink_t));
		link->cfg = NULL;
		link->dev = NULL;
		link->prev = dev_last;
		link->next = NULL;

		if (dev_last != NULL)
			dev_last->next = link;
		if (dev_first == NULL)
			dev_first = link;
		dev_last = link;

		sysMount(name, link);
	}

	if (cfg)
		link->cfg = cfg;

	if (dev == NULL &&
		link->dev == NULL)
	{
		_cputws_check(L" \n" CHECK_L2);
		_cputws_check(name);
		_cputws_check(L"\r\t");

		drv = devInstallNewDevice(name, cfg);
		if (drv == NULL ||
			!drv->add_device)
		{
			link->dev = NULL;
			return false;
		}

		dev = drv->add_device(drv, name, cfg);
		hndFree(drv);
		link->dev = dev;
	}
	else if (dev)
		link->dev = dev;

	if (dev)
	{
		dev->req_first = dev->req_last = NULL;
		dev->config = cfg;
	}
		
	return true;
}

//! Adds a request to a device's request queue
void devStartRequest(device_t* dev, request_t* req)
{
	req->next = NULL;
	if (dev->req_last)
		dev->req_last->next = req;
	dev->req_last = req;
	if (dev->req_first == NULL)
		dev->req_first = req;

	req->user_length = req->params.buffered.length;
	req->queued++;
}

//! Marks a request as finished and removes it from the device's queue
void devFinishRequest(device_t* dev, request_t* req)
{
	request_t *next;

	hndSignal(req->event, true);

	if (dev->req_first == req)
		dev->req_first = req->next;
	if (dev->req_last == req)
	{
		dev->req_last = NULL;

		for (next = dev->req_first; next; next = next->next)
			if (next->next == req)
			{
				dev->req_last = next;
				break;
			}
	}

	req->next = NULL;
	req->queued--;
}

//! Reads from a device synchronously
status_t devReadSync(device_t* dev, qword pos, void* buffer, size_t* length)
{
	request_t req;
	req.code = DEV_READ;
	req.params.read.buffer = buffer;
	req.params.read.length = *length;
	req.params.read.pos = pos;

	if (devRequestSync(dev, &req) == 0)
	{
		*length = req.params.read.length;
		return 0;
	}
	else
		return req.result;
}

//! Writes to a device synchronously
status_t devWriteSync(device_t* dev, qword pos, const void* buffer, size_t* length)
{
	request_t req;
	req.code = DEV_WRITE;
	req.params.write.buffer = buffer;
	req.params.write.length = *length;
	req.params.write.pos = pos;

	if (devRequestSync(dev, &req) == 0)
	{
		*length = req.params.write.length;
		return 0;
	}
	else
		return req.result;
}

//! Issues a request from user mode
status_t devUserRequest(device_t* dev, request_t* req, size_t size)
{
	request_t* kreq;
	void* buffer;
	bool ret;

	if (size < sizeof(request_t))
		size = sizeof(request_t);
	if (!dev)
		return EINVALID;

	kreq = (request_t*) hndAlloc(size, NULL);
	assert(kreq != NULL);

	memcpy(kreq, req, size);
	req->kernel_request = kreq;
	req->original_request_size = 0;
	kreq->kernel_request = NULL;
	kreq->original_request_size = size;
	//wprintf(L"[dur] User request = %p, kernel request = %p\n", 
		//req, req->kernel_request);

	if (devIsBufferedRequest(req->code))
	{
		buffer = malloc(req->params.buffered.length);
		//wprintf(L"[dur buffered] User buffer = %p, kernel buffer = %p, size = %d\n",
			//req->params.buffered.buffer, buffer, req->params.buffered.length);
		assert(buffer != NULL);
		memcpy(buffer, (void*) req->params.buffered.buffer, req->params.buffered.length);
		kreq->params.buffered.buffer = (addr_t) buffer;
	}

	ret = devRequest(dev, kreq);

	if (ret)
	{
		hndFree(kreq);
		req->kernel_request = NULL;
	}

	/*
	 * event and result need to be reflected now, because they will have been 
	 *	updated by devRequest, and are needed before devUserFinishRequest is
	 *	called.
	 */
	req->event = kreq->event;
	req->result = kreq->result;
	return req->result;
}

//! Retrieves the results from a request previously made from user mode
status_t devUserFinishRequest(request_t* req, bool delete_event)
{
	addr_t user_buffer;
	request_t* kreq;
	size_t size;

	if (req == NULL ||
		req->kernel_request == NULL)
		return EINVALID;

	if (req->kernel_request->queued)
		_cputws(L"Request still queued\n");

	kreq = req->kernel_request;
	size = kreq->original_request_size;
	//wprintf(L"[dufr] User request = %p, kernel request = %p, size = %d, sizeof = %d\n", 
		//req, req->kernel_request, size, sizeof(request_t));

	if (devIsBufferedRequest(req->code))
	{
		user_buffer = req->params.buffered.buffer;
		//wprintf(L"[dufr buffered] User buffer = %p, kernel buffer = %p, length = %d\n", 
			//user_buffer, req->kernel_request->params.buffered.buffer, 
			//req->params.buffered.length);
		memcpy((void*) user_buffer, (void*) req->kernel_request->params.buffered.buffer, 
			req->params.buffered.length);
		free((void*) req->kernel_request->params.buffered.buffer);
	}

	memcpy(req, kreq, size);
	hndFree(kreq);

	if (delete_event)
		hndFree(req->event);

	//_cputws(L"[dufr] All finished!\n");
	return req->result;
}

//! Searches for a specific resource in a device configuration list
/*!
 *	\param	cfg	The configuration structure to search
 *	\param	cls	The type of resource to return
 *	\param	index	The resource index to return
 *	\return	The number of the index'th resource of type cls in the 
 *		configuration structure, or (dword) -1 if unsuccessful.
 */
dword devFindResource(const device_config_t *cfg, int cls, int index)
{
	int i, j;

	j = 0;
	for (i = 0; i < cfg->num_resources; i++)
	{
		if (cfg->resources[i].cls == cls)
		{
			if (j == index)
				return i;

			j++;
		}
	}

	return (dword) -1;
}