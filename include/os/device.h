/* $Id: device.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */
#ifndef __OS_DEVICE_H
#define __OS_DEVICE_H

#include <sys/types.h>

/*!
 *    \ingroup	  libsys
 *    \defgroup    osdev    Device Interface
 *    @{
 */

typedef union params_port_t params_port_t;
/*!    \brief	 Parameters for a \b PORT_xxx request */
union params_port_t
{
    struct
    {
        uint32_t name_size;
        const wchar_t *remote;
        handle_t port;
    } port_connect;

    struct
    {
        uint32_t unused[2];
        handle_t port;
    } port_listen;

    struct
    {
        uint32_t unused[2];
        handle_t port;
        handle_t client;
        uint32_t flags;
    } port_accept;
};

#define REQUEST_CODE(buff, sys, maj, min)    \
    ((uint32_t) ((buff) << 31 | \
    ((sys) << 16) | \
    ((maj) << 8) | (min)))

#define DEV_OPEN            REQUEST_CODE(0, 0, 'd', 'o')
#define DEV_CLOSE           REQUEST_CODE(0, 0, 'd', 'c')
#define DEV_REMOVE          REQUEST_CODE(0, 0, 'd', 'e')
#define DEV_READ            REQUEST_CODE(1, 0, 'd', 'r')
#define DEV_WRITE           REQUEST_CODE(1, 0, 'd', 'w')
#define DEV_READ_DIRECT     REQUEST_CODE(1, 0, 'd', 'R')
#define DEV_WRITE_DIRECT    REQUEST_CODE(1, 0, 'd', 'W')
#define DEV_IOCTL           REQUEST_CODE(1, 0, 'd', 'i')

#define FS_OPEN             REQUEST_CODE(1, 0, 'f', 'o')
#define FS_CREATE           REQUEST_CODE(1, 0, 'f', 'C')
#define FS_CLOSE            REQUEST_CODE(0, 0, 'f', 'c')
#define FS_MOUNT            REQUEST_CODE(1, 0, 'f', 'm')
#define FS_READ             REQUEST_CODE(1, 0, 'f', 'r')
#define FS_WRITE            REQUEST_CODE(1, 0, 'f', 'w')
#define FS_IOCTL            REQUEST_CODE(1, 0, 'f', 'i')
#define FS_QUERYFILE        REQUEST_CODE(0, 0, 'f', 'q')
#define FS_OPENSEARCH       REQUEST_CODE(1, 0, 'f', 's')
#define FS_PASSTHROUGH      REQUEST_CODE(1, 0, 'f', 'p')

#define BLK_GETSIZE         REQUEST_CODE(1, 0, 'b', 's')

#define CHR_GETSIZE         REQUEST_CODE(1, 0, 'c', 's')

/* Ports respond to the FS_xxx codes as well */
#define PORT_CONNECT        REQUEST_CODE(1, 0, 'p', 'c')
#define PORT_LISTEN         REQUEST_CODE(0, 0, 'p', 'l')
#define PORT_ACCEPT         REQUEST_CODE(0, 0, 'p', 'a')

#define NET_GET_HW_INFO     REQUEST_CODE(0, 0, 'n', 'h')

#define ETH_SEND            REQUEST_CODE(1, 0, 'e', 's')
#define ETH_RECEIVE         REQUEST_CODE(1, 0, 'e', 'r')

/*! @} */

#endif
