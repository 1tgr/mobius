/* $Id: driver.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */
#ifndef __KERNEL_DRIVER_H
#define __KERNEL_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/handle.h>
#include <os/device.h>
#include <os/comdef.h>
#include <kernel/memory.h>
#include <kernel/io.h>

#ifndef __DEVICE_T_DEFINED
typedef struct device_t device_t;
#define __DEVICE_T_DEFINED
#endif

/*!
 *    \ingroup    kernel
 *    \defgroup    dev    Device Driver Interface
 *    @{
 *
 *    These are the functions that are intended to be called by device drivers.
 *    Note that you can call any exported kernel function from your drivers
 *    (including things like \b libc), but that this is the 'official' driver 
 *    API.
 *
 *    \ref    devicebook
 *
 */

typedef union params_dev_t params_dev_t;

/*!    \brief	 Parameters for a \b DEV_xxx request */
union params_dev_t
{
    struct
    {
	uint32_t length;
	//void *buffer;
        page_array_t *pages;
	uint64_t offset;
    } buffered;

    struct
    {
        uint32_t length;
        void *buffer;
        uint64_t offset;
    } direct;

    struct
    {
	/*! Number of bytes to read */
	uint32_t length;
	/*! Buffer into which to read */
	//void *buffer;
        page_array_t *pages;
	/*!
	 *    \brief	Offset of the first byte to read
	 *
	 *    Block devices (e.g. disk drives) must honour this; character 
	 *    devices (e.g. serial ports) may ignore this.
	 */
	uint64_t offset;
    } dev_read;

    struct
    {
	/*! Number of bytes to write */
	uint32_t length;
	/*! Buffer from which to write */
	//const void *buffer;
        page_array_t *pages;
	/*!
	 *    \brief	Offset of the first byte to write
	 *
	 *    Block devices (e.g. disk drives) must honour this; character 
	 *    devices (e.g. serial ports) may ignore this.
	 */
	uint64_t offset;
    } dev_write;

    struct
    {
	/*! Number of bytes to read */
	uint32_t length;
	/*! Buffer into which to read */
	void *buffer;
        /*!
	 *    \brief	Offset of the first byte to read
	 *
	 *    Block devices (e.g. disk drives) must honour this; character 
	 *    devices (e.g. serial ports) may ignore this.
	 */
	uint64_t offset;
    } dev_read_direct;

    struct
    {
	/*! Number of bytes to write */
	uint32_t length;
	/*! Buffer from which to write */
	const void *buffer;
        /*!
	 *    \brief	Offset of the first byte to write
	 *
	 *    Block devices (e.g. disk drives) must honour this; character 
	 *    devices (e.g. serial ports) may ignore this.
	 */
	uint64_t offset;
    } dev_write_direct;

    struct
    {
	uint32_t size;
	void *params;
        uint32_t code;
	uint32_t unused;
    } dev_ioctl;

    struct
    {
	uint8_t irq;
    } dev_irq;
};

typedef union params_fs_t params_fs_t;
/*!    \brief	 Parameters for a \b FS_xxx request */
union params_fs_t
{
    struct
    {
	size_t length;
	page_array_t *pages;
	handle_t file;
    } buffered, fs_read, fs_write;

    struct
    {
	size_t length;
	void *buffer;
	handle_t file;
    } direct;

    struct
    {
	size_t length;
	void *buffer;
	handle_t file;
    } fs_read_direct;

    struct
    {
	size_t length;
	const void *buffer;
	handle_t file;
    } fs_write_direct;

    struct
    {
	size_t length;
	void *buffer;
	handle_t file;
	uint32_t code;
    } fs_ioctl;

    struct
    {
	size_t name_size;
	const wchar_t *name;
	handle_t file;
	uint32_t flags;
    } fs_open, fs_create, fs_opensearch;

    struct
    {
	handle_t file;
    } fs_close;

    struct
    {
	size_t buffer_size;
	void *buffer;
	const wchar_t *name;
	uint32_t query_class;
    } fs_queryfile;

    struct
    {
	size_t name_size;
	const wchar_t *name;
	struct device_t *fsd;
    } fs_mount;

    struct
    {
        size_t params_size;
        void *params;
        handle_t file;
        uint32_t code;
    } fs_passthrough;
};

typedef struct request_t request_t;
/*!
 *    \brief    Specifies parameters for a device request
 */
struct request_t
{
    uint32_t code;
    status_t result;
    request_t *original;
    /*struct device_t *from;*/
    io_callback_t callback;
    void *param;
};

typedef struct request_dev_t request_dev_t;
struct request_dev_t
{
    request_t header;
    params_dev_t params;
};

typedef struct request_fs_t request_fs_t;
struct request_fs_t
{
    request_t header;
    params_fs_t params;
};

typedef struct request_port_t request_port_t;
struct request_port_t
{
    request_t header;
    params_port_t params;
};

struct page_array_t;

typedef struct asyncio_t asyncio_t;

/*!
 *    \brief    Holds device request information in a device's asynchronous queue
 */
struct asyncio_t
{
    asyncio_t *prev, *next;
    asyncio_t *thread_prev, *thread_next;

    /*! Thread that owns this request */
    struct thread_t *owner;

    /*! Copy of the original request structure */
    request_t *req;

    /*! Size of the original request structure */
    size_t req_size;

    /*! Device to which this request applies */
    device_t *dev;

    /*! Spare pointer to device-specific information */
    void *extra;

    /*! Size, in bytes, of the user buffer */
    size_t length;

    /*! Page array structure for the request's buffer */
    struct page_array_t *pages;
};

