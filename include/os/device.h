#ifndef __OS_DEVICE_H
#define __OS_DEVICE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/*!
 *	\defgroup	dev	Device Manager
 *	@{
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
	addr_t event;		// out when calling devRequest;
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
			addr_t fd;					// out
		} fs_open;

		struct
		{
			const wchar_t* name;		// in
			size_t name_length;			// in
			addr_t dev;					// in
		} fs_mount;

		struct
		{
			addr_t buffer;				// out
			size_t length;				// in/out
			addr_t fd;					// in
		} fs_read, fs_write;

		struct
		{
			addr_t fd;					// in
		} fs_close;
	} params;

	dword cks;
};

addr_t devOpen(const wchar_t* name, const wchar_t* params);
bool devClose(addr_t dev);
status_t devUserRequest(addr_t dev, request_t* req, size_t size);
status_t devUserFinishRequest(request_t* req, bool delete_event);
size_t devReadSync(addr_t dev, qword pos, void* buffer, size_t length);
size_t devWriteSync(addr_t dev, qword pos, void* buffer, size_t length);
status_t devUserRequestSync(addr_t dev, request_t* req, size_t size);

//@}

#ifdef __cplusplus
}
#endif

#endif