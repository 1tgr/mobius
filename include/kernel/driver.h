#ifndef __KERNEL_DRIVER_H
#define __KERNEL_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/proc.h>
#include <kernel/handle.h>

/*!
 * \defgroup dev Device Manager
 * @{
 */

#include <os/devreq.h>

typedef struct request_t request_t;

//! Contains information on a request issued to a device driver
struct request_t
{
	//! Specifies the request code (one of DEV_xxx)
	dword code;			// in
	//! Indicates the result of the request
	status_t result;		// out
	//! Triggered when the request has completed
	handle_t* event;	// out when calling devRequest;
						// in wrt driver

	//! Points to the next request in a driver's request list
	request_t* next;

	/* Internal members */
	size_t user_length;		// original user length
	request_t *kernel_request;	// in user mode, points to the copy made in kernel space
	size_t original_request_size;	// in kernel mode, the size of the original request
	int queued;				// is this request queued?

	union
	{
		//! General parameters for a buffered request
		struct
		{
			//! The buffer used
			addr_t buffer;
			//! Size of the buffer in bytes
			size_t length;
			//! The position where the buffered operation is to start
			/*! Ignored for character stream devices. */
			qword pos;
		} buffered;

		//! Parameters for a DEV_OPEN request
		struct
		{
			//! Specifies the parameters passed to devOpen()
			const wchar_t* params;
		} open;

		//! Parameters for a DEV_READ request
		struct
		{
			//! Specifies the buffer into which data are read
			void* buffer;		// out
			//! Specifies the number of bytes to read, and indicates the number
			//!	of bytes actually read
			size_t length;		// in/out
			//! Specifies the offset where reading is to start
			/*! Ignored for character stream devices. */
			qword pos;			// in
		} read;

		//! Parameters for a DEV_WRITE request
		struct
		{
			//! Specifies the buffer from which data are written
			const void* buffer;	// in
			//! Specifies the number of bytes to write, and indicates the number
			//!	of bytes actually written
			size_t length;		// in/out
			//! Specifies the offset where writing is to start
			/*! Ignored for character stream devices. */
			qword pos;			// in
		} write;

		//! Parameters for a DEV_IOCTL request
		struct
		{
			//! Specifies a buffer containing ioctl parameters and data
			void* buffer;		// in/out
			//! Specifies the size of the buffer
			size_t length;		// in/out
			//! Specifies the IOCTL function to perform
			dword code;		// in
		} ioctl;

		//! Parameters for a DEV_ISR request
		struct
		{
			//! Specifies the IRQ which was triggered
			byte irq;			// in
		} isr;

		//! Parameters for an FS_OPENFILE request
		struct
		{
			const wchar_t* name;		// in
			size_t name_length;			// in
			struct file_t* fd;			// out
		} fs_open;

		struct
		{
			const wchar_t* name;		// in
			size_t name_length;			// in
			struct device_t* dev;		// in
		} fs_mount;

		struct
		{
			addr_t buffer;				// out
			size_t length;				// in/out
			struct file_t* fd;			// in
		} fs_read, fs_write;

		struct
		{
			struct file_t* fd;			// in
		} fs_close;
	} params;

	dword cks;
};

typedef struct device_t device_t;
typedef struct driver_t driver_t;

//! The prototype of the driver's request function
typedef bool (*DEVREQ)(device_t*, request_t*);

typedef struct device_config_t device_config_t;

//! The kernel structure used to contain information on each device
/*!
 * Since it is the device driver's responsibility to allocate memory for this
 *	structure, the driver author may place additional fields after this 
 *	structure. For example, it is possible to use an instance of this structure
 *	as the first member of a larger one.
 */
struct device_t
{
	//! The driver that registered this device
	driver_t* driver;
	//! The function to be called for each device request
	DEVREQ request;
	//! Pointer to the device's first queued request, if any
	request_t *req_first,
	//! Pointer to the device's last queued request, if any
		*req_last;
	device_config_t *config;
};

typedef struct device_resource_t device_resource_t;

enum { dresMemory, dresIo, dresIrq };
struct device_resource_t
{
	int cls;
	union
	{
		struct
		{
			addr_t base;
			size_t length;
		} memory;

		struct
		{
			addr_t base;
			size_t length;
		} io;

		byte irq;
	} u;
};

struct device_config_t
{
	device_t *parent;
	word vendor_id, device_id;
	dword subsystem;
	int num_resources;
	device_resource_t *resources;
};

//! The kernel structure used to maintain each device driver DLL
struct driver_t
{
	//! The module used to load this driver
	module_t* mod;
	device_t* (*add_device)(driver_t*, const wchar_t*, device_config_t*);
	device_t* (*mount_fs)(driver_t*, const wchar_t*, device_t*);
};

device_t*	devOpen(const wchar_t* name, const wchar_t* params);
bool		devClose(device_t* dev);
bool		devRemove(device_t* dev);
status_t	devRequest(device_t* dev, request_t* req);
status_t	devRequestSync(device_t* dev, request_t* req);
bool		devRegisterIrq(device_t* dev, byte irq, bool install);
driver_t*	devInstallNewDevice(const wchar_t* name, device_config_t* cfg);

bool		devRegister(const wchar_t* name, device_t* dev, device_config_t* cfg);
//bool		devReadConfig(const wchar_t* name, device_config_t* cfg, size_t size);

void		devStartRequest(device_t* dev, request_t* req);
void		devFinishRequest(device_t* dev, request_t* req);
status_t	devReadSync(device_t* dev, qword pos, void* buffer, size_t* length);
status_t	devWriteSync(device_t* dev, qword pos, const void* buffer, size_t* length);
status_t	devUserRequest(device_t* dev, request_t* req, size_t size);
status_t	devUserFinishRequest(request_t* req, bool delete_event);
dword		devFindResource(const device_config_t *cfg, int cls, int index);

#define		devIsBufferedRequest(code) (((int) (code)) < 0)
	
bool STDCALL	drvInit(driver_t*);
void		msleep(dword ms);

//@}

/*!
 *  \defgroup drivers Device Drivers
 */

#ifdef __cplusplus
}
#endif

#endif