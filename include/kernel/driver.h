/* $Id: driver.h,v 1.12 2002/03/04 18:56:07 pavlovskii Exp $ */
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

/*!
 *    \ingroup    kernel
 *    \defgroup    dev    Device Driver Interface
 *    @{
 *
 *    These are the functions that are intended to be called by device drivers.
 *    Note that you can call any exported kernel function from your drivers
 *    (including things like \p libc), but that this is the 'official' driver 
 *    API.
 *
 *    \ref    devicebook
 *
 */

typedef struct request_t request_t;
/*!
 *    \brief    Specifies parameters for a device request
 */
struct request_t
{
    uint32_t code;
    status_t result;
    request_t *original;
    struct device_t *from;
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

typedef struct asyncio_t asyncio_t;

/*!
 *    \brief    Holds device request information in a device's asynchronous queue
 */
struct asyncio_t
{
    asyncio_t *prev, *next;

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
    /*! Size, in pages, of the user buffer */
    size_t length_pages;
    /*! Offset of the start of the user buffer from the previous page boundary */
    unsigned mod_buffer_start;
    /*! Physical page addresses follow this structure */
#ifdef __DOXYGEN__
    addr_t buffer[];
#endif
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
/*!    Device configuration structure */
struct device_config_t
{
    device_t *parent;
    unsigned resource_count;
    uint16_t vendor_id;
    uint16_t device_id;
};

#define DEV_CFG_RESOURCES(cfg)    ((device_resource_t*) ((cfg) + 1))

typedef struct driver_t driver_t;
/*!
 *    \brief    Device driver structure
 *
 *    The device manager maintains one of these for every driver. It is created
 *    by \p DevInstallNewDriver and freed by \p DevRemove.
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
     *    response to a \p DEV_REMOVE request.
     *
     *    \param    driver    Driver object (the same one passed to \p DrvInit )
     *    \param    name    Name of the device (may be \p NULL )
     *    \param    config    Device configuration (may be \p NULL )
     *    \return    A pointer to a device object
     */
    device_t * (*add_device)(driver_t *driver, const wchar_t *name, 
        device_config_t *config);

    /*!
     *    \brief    Called when a file system supported by the driver is mounted
     *
     *    This function fulfils much the same purpose as \p driver_t::mount_fs,
     *    except that it is called when \p FsMount needs to mount a file system.
     *    The file system driver needs to determine the format of the file
     *    system, allocate a file system device object and mount the file system.
     *    The FSD will deallocate itself in response to a \p DEV_REMOVE request.
     *
     *    \param    driver    Driver object (the same oone passed to \p DrvInit )
     *    \param    path    Path where the file system is to be mounted
     *    \param    dev    Device that on which the file system is to be fount
     *    \return    A pointer to a file system device object
     */

    device_t * (*mount_fs)(driver_t *driver, const wchar_t *path, 
        device_t *dev);
};

typedef struct device_vtbl_t device_vtbl_t;
/*!    \brief    Virtual function table for device objects */
struct device_vtbl_t
{
    /*!    \brief    Initialites a request */
    bool (*request)(device_t *dev, request_t *req);
    /*!    \brief    Signals an interrupt */
    bool (*isr)(device_t *dev, uint8_t irq);
    /*!    \brief    Signals completion of an I/O request queued for the driver */
    void (*finishio)(device_t *dev, request_t *req);
};

#ifdef __cplusplus
/*!    \brief    Device object (C++ definition) */
struct __attribute__((com_interface)) device_t
{
    virtual bool request(request_t *req) = 0;
    virtual bool isr(uint8_t irq) = 0;
    virtual void finishio(request_t *req) = 0;

    driver_t *driver;
    device_config_t *cfg;
    asyncio_t *io_first, *io_last;
    const struct dirent_device_t *info;
};
#else
/*!    \brief    Device object (C definition) */
struct device_t
{
    driver_t *driver;
    device_config_t *cfg;
    asyncio_t *io_first, *io_last;
    const struct dirent_device_t *info;
    const device_vtbl_t *vtbl;
};
#endif

typedef struct irq_t irq_t;
struct irq_t
{
    irq_t *next;
    device_t *dev;
};

/* Kernel device driver helper routines */
driver_t *    DevInstallNewDriver(const wchar_t *name);
device_t *DevInstallDevice(const wchar_t *driver, const wchar_t *name, 
                           device_config_t *cfg);
void    DevUnloadDriver(driver_t *driver);

bool    DevRegisterIrq(uint8_t irq, device_t *dev);
bool    DevAddDevice(device_t *dev, const wchar_t *name, 
                     device_config_t *cfg);
asyncio_t*    DevQueueRequest(device_t *dev, request_t *req, size_t size,
                            void *user_buffer,
                            size_t user_buffer_length);
void    DevFinishIo(device_t *dev, asyncio_t *io, status_t result);
uint8_t    DevCfgFindIrq(const device_config_t *cfg, unsigned n, uint8_t dflt);
void *    DevMapBuffer(asyncio_t *io);

void    MemUnmapTemp(void);
#define DevUnmapBuffer    MemUnmapTemp

/* Driver entry point (you write this) */
bool	DrvEntry(driver_t *);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif