/* $Id: devicebook.c,v 1.1 2002/03/13 14:38:30 pavlovskii Exp $ */

/*!
 *	\page	devicebook	Device Driver Book
 *	Your driver starts execution at the \p DrvEntry routine. It is passed one
 *	parameter by the device manager (\p driver_t*), which identifies your 
 *	device driver. Here you need to set the entry point(s) for your own device
 *	enumeration functions:
 *
 *	- \p add_device for adding block and character devices
 *	- \p mount_fs for mounting file systems
 *
 *	You'd normally only initialize one of these, depending on what kind of
 *	driver you were writing.
 *
 *	\subsection	dev	Block and Character Devices
 *
 *	\sa	driver_t#add_device, device_vtbl_t#request, device_vtbl_t#isr, 
 *		device_vtbl_t#finishio
 *
 *	Block and character devices (or just devices) are created through the
 *	\p add_device function of the driver object. \p add_device is called by
 *	the bus driver that hosts your device; usually this will be either the
 *	PCI or ISA drivers, although bus drivers include things like ATA 
 *	controllers and serial port enumerators.
 *
 *	In your \p add_device you first need to deduce what kind of device you're
 *	supposed to be creating (if you support multiple devices in one driver).
 *	For PCI devices you can look at the \p device_config_t::vendor_id and 
 *	\p device_config_t::device_id fields; identification of other devices 
 *	depends on the bus driver, and the device information might be encoded
 *	in the \p device_config_t structure, in the name, or elsewhere.
 *
 *	To create your device object you need to allocate a \p device_t structure,
 *	usually with \p malloc (although some drivers, particularly those for 
 *	software devices such as \p tty, keep a static array of devices).
 *	Note that most drivers define their own structure with a \p device_t
 *	structure as the first field and pass the address of that field to 
 *	the device manager. Once you've allocated your device object you need to:
 *
 *	- Set up the \p vtbl pointer to your driver's virtual function table
 *	- Reset the hardware device to a known configuration
 *	- Register any IRQs used by the device using \p DevRegisterIrq
 *	- Return a pointer to the \p device_t structure
 *
 *	The device manager uses virtual function tables to allow you to write
 *	drivers as C++ classes (just inherit from \p device_t or, better,
 *	kernel::device and implement three virtual functions). In C you need to
 *	define a \p const global instance of \p device_vtbl_t and initialize it
 *	with function pointers for your device class.
 *
 *	\note	The device manager will complain if either the \p vtbl or
 *		\p vtbl->request pointers are \p NULL; both of these are required
 *		for every driver.
 *
 *	Once you've returned from your \p add_device function, your device is
 *	entered into the device manager's namespace (and into the 
 *	\p /System/Devices directory) and is available for use by applications.
 *
 *	Your device will receive I/O requests through its \p request function. 
 *	This is the core of the device's code and it is responsible for 
 *	dispatching requests issued via \p IoRequest (and, by extension, 
 *	\p IoReadSync, \p FsWrite and the others). Requests are represented by 
 *	the \p request_t structure passed to the request function. This is 
 *	identical to the pointer passed to \p IoRequest (the request function 
 *	runs in the same context as the originator) and it is really only a 
 *	pointer to the header of a larger structure. The various types of 
 *	requests can be distinguished by the \p request_t::code field; each 
 *	request code has different information ("parameters") stored after the 
 *	request header.
 *
 *	The two main codes you'll see are \p DEV_READ and \p DEV_WRITE. Not 
 *	surprisingly, these are invoked when something needs to read from or
 *	write to your device; when invoked from user mode, these code via
 *	\p FsRead and \p FsWrite. Both \p DEV_READ and \p DEV_WRITE have similar
 *	parameters: the \p request_dev_t structure defines both the parameters
 *	and the header, and \p request_dev_t::params::buffered can be used for
 *	each. You are provided with three pieces of information: a buffer,
 *	the length of the buffer, and an offset.
 *	
 *	- The buffer is the address passed to \p FsRead or \p FsWrite and could 
 *	be in user or kernel space. It will be accessible from inside the 
 *	\p request function but not necessarily anywhere else.
 *	- The extent of the buffer is defined by the length. Do not write anywhere
 *	outside the valid extent of the buffer.
 *	- The offset is used for block devices (such as hard drives), and can be
 *	ignored for character devices (such as mice). It is specified as a 64-bit
 *	number relative to the start of the device.
 *
 *	\note	If your device can't handle the length or offset given (for 
 *		example, disk drivers generally read in 512-byte blocks on boundaries
 *		of 512 bytes), feel free to set \p request_t::result appropriately
 *		and return \p false.
 *
 *	You've got all the parameters you need now. If the operation is going to
 *	be quick then you can handle it there and then and copy the results into
 *	the buffer. If you don't read as many bytes as requested, update the 
 *	length field before returning. If the operation fails you need to set
 *	request_t::result to an appropriate error code and return \p false from
 *	the request function; for success you just need to return \p true.
 *
 *	If the operation is going to take some time (any interrupt-driven I/O
 *	falls into this category) then you need to use asynchronous I/O. Each
 *	device has an internal queue of asynchronous requests, which the device
 *	manager maintains. In the case of async I/O, all the request function 
 *	does is validate the parameters and queue the request using 
 *	\p DevQueueRequest. If the hardware device is idle the driver needs to
 *	start processing the request it just queued, usually in a common 
 *	\p startio routine. Here the driver needs to get the hardware to start
 *	the I/O operation, with the expectation that it will trigger a hardware
 *	interrupt when it has finished. Once the hardware has started, but before
 *	it finishes, the request function can return.
 *
 *	Apart from queueing the request, the \p DevQueueRequest function does
 *	two main things:
 *	- Sets \p request_t::result to \p SIOPENDING, which tells the originator
 *		that the driver has queued an asynchronous request.
 *	- Locks the user buffer
 *
 *	The locking part is important. The Möbius is both multi-tasking and
 *	protected; there can be many processes running at once, each of which
 *	has its own address space, and any of which can call your driver. In
 *	addition to this, your hardware could interrupt at any time, with the 
 *	result that your interrupt handler could be called from any context.
 *	By locking the user buffer, the device manager makes sure that it is
 *	all present in physical memory and that it will not be paged out until
 *	the operation has finished. \p DevQueueRequest returns a pointer to
 *	an asyncio_t structure, which is followed by an array of physical 
 *	addresses (one for each page of the user buffer).
 *
 *	\note	Although most of the \p asyncio_t structure is opaque, you 
 *		can modify the asyncio_t::length and asyncio_t::extra fields as your
 *		request is processed. A lot of drivers record the number of bytes
 *		transferred in the \p length field, and the \p extra field can point
 *		to anything in kernel memory.
 *
 *	Now that the request has been queued, and your request function has 
 *	returned, the kernel is free to keep on scheduling other threads; if it
 *	wants, the originator (which might be a user process, a file system
 *	driver or something else) can keep on working until your driver has 
 *	finished.
 *
 *	When your hardware triggers an interrupt your \p isr function will be
 *	called (assuming you called \p DevRegisterIrq in \p add_device). 
 *	Remember that it will be called in an arbitrary context: don't modify
 *	any user memory and don't take too long. Here you need to check which
 *	request this interrupt applies to (if you're handling requests 
 *	sequentially it will always be the first one, at \p device_t::io_first) 
 *	and retrieve the results from the hardware. You'll probably need to access 
 *	the request's user buffer at this point. Remember that \p DevQueueRequest
 *	locked it: now you can call \p DevMapBuffer to access that buffer.
 *	\p DevMapBuffer maps the original buffer's physical memory pages into
 *	a temporary region of kernel address space. Due to the nature of the
 *	32-bit paged architecture it can only map whole pages, whereas the 
 *	originator is free to specify buffers aligned to single bytes. So
 *	you need to add \p asyncio_t::mod_buffer_start to the address returned
 *	by \p DevMapBuffer to get the real start address; don't access any memory
 *	before that point because it's almost certainly not yours. Call
 *	\p DevUnmapBuffer when you're finished with the memory.
 *
 *	If this request has finished (it might be finished after one IRQ, or the
 *	device might give you several IRQs before the operation is finished, as
 *	floppy drives do) then you need to call \p DevFinishIo. This effectively
 *	performs the reverse of \p DevQueueRequest: it unlocks the user buffer and
 *	frees any internal memory allocated. It also signals the originator's
 *	\p finishio routine, if the originator was a device (e.g. the 
 *	\p devfs file system that maintains the \p /System/Devices directory).
 *	This allows nested hierarchies of devices to be created: the originator
 *	is able to go off and continue its own request, which will notify the
 *	next higher originator, and so on (until user mode is reached).
 *
 *	\subsection	fsd	File Systems
 *
 *	\sa	driver_t#mount_fs
 *
 *	File system drivers follow much the same lines as regular device drivers:
 *	they define the \p mount_fs entry point in the \p driver_t structure, which
 *	the device manager calls when a file system managed by that FSD is mounted.
 *
 *	At mount time the FSD receives two pieces of information: the VFS path 
 *	where the volume is being mounted and the physical device on which the 
 *	volume resides. The path is a more-or-less gratuitous piece of information;
 *	however, the FSD needs to take note of the device pointer because that is
 *	the device that the FSD will be talking to.
 *
 *	In the \p mount_fs routine, the FSD needs to validate the device (i.e. 
 *	check that it is the correct type of file system) and load any file system
 *	structure data that will be needed. For example, the FAT driver loads the 
 *	boot sector, a copy of the FAT and the root directory entries here. 
 *	\p mount_fs returns a pointer to a \p device_t (just like \p add_device) 
 *	which is allocated by the FSD and can contain extra information beyond the
 *	standard fields. The device manager and VFS will use the same \p device_t
 *	later when making requests.
 *
 *	Requests are received through the file system device's \p request routine
 *	as normal; there is a separate set of \p FS_XXX request codes and FS 
 *	devices are not obliged to support other sets of codes such as \p DEV_XXX.
 *
 *	The other basic object in the file system world is the \p file_t ; this 
 *	contains information the VFS needs for each file (a pointer to the 
 *	associated FSD, the current file pointer and the file access mode) as well 
 *	as any information the FSD might need to keep. Again, \p file_t's are 
 *	allocated and freed by the FSD and only the first set of standard fields 
 *	need be present. User-mode file handles map directly to \p file_t 
 *	structures; in addition, all \p FS_XXX requests deal in file handles so it 
 *	is up to the FSD to validate the \p file_t pointer in the calling 
 *	process's handle table (a matter of calling the \p HndLock and \p HndUnlock 
 *	functions).
 *
 *	The two places where files are allocated are in the implementation of the 
 *	\p FS_CREATE and \p FS_OPEN requests. These provide virtually the same
 *	parameters (path relative to the FS device's mount point and access mode);
 *	as the names suggest, \p FS_CREATE creates a new file and \p FS_OPEN opens
 *	an existing one. Each return a new file handle. The FSD needs to set the
 *	\p file_t.flags member as well (this is not done automatically by 
 *	\p FsCreate and \p FsOpen).
 *
 *	File accesses take place via the \p FS_READ and \p FS_WRITE requests; the 
 *	file's access mode is checked by \p FsRead and \p FsWrite before the 
 *	request reaches the FSD, so all the FSD needs to do is lock the file handle,
 *	read the data, and unlock the handle. Read and write requests can take 
 *	place at arbitrary file offsets and with no restriction to the number of
 *	bytes, so the FSD needs to (a) check the file pointer on entry (and perform
 *	a seek if necessary) and (b) handle different numbers of bytes correctly.
 *
 *	There are two areas where the read/write length can matter:
 *	-# The operation would take the file pointer beyond the end of the file.
 *	-# The number of bytes is different to the file system's cluster size
 *	and/or the device's sector size.
 *
 *	(1) relates to how the FSD handles the end-of-file condition. It needs to 
 *	check how far the file pointer is from the end of the file; if it could
 *	transfer at least one byte, it needs to do so and update the number of
 *	bytes transferred, which will be passed back to the caller along with a
 *	success status (assuming the transfer didn't fail for some other reason). 
 *	If the file pointer is already at or beyond the end of the file, the FSD 
 *	needs to report zero bytes transferred and give an \p EEOF error code. In
 *	other words: an EOF while reading or writing results in fewer bytes being
 *	transferred but is not an error, whereas reading or writing when the file
 *	pointer is already at the end results in zero bytes transferred and an 
 *	\p EEOF error.
 *
 *	(2) relates to the fact that typical disk devices can only transfer data
 *	in multiples of one sector, and that file systems typically align file
 *	data to cluster boundaries (with clusters being a whole number of sectors
 *	each). The easiest (and best) way to look after this is to use the file
 *	cache. This way, the FSD reads data into and writes data from the system's
 *	cache, which can be a whole number of clusters long, and merely copies data
 *	between the cache and the user buffer when necessary.
 *
 *	The general cacheing scheme is as follows.
 *	- On \p FS_OPEN or \p FS_CREATE, call \p CcCreateFileCache to create a new
 *	cache object. Typically you will use the file system's cluster size as the
 *	block size, although you are free to use whatever block size is most 
 *	convenient.
 *	- On \p FS_READ, call \p CcIsBlockValid to check whether the requested
 *	block (cluster) is already present in memory. If not, you need to request
 *	that block's memory address using \p CcRequestBlock and read into there 
 *	(remembering to call \p CcReleaseBlock afterwards -- cache blocks are
 *	reference counted; \p CcRequestBlock also locks the block down into 
 *	physical memory in anticipation of a call to \p DevMapBuffer by the disk
 *	driver). Once the read has completed, repeat the process; the block will
 *	now be valid and you will be able to \p memcpy from the cache into the
 *	user buffer.
 *	- Handle \p FS_WRITE similarly to \p FS_READ, except that you will usually
 *	only \p memcpy into the cache block; flushing (i.e. writing the cache block
 *	to disk) will happen later.
 *	- On \p FS_CLOSE, free the cache using \p CcDeleteFileCache . The cache
 *	manager will callback the FSD to get it to flush any dirtied pages.
 *
 *	The cache manager will sometimes need to clear out cached pages (for 
 *	example, when clearing out unneeded pages prior to allocation). Physical 
 *	memory for cache blocks is only allocated when it is valid (so cacheing
 *	a huge file will only result in the relevant parts being kept in memory),
 *	but it will still be necessary for the cache manager to request that the
 *	FSD flushes some cache pages. The mechanism for this has not been 
 *	implemented yet.
 *
 */