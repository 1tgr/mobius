#ifndef __ATA_H
#define __ATA_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/driver.h>

/*!
 *  \ingroup drivers
 *  \defgroup ata ATA/ATAPI
 *  @{
 */

/* ATA register file (offsets from 0x1F0 or 0x170) */
#define	ATA_REG_DATA		0	/* data (16-bit) */
#define	ATA_REG_FEAT		1	/* write: feature reg */
#define	ATA_REG_ERROR		ATA_REG_FEAT	/* read: error */
#define	ATA_REG_COUNT		2	/* sector count */
#define	ATA_REG_SECTOR		3	/* sector */
#define	ATA_REG_LOCYL		4	/* LSB of cylinder */
#define	ATA_REG_HICYL		5	/* MSB of cylinder */
#define	ATA_REG_DRVHD		6	/* drive select; head */
#define	ATA_REG_CMD			7	/* write: drive command */
#define	ATA_REG_STATUS		7	/* read: status and error flags */
#define	ATA_REG_DEVCTRL		0x206	/* write: device control */
//efine	ATA_REG_ALTSTAT		0x206	/* read: alternate status/error */

/* a few of the ATA registers are used differently by ATAPI... */
#define	ATAPI_REG_REASON	2	/* interrupt reason */
#define	ATAPI_REG_LOCNT		4	/* LSB of transfer count */
#define	ATAPI_REG_HICNT		5	/* MSB of transfer count */

/* ATA command bytes */
#define	ATA_CMD_READ		0x20	/* read sectors */
#define	ATA_CMD_PKT			0xA0	/* signals ATAPI packet command */
#define	ATA_CMD_PID			0xA1	/* identify ATAPI device */
#define	ATA_CMD_READMULT	0xC4	/* read sectors, one interrupt */
#define	ATA_CMD_MULTMODE	0xC6
#define	ATA_CMD_ID			0xEC	/* identify ATA device */

#define ATA_ERR				0x01
#define ATA_IDX				0x02
#define ATA_CORR			0x04
#define ATA_DRQ				0x08
#define ATA_DSC				0x10
#define ATA_DF				0x20
#define ATA_READY			0x40
#define ATA_BUSY			0x80

//!@}

#ifdef __cplusplus
}
#endif

#endif