typedef struct dev_resource_t dev_resource_t;
struct dev_resource_t
{
    enum
    {
        resMemory,
        resIo,
        resIrq
    } cls;

    union
    {
        struct
        {
            addr_t base;
            size_t length;
        } memory;

        struct
        {
            uint16_t base;
            unsigned length;
        } io;

        uint8_t irq;
    } u;
};

#define DEV_BUS_UNKNOWN		0
#define DEV_BUS_ISA			1
#define DEV_BUS_PCI			2
#define DEV_BUS_ATA			3
#define DEV_BUS_ATA_DRIVE	4

typedef struct pci_location_t pci_location_t;
struct pci_location_t
{
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
};

typedef struct pci_businfo_t pci_businfo_t;
struct pci_businfo_t
{
    uint16_t vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t revision_id;
    uint8_t interface;
    uint8_t sub_class;
    uint8_t base_class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;	

    uint8_t irq;
    uint32_t base[6];
    uint32_t size[6];

    uint16_t subsys_vendor;
    uint16_t subsys;
};


typedef struct isa_businfo_t isa_businfo_t;
struct isa_businfo_t
{
	uint16_t vendor_id;
	uint16_t device_id;
	wchar_t name[8];
};


typedef struct dev_config_t dev_config_t;
/*! Device configuration structure */
struct dev_config_t
{
    device_t *bus;
	unsigned bus_type;
    unsigned num_resources;
    dev_resource_t *resources;
	wchar_t *profile_key;
    uint16_t device_class;
	void *businfo;
	union
	{
		void *ptr;
		uint32_t number;
	} location;
};

struct fsd_t;

typedef struct driver_t driver_t;
/*!
 *    \brief    Device driver structure
 *
 *    The device manager maintains one of these for every driver. It is created
 *    by \b DevInstallNewDriver and freed by \b DevRemove.
 */
struct driver_t
{
    struct module_t *mod;
    driver_t *prev, *next;

    /*!
     *    \brief    Called when a device supported by the driver is added
     *
     *    This function is provided by the driver; it needs to determine
     *    the device type required, allocate a device object and
     *    initialize the hardware. The device will deallocate itself in
     *    response to a \b DEV_REMOVE request.
     *
     *    \param    driver    Driver object (the same one passed to \b DrvInit )
     *    \param    name    Name of the device (may be \b NULL )
     *    \param    config    Device configuration (may be \b NULL )
     *    \return    A pointer to a device object
     */
    void (*add_device)(driver_t *driver, const wchar_t *name, 
        dev_config_t *config);

    /*!
     *    \brief    Called when a file system supported by the driver is mounted
     *
     *    This function fulfils much the same purpose as \b driver_t::mount_fs,
     *    except that it is called when \b FsMount needs to mount a file system.
     *    The file system driver needs to determine the format of the file
     *    system, allocate a file system device object and mount the file system.
     *    The FSD will deallocate itself in response to a \b DEV_REMOVE request.
     *
     *    \param    driver    Driver object (the same one passed to \b DrvInit )
     *    \param    path    Path where the file system is to be mounted
     *    \param    dev    Device that on which the file system is to be fount
     *    \return    A pointer to a file system device object
     */

    struct fsd_t * (*mount_fs)(driver_t *driver, const wchar_t *dest);
};

typedef struct device_vtbl_t device_vtbl_t;
/*!    \brief    Virtual function table for device objects */
struct device_vtbl_t
{
	void (*delete_device)(device_t *dev);
    status_t (*request)(device_t *dev, request_t *req);
    bool (*isr)(device_t *dev, uint8_t irq);
    void (*finishio)(device_t *dev, request_t *req);
    bool (*cancelio)(device_t *dev, asyncio_t *io);
	status_t (*claim_resources)(device_t *dev,
		device_t *child,
		dev_resource_t *resources, 
		unsigned num_resources,
		bool is_claiming);
	status_t (*remove_child)(device_t *dev,
		device_t *child);
};


struct device_t
{
	const device_vtbl_t *vtbl;
	void *cookie;
    driver_t *driver;
    dev_config_t *cfg;
    asyncio_t *io_first, *io_last;
    struct device_info_t *info;
    uint32_t flags;
};

typedef struct irq_t irq_t;
struct irq_t
{
    irq_t *next;
    device_t *dev;
};

/* Kernel device driver helper routines */
driver_t *  DevInstallNewDriver(const wchar_t *name);
bool        DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
                             dev_config_t *cfg);
bool		DevInstallIsaDevice(const wchar_t *driver, const wchar_t *device);
void	    DevUnloadDriver(driver_t *driver);

bool	    DevRegisterIrq(uint8_t irq, device_t *dev);
device_t *  DevAddDevice(driver_t *drv, const device_vtbl_t *vtbl, uint32_t flags,
						 const wchar_t *name, dev_config_t *cfg, void *cookie);
asyncio_t*  DevQueueRequest(device_t *dev, request_t *req, size_t size, 
                            page_array_t *pages, size_t user_buffer_length);
void	    DevFinishIo(device_t *dev, asyncio_t *io, status_t result);
bool        DevCancelIo(asyncio_t *io);
uint8_t	    DevCfgFindIrq(const dev_config_t *cfg, unsigned n, uint8_t dflt);
dev_resource_t *DevCfgFindMemory(const dev_config_t *cfg, unsigned n);

/* Driver entry point (you write this) */
bool	    DrvEntry(driver_t *);

/* In pci.drv (pci.lib) */
uint32_t    PciReadConfig(const pci_location_t *loc, unsigned reg, 
                          unsigned bytes);
void        PciWriteConfig(const pci_location_t *loc, unsigned reg, 
                           uint32_t v, unsigned bytes);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif
