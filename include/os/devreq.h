#ifndef __OS_DEVREQ_H
#define __OS_DEVREQ_H

//! Creates a device code made up of subsystem (sys) and major (maj) and 
//!		minor (min) codes
#define REQUEST_CODE(buff, sys, maj, min)	\
	((buff) << 31 | \
	((sys) << 16) | \
	((maj) << 8) | (min))

//! Issued when a device is being opened by the Device Manager
#define	DEV_OPEN	REQUEST_CODE(0, 0, 'd', 'o')
//! Issued when a device is being closed by the Device Manager
#define	DEV_CLOSE	REQUEST_CODE(0, 0, 'd', 'c')
//! Issued when a device is being removed from the Device Manager
#define DEV_REMOVE	REQUEST_CODE(0, 0, 'd', 'R')
//! Reads from a device
#define	DEV_READ	REQUEST_CODE(1, 0, 'd', 'r')
//! Writes to a device
#define	DEV_WRITE	REQUEST_CODE(1, 0, 'd', 'w')
//! Performs general device state manipulation functions
#define	DEV_IOCTL	REQUEST_CODE(1, 0, 'd', 'i')
//! Issued when a device's IRQ is triggered
#define	DEV_ISR		REQUEST_CODE(0, 0, 'd', 'q')

//! Opens a file on a file system device
#define FS_OPEN		REQUEST_CODE(1, 0, 'f', 'o')
//! Closes a file
#define FS_CLOSE	REQUEST_CODE(0, 0, 'f', 'c')
//! Mount a file or device in a directory
#define FS_MOUNT	REQUEST_CODE(1, 0, 'f', 'm')
//! Reads from a file
#define FS_READ		REQUEST_CODE(1, 0, 'f', 'r')
//! Writes to a file
#define FS_WRITE	REQUEST_CODE(1, 0, 'f', 'w')

//! Retrieves size statistics for a block device
#define BLK_GETSIZE	REQUEST_CODE(1, 0, 'b', 's')

#endif