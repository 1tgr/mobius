/* $Id: ata.c,v 1.14 2002/03/04 18:52:25 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/cache.h>
#include <kernel/io.h>

#define DEBUG
#include <kernel/debug.h>

#include <os/syscall.h>

#include <errno.h>
#include <stdio.h>
#include <wchar.h>

/* I/O Ports used by winchester disk controllers. */

/* Read and write registers */
#define REG_BASE0	 0x1F0	  /* base register of controller 0 */
#define REG_BASE1	 0x170	  /* base register of controller 1 */
#define REG_DATA	    0	 /* data register (offset from the base reg.) */
#define REG_PRECOMP	    1	 /* start of write precompensation */
#define REG_COUNT	     2	  /* sectors to transfer */
#define REG_REASON	      2
#define REG_SECTOR	      3    /* sector number */
#define REG_CYL_LO	      4    /* low byte of cylinder number */
#define REG_CYL_HI	      5    /* high byte of cylinder number */
#define REG_COUNT_LO	    REG_CYL_LO
#define REG_COUNT_HI	    REG_CYL_HI
#define REG_LDH 	    6	 /* lba, drive and head */
#define   LDH_DEFAULT		 0xA0	 /* ECC enable, 512 bytes per sector */
#define   LDH_LBA		 0x40	 /* Use LBA addressing */
#define   ldh_init(drive)	 (LDH_DEFAULT | ((drive) << 4))

/* Read only registers */
#define REG_STATUS	      7    /* status */
#define   STATUS_BSY		0x80	/* controller busy */
#define   STATUS_RDY		0x40	/* drive ready */
#define   STATUS_WF		0x20	/* write fault */
#define   STATUS_SC		0x10	/* seek complete (obsolete) */
#define   STATUS_DRQ		0x08	/* data transfer request */
#define   STATUS_CRD		0x04	/* corrected data */
#define   STATUS_IDX		0x02	/* index pulse */
#define   STATUS_ERR		0x01	/* error */
#define REG_ERROR	     1	  /* error code */
#define   ERROR_BB		  0x80	  /* bad block */
#define   ERROR_ECC		0x40	/* bad ecc bytes */
#define   ERROR_ID		  0x10	  /* id not found */
#define   ERROR_AC		  0x04	  /* aborted command */
#define   ERROR_TK		  0x02	  /* track zero error */
#define   ERROR_DM		  0x01	  /* no data address mark */

/* Write only registers */
#define REG_COMMAND	    7	 /* command */
#define   CMD_IDLE		  0x00	  /* for w_command: drive idle */
#define   CMD_RECALIBRATE	 0x10	 /* recalibrate drive */
#define   CMD_READ		  0x20	  /* read data */
#define   CMD_WRITE		0x30	/* write data */
#define   CMD_READVERIFY	0x40	/* read verify */
#define   CMD_FORMAT		0x50	/* format track */
#define   CMD_SEEK		  0x70	  /* seek cylinder */
#define   CMD_DIAG		  0x90	  /* execute device diagnostics */
#define   CMD_SPECIFY		 0x91	 /* specify parameters */
#define   CMD_PACKET		0xA0	/* ATAPI packet cmd */
#define   ATAPI_IDENTIFY	0xA1	/* ATAPI identify */
#define   ATA_IDENTIFY		  0xEC	  /* identify drive */
#define REG_CTL 	0x206	 /* control register */
#define   CTL_NORETRY		 0x80	 /* disable access retry */
#define   CTL_NOECC		0x40	/* disable ecc retry */
#define   CTL_EIGHTHEADS	0x08	/* more than eight heads */
#define   CTL_RESET		0x04	/* reset controller */
#define   CTL_INTDISABLE	0x02	/* disable interrupts */

/* ATAPI packet command bytes */
#define    ATAPI_CMD_START_STOP    0x1B    /* eject/load */
#define    ATAPI_CMD_READ10	   0x28    /* read data sector(s) */
#define    ATAPI_CMD_READTOC	    0x43    /* read audio table-of-contents */
#define    ATAPI_CMD_PLAY	     0x47    /* play audio */
#define    ATAPI_CMD_PAUSE	      0x4B    /* pause/continue audio */

/* Interrupt request lines. */
#define AT_IRQ0 	14	  /* interrupt number for controller 0 */
#define AT_IRQ1 	15	  /* interrupt number for controller 1 */

#define SECTOR_SIZE		   512
#define ATAPI_SECTOR_SIZE	 2048
#define DRIVES_PER_CONTROLLER	 2
#define IDE_BASE_NAME		 L"ide"

#define waitfor(ctrl, mask, value)    \
    ((in(ctrl->base + REG_STATUS) & mask) == value \
	|| AtaWaitForStatus(ctrl, mask, value))

typedef struct ata_drive_t ata_drive_t;
struct ata_drive_t
{
    device_t dev;
    dirent_device_t info;
    unsigned lcylinders;
    unsigned lsectors;
    unsigned lheads;
    unsigned pcylinders;
    unsigned psectors;
    unsigned pheads;
    unsigned precomp;
    uint8_t ldhpref;
    struct ata_ctrl_t *ctrl;
    bool is_atapi;
};

typedef struct ata_ctrl_t ata_ctrl_t;
struct ata_ctrl_t
{
    device_t dev;
    uint16_t base;
    semaphore_t sem;
    ata_drive_t drives[DRIVES_PER_CONTROLLER];
    uint8_t command;
    uint8_t status;
    bool need_reset;
};

#pragma pack(push, 1)

typedef struct partition_t partition_t;
struct partition_t
{
    uint8_t boot;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t system;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_sector_abs;
    uint32_t sector_count;
};

typedef struct ata_identify_t ata_identify_t;
struct ata_identify_t
{
    uint16_t config;		/* obsolete stuff */
    uint16_t cylinders; 	   /* logical cylinders */
    uint16_t _reserved_2;
    uint16_t heads;		   /* logical heads */
    uint16_t _vendor_4;
    uint16_t _vendor_5;
    uint16_t sectors;		  /* logical sectors */
    uint16_t _vendor_7;
    uint16_t _vendor_8;
    uint16_t _vendor_9;
    char serial[20];		/* serial number */
    uint16_t _vendor_20;
    uint16_t _vendor_21;
    uint16_t vend_bytes_long;	  /* no. vendor bytes on long cmd */
    char firmware[8];
    char model[40];
    uint16_t mult_support;	  /* vendor stuff and multiple cmds */
    uint16_t _reserved_48;
    uint16_t capabilities;
    uint16_t _reserved_50;
    uint16_t pio;
    uint16_t dma;
    uint16_t _reserved_53;
    uint16_t curr_cyls; 	   /* current logical cylinders */
    uint16_t curr_heads;	/* current logical heads */
    uint16_t curr_sectors;	  /* current logical sectors */
    uint32_t capacity;		  /* capacity in sectors */
    uint16_t _pad[256-59];	  /* don't need this stuff for now */
};

#pragma pack(pop)

CASSERT(offsetof(ata_identify_t, pio) == 49 * 2);

typedef struct ata_part_t ata_part_t;
struct ata_part_t
{
    device_t dev;
    uint32_t start_sector;
    uint32_t sector_count;
    device_t *drive;
};

typedef struct ata_command_t ata_command_t;
struct ata_command_t
{
    uint8_t count;
    uint32_t block;
    uint8_t command;
};

typedef struct atapi_packet_t atapi_packet_t;
struct atapi_packet_t
{
    ata_drive_t *drive;
    uint8_t packet[12];
    void *buffer;
    uint32_t block;
    uint8_t count;
};

typedef struct ata_ctrlreq_t ata_ctrlreq_t;
struct ata_ctrlreq_t
{
    request_t header;
    union
    {
	struct
	{
	    ata_drive_t *drive;
	    ata_command_t cmd;
	    void *buffer;
	} ata_command;

	atapi_packet_t atapi_packet;
    } params;
};

#define ATA_COMMAND	   REQUEST_CODE(0, 0, 'a', 'c')
#define ATAPI_PACKET	REQUEST_CODE(0, 0, 'a', 'p')

unsigned num_controllers;
uint8_t bios_params[16];

void nsleep(unsigned ns)
{
}

void AtaReset(ata_ctrl_t *ctrl)
{
    out(ctrl->base + REG_CTL, CTL_RESET | CTL_INTDISABLE);
    nsleep(400);
    out(ctrl->base + REG_CTL, 0);
}

void AtaNeedReset(ata_ctrl_t *ctrl)
{
    ctrl->need_reset = true;
}

bool AtaWaitForStatus(ata_ctrl_t *ctrl, unsigned mask, unsigned value)
{
/* Wait until controller is in the required state.  Return zero on timeout. */

    unsigned time_end, status;

    time_end = SysUpTime() + 5000;
    do
    {
	status = in(ctrl->base + REG_STATUS);
	if ((status & mask) == value)
	    return true;
    } while (SysUpTime() < time_end);

    AtaNeedReset(ctrl);    /* Controller gone deaf. */
    return false;
}

bool AtaIssueCommand(ata_drive_t *drive, ata_command_t *cmd)
{
    uint8_t sector, cyl_lo, cyl_hi, ldh;
    ata_ctrl_t *ctrl;

    ctrl = drive->ctrl;
    ldh = drive->ldhpref;
    if (drive->ldhpref & LDH_LBA)
    {
	sector = (cmd->block >>  0) & 0xFF;
	cyl_lo = (cmd->block >>  8) & 0xFF;
	cyl_hi = (cmd->block >> 16) & 0xFF;
	ldh |= ((cmd->block >> 24) & 0xF);
    }
    else if (drive->pheads && drive->psectors)
    {
	unsigned secspcyl, cylinder, head;

	secspcyl = drive->pheads * drive->psectors;
	cylinder = cmd->block / secspcyl;
	head = (cmd->block % secspcyl) / drive->psectors;
	sector = (cmd->block % drive->psectors) + 1;
	cyl_lo = cylinder;
	cyl_hi = cylinder >> 8;
	ldh |= head;
    }
    else
	sector = cyl_lo = cyl_hi = 0;

    if (ctrl->need_reset)
	AtaReset(ctrl);

    /* Output the command block to the winchester controller and return status */

    if (!waitfor(ctrl, STATUS_BSY, 0))
    {
	wprintf(L"AtaIssueCommand: controller not ready\n");
	return false;
    }
    
    /* Select drive. */
    out(ctrl->base + REG_LDH, ldh);

    if (!waitfor(ctrl, STATUS_BSY, 0))
    {
	wprintf(L"AtaIssueCommand: drive not ready\n");
	return false;
    }

    out(ctrl->base + REG_CTL, /*ctrl->pheads >= 8 ? */CTL_EIGHTHEADS/* : 0*/);
    out(ctrl->base + REG_PRECOMP, 0);
    out(ctrl->base + REG_COUNT, cmd->count);
    out(ctrl->base + REG_SECTOR, sector);
    out(ctrl->base + REG_CYL_LO, cyl_lo);
    out(ctrl->base + REG_CYL_HI, cyl_hi);
    SemAcquire(&ctrl->sem);
    out(ctrl->base + REG_COMMAND, cmd->command);
    ctrl->command = cmd->command;
    ctrl->status = STATUS_BSY;
    SemRelease(&ctrl->sem);
    return true;
}

/*
    ATA_REG_STAT & 0x08 (DRQ)	 ATA_REG_REASON        "phase"
    0				 0		      ATAPI_PH_ABORT
    0				 1		      bad
    0				 2		      bad
    0				 3		      ATAPI_PH_DONE
    8				 0		      ATAPI_PH_DATAOUT
    8				 1		      ATAPI_PH_CMDOUT
    8				 2		      ATAPI_PH_DATAIN
    8				 3		      bad
b0 of ATA_REG_REASON is C/nD (0=data, 1=command)
b1 of ATA_REG_REASON is IO (0=out to drive, 1=in from drive)
*/

bool AtapiIssuePacket(ata_drive_t *drive, const uint8_t *pkt)
{
    ata_ctrl_t *ctrl = drive->ctrl;
    unsigned i;

    if (!waitfor(ctrl, STATUS_BSY, 0))
    {
	wprintf(L"AtaIssuePacket: controller not ready\n");
	return false;
    }
    
    out(ctrl->base + REG_LDH, LDH_LBA | drive->ldhpref);
    
    if (!waitfor(ctrl, STATUS_BSY, 0))
    {
	wprintf(L"AtaIssuePacket: drive not ready\n");
	return false;
    }

    out(ctrl->base + REG_PRECOMP, 0);
    out(ctrl->base + REG_COUNT, 0);
    out(ctrl->base + REG_SECTOR, 0);
    out(ctrl->base + REG_CYL_LO, 0);
    out(ctrl->base + REG_CYL_HI, 0x80);

    SemAcquire(&ctrl->sem);
    out(ctrl->base + REG_COMMAND, CMD_PACKET);
    ctrl->command = CMD_PACKET;
    ctrl->status = STATUS_BSY;
    SemRelease(&ctrl->sem);

    for (i = 0; i < 6; i++)
	out16(ctrl->base + REG_DATA, ((uint16_t*) pkt)[i]);

    return true;
}

enum { wfiOk, wfiGeneric, wfiTimeout, wfiBadSector };

int AtaWaitForInterrupt(volatile ata_ctrl_t *ctrl)
{
    /* Wait for a task completion interrupt and return results. */

    int r;
    unsigned time_end;

    time_end = SysUpTime() + 10000;
    /* Wait for an interrupt that sets w_status to "not busy". */
    while (ctrl->status & STATUS_BSY)
    {
	ArchProcessorIdle();
	wprintf(L"%02x\b\b", ctrl->status);
	if (SysUpTime() > time_end)
	    return wfiTimeout;
    }

    /* Check status. */
    SemAcquire((semaphore_t*) &ctrl->sem);
    if ((ctrl->status & (STATUS_BSY | STATUS_RDY | STATUS_WF | STATUS_ERR))
			    == STATUS_RDY)
    {
	r = wfiOk;
	ctrl->status |= STATUS_BSY;    /* assume still busy with I/O */
    }
    else if ((ctrl->status & STATUS_ERR) && (in(ctrl->base + REG_ERROR) & ERROR_BB))
	r = wfiBadSector;    /* sector marked bad, retries won't help */
    else
	r = wfiGeneric;        /* any other error */
    
    SemRelease((semaphore_t*) &ctrl->sem);
    return r;
}

int AtaIssueSimpleCommand(ata_drive_t *drive, ata_command_t *cmd)
{
    /* A simple controller command, only one interrupt and no data-out phase. */
    int r;

    if (AtaIssueCommand(drive, cmd))
    {
	r = AtaWaitForInterrupt(drive->ctrl);

	if (r == wfiTimeout)
	    AtaReset(drive->ctrl);
    }
    else
	r = wfiGeneric;

    drive->ctrl->command = CMD_IDLE;
    return r;
}

void AtaServiceCtrlRequest(ata_ctrl_t *ctrl)
{
    asyncio_t *io = (asyncio_t*) ctrl->dev.io_first;

    if (io)
    {
	ata_ctrlreq_t *req = (ata_ctrlreq_t*) io->req;
	switch (req->header.code)
	{
	case ATA_COMMAND:
	    if (!AtaIssueCommand(req->params.ata_command.drive, 
		&req->params.ata_command.cmd))
		DevFinishIo(&ctrl->dev, io, EHARDWARE);
	    break;

	case ATAPI_PACKET:
	    if (!AtapiIssuePacket(req->params.atapi_packet.drive,
		req->params.atapi_packet.packet))
		DevFinishIo(&ctrl->dev, io, EHARDWARE);
	    break;

	default:
	    wprintf(L"AtaServiceCtrlRequest: invalid code (%4.4S)\n",
		&req->header.code);
	    DevFinishIo(&ctrl->dev, io, EINVALID);
	    break;
	}
    }
}

unsigned AtapiTransferCount(ata_ctrl_t *ctrl)
{
    unsigned RetVal;

    RetVal = in(ctrl->base + REG_COUNT_HI);
    RetVal <<= 8;
    RetVal |= in(ctrl->base + REG_COUNT_LO);
    return RetVal;
}

unsigned AtapiReadAndDiscard(ata_ctrl_t *ctrl, uint8_t *Buffer, unsigned Want)
{
    unsigned Count, Got, i;
    uint16_t *ptr;

    Got = AtapiTransferCount(ctrl);
    /*wprintf(L"atapiReadAndDiscard: Want %u bytes, Got %u\n", Want, Got);*/
    Count = min(Want, Got);

    for (i = 0, ptr = (uint16_t*) Buffer; i < Count; i += 2)
	*ptr++ = in16(ctrl->base + REG_DATA);
    
    if (Got > Count)
    {
	/* read only 16-bit words where possible */
	for (Count = Got - Count; Count > 1; Count -= 2)
	    in16(ctrl->base + REG_DATA);

	/* if the byte count is odd, read the odd byte last */
	if (Count != 0)
	    in(ctrl->base + REG_DATA);
    }
    else if (Count < Want)
	/* unexpectedly short read: decrease Want */
	Want = Got;

    return Want;
}

/* ATAPI data/command transfer 'phases' */
#define    ATAPI_PH_ABORT	 0    /* other possible phases */
#define    ATAPI_PH_DONE	3    /* (1, 2, 11) are invalid */
#define    ATAPI_PH_DATAOUT    8
#define    ATAPI_PH_CMDOUT	  9
#define    ATAPI_PH_DATAIN	  10

/******************************************************************************
 * AtaCtrl/Atapi
 */

bool AtapiPacketInterrupt(ata_ctrl_t *ctrl, ata_ctrlreq_t *req_ctrl)
{
    unsigned count, Phase;
    atapi_packet_t *ap;
    
    ap = &req_ctrl->params.atapi_packet;
    Phase = ctrl->status & 0x08;
    Phase |= (in(ctrl->base + REG_REASON) & 3);
    /*wprintf(L"ATAPI interrupt: phase = %u: ", Phase);*/
    
    switch (Phase)
    {
    case ATAPI_PH_ABORT:
	/* drive aborted the command */
	wprintf(L"error: drive aborted cmd\n");
	req_ctrl->header.result = EHARDWARE;
	return true;

    case ATAPI_PH_DATAIN:
	/* read data, advance pointers */
	count = AtapiReadAndDiscard(ctrl, ap->buffer, 0xFFFFu);
	ap->buffer = (uint8_t*) ap->buffer + count;
	
	/* XXX - count had better be a multiple of 2048... */
	count /= ATAPI_SECTOR_SIZE;
	ap->count -= count;
	ap->block += count;

	if (Phase == ATAPI_PH_DONE)
	{
	    /*wprintf(L"finished\n");*/
	    return true;
	}
	else
	{
	    /*wprintf(L"%u ", ap->block);*/
	    return false;
	}

	break;
    
    case ATAPI_PH_DONE:
	if (ap->count != 0)
	{
	    wprintf(L"error: data shortage (count = %u)\n", ap->count);
	    req_ctrl->header.result = EHARDWARE;
	}

	return true;
	
    default:
	/* ATAPI_PH_DATAOUT or ATAPI_PH_CMDOUT or something completely bogus */
	wprintf(L"error: bad phase %u\n", Phase);
	req_ctrl->header.result = EINVALID;
	return true;
    }
}

bool AtaCtrlIsr(device_t *dev, uint8_t irq)
{
    ata_ctrl_t *ctrl = (ata_ctrl_t*) dev;
    ata_ctrlreq_t *req_ctrl;
    asyncio_t *io;
    addr_t *ptr;
    uint8_t *buf;
    
    ctrl->status = in(ctrl->base + REG_STATUS);
    /*wprintf(L"AtaCtrlIsr: status = %x\n", ctrl->status);*/

    io = ctrl->dev.io_first;
    if (io != NULL)
    {
	bool finish;

	req_ctrl = (ata_ctrlreq_t*) io->req;
	finish = false;
	req_ctrl->header.result = 0;
	if (req_ctrl->header.code == ATA_COMMAND)
	{
	    ptr = (addr_t*) (io + 1);
	    
	    TRACE3("Finish ATA command: count = %u length = %u phys = %x ",
		req_ctrl->params.ata_command.cmd.count,
		io->length,
		*ptr);
	    
	    buf = MemMapTemp(ptr, io->length_pages, 
		PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);

	    assert(buf != NULL);
	    TRACE2("buf = %p + %x\n", buf, io->mod_buffer_start);
	    ins16(ctrl->base + REG_DATA, buf + io->mod_buffer_start, io->length);
	    
	    MemUnmapTemp();
	    finish = true;
	}
	else if (req_ctrl->header.code == ATAPI_PACKET)
	    finish = AtapiPacketInterrupt(ctrl, req_ctrl);
	else
	{
	    TRACE1("AtaCtrlIsr: got code %x\n", req_ctrl->header.code);
	    assert(req_ctrl->header.code == ATA_COMMAND ||
		req_ctrl->header.code == ATAPI_PACKET);
	}

	if (finish)
	{
	    ctrl->command = CMD_IDLE;
	    DevFinishIo(dev, io, 0);
	    AtaServiceCtrlRequest(ctrl);
	}
    }

    return true;
}

bool AtaCtrlRequest(device_t *dev, request_t *req)
{
    ata_ctrl_t *ctrl = (ata_ctrl_t*) dev;
    ata_ctrlreq_t *req_ctrl;
    asyncio_t *io;

    switch (req->code)
    {
    case DEV_REMOVE:
	free(ctrl);
	return true;

    case ATAPI_PACKET:
	req_ctrl = (ata_ctrlreq_t*) req;
	io = DevQueueRequest(dev, req, sizeof(ata_ctrlreq_t),
	    req_ctrl->params.atapi_packet.buffer, 
	    ATAPI_SECTOR_SIZE * req_ctrl->params.atapi_packet.count);
	if (ctrl->command == CMD_IDLE)
	    AtaServiceCtrlRequest(ctrl);
	return true;
    
    case ATA_COMMAND:
	req_ctrl = (ata_ctrlreq_t*) req;
	io = DevQueueRequest(dev, req, sizeof(ata_ctrlreq_t),
	    req_ctrl->params.ata_command.buffer,
	    SECTOR_SIZE * req_ctrl->params.ata_command.cmd.count);
	if (ctrl->command == CMD_IDLE)
	    AtaServiceCtrlRequest(ctrl);
	return true;
    }

    req->result = ENOTIMPL;
    return false;
}

/******************************************************************************
 * AtaDrive
 */

bool AtaDriveRequest(device_t *dev, request_t *req)
{
    ata_drive_t *drive = (ata_drive_t*) dev;
    ata_ctrlreq_t ctrl_req;
    request_dev_t *req_dev = (request_dev_t*) req;
    
    switch (req->code)
    {
    case DEV_READ:
    case DEV_WRITE:
	if (drive->is_atapi)
	{
	    uint8_t Pkt[12];
	    uint16_t Count;
	    addr_t block;

	    if (req->code != DEV_READ)
	    {
		req->result = EINVALID;
		return false;
	    }

	    memset(Pkt, 0, sizeof(Pkt));

	    block = req_dev->params.dev_read.offset / ATAPI_SECTOR_SIZE;
	    /* convert general block device command code into ATAPI packet commands */
	    Pkt[0] = ATAPI_CMD_READ10;
	    Pkt[2] = block >> 24;
	    Pkt[3] = block >> 16;
	    Pkt[4] = block >> 8;
	    Pkt[5] = block;

	    Count = req_dev->params.dev_read.length / ATAPI_SECTOR_SIZE;
	    /*Pkt[6] = Count >> 16;*/
	    Pkt[7] = Count >> 8;
	    Pkt[8] = Count;

	    ctrl_req.header.code = ATAPI_PACKET;
	    ctrl_req.params.atapi_packet.drive = drive;
	    ctrl_req.params.atapi_packet.count = Count;
	    ctrl_req.params.atapi_packet.block = block;
	    memcpy(ctrl_req.params.atapi_packet.packet, Pkt, sizeof(Pkt));
	    ctrl_req.params.atapi_packet.buffer = 
		req_dev->params.dev_read.buffer;
	}
	else
	{
	    ata_command_t cmd;

	    /*cmd.precomp = drive->precomp;*/
	    cmd.count = req_dev->params.dev_read.length / SECTOR_SIZE;
	    cmd.block = req_dev->params.dev_read.offset / SECTOR_SIZE;
	    cmd.command = req->code == DEV_WRITE ? CMD_WRITE : CMD_READ;

	    ctrl_req.header.code = ATA_COMMAND;
	    ctrl_req.params.ata_command.drive = drive;
	    ctrl_req.params.ata_command.cmd = cmd;
	    ctrl_req.params.ata_command.buffer = 
		req_dev->params.dev_read.buffer;
	}

	/*ctrl_req.header.original = req;*/
	ctrl_req.header.param = req;
	if (IoRequest(dev, &drive->ctrl->dev, &ctrl_req.header))
	{
	    /* Be sure to mirror SIOPENDING result to originator */
	    req->result = ctrl_req.header.result;
	    return true;
	}
	else
	{
	    req->result = ctrl_req.header.result;
	    return false;
	}
    }

    req->result = ENOTIMPL;
    return false;
}

void AtaDriveFinishIo(device_t *dev, request_t *req)
{
    assert(req->original != NULL);
    assert(req->param != NULL);
    wprintf(L"AtaDriveFinishIo: dev = %p, req = %p, req->from = %p\n"
	L"\treq->param = %p, req->param->from = %p\n", 
	dev, req, req->from, 
	req->param, ((request_t*) req->param)->from);
    if (req->param != req)
	IoNotifyCompletion(req->param);
}

static const device_vtbl_t ata_drive_vtbl =
{
    AtaDriveRequest,	/* request */
    NULL,		/* isr */
    AtaDriveFinishIo,	/* finishio */
};

/******************************************************************************
 * AtaPartition
 */

bool AtaPartitionRequest(device_t *dev, request_t *req)
{
    ata_part_t *part = (ata_part_t*) dev;
    request_dev_t *req_dev = (request_dev_t*) req;

    switch (req->code)
    {
    case DEV_READ:
    case DEV_WRITE:
	req_dev->params.dev_read.offset += part->start_sector * SECTOR_SIZE;

    default:
	return part->drive->vtbl->request(part->drive, req);
    }
}

static const device_vtbl_t ata_partition_vtbl =
{
    AtaPartitionRequest,
    NULL
};

void AtaPartitionDevice(device_t *dev, const wchar_t *base_name)
{
    wchar_t name[20], *suffix;
    unsigned i;
    ata_part_t *part;

    /*union
    {
	uint8_t bytes[512];
	struct
	{
	    uint8_t pad[512 - sizeof(partition_t) * 4 - 2];
	    partition_t parts[4];
	    uint16_t _55aa;
	} ptab;
    } sec0;*/

    uint8_t bytes[512];
    partition_t *parts;

    parts = (partition_t*) (bytes + 0x1be);
    TRACE1("bytes = %p\n", bytes);
    if (!IoReadSync(dev, 0, bytes, sizeof(bytes)))
	return;

    wcscpy(name, base_name);
    suffix = name + wcslen(base_name);
    suffix[1] = '\0';
    
    for (i = 0; i < 4; i++)
	if (parts[i].system != 0)
	{
	    part = malloc(sizeof(ata_part_t));
	    memset(part, 0, sizeof(ata_part_t));
	    
	    part->dev.vtbl = &ata_partition_vtbl;
	    part->dev.driver = dev->driver;
	    
	    part->start_sector = parts[i].start_sector_abs;
	    part->sector_count = parts[i].sector_count;
	    part->drive = dev;
	    wprintf(L"partition %c: start = %lu, count = %lu\n",
		i + 'a', part->start_sector, part->sector_count);

	    suffix[0] = i + 'a';
	    DevAddDevice(&part->dev, name, NULL);
	}
}

/******************************************************************************
 * Initialisation
 */

void _swab(const char *src, char *dest, int n)
{
    char temp;
    n &= -2;
    for (; n > 0; n -= 2, src += 2, dest += 2)
    {
	temp = dest[0];
	dest[0] = src[1];
	dest[1] = temp;
    }
}

void AtaFormatString(char *dest, const char *src, size_t count)
{
    char *ch;
    unsigned j;

    _swab(src, dest, count);
    ch = dest;
    for (j = 0; j < count; j++)
	if (dest[j] != ' ')
	    ch = dest + j + 1;
    if (ch != NULL &&
	ch < dest + count)
	*ch = '\0';
}

bool AtaInitController(ata_ctrl_t *ctrl)
{
#define id_byte(n)    (((uint8_t*) tmp_buf) + (n))
#define id_word(n)    (tmp_buf + (n))

    uint8_t r;
    unsigned i, j;
    /*uint16_t tmp_buf[256];*/
    ata_identify_t id;
    uint16_t *ptr;
    ata_command_t cmd;
    uint32_t size;
    wchar_t name[50];
    device_t *dev;
    
    wprintf(L"AtaInitController: initialising controller on %x\n", ctrl->base);

    /* Check for controller */
    AtaReset(ctrl);
    r = in(ctrl->base + REG_CYL_LO);
    out(ctrl->base + REG_CYL_LO, ~r);
    if (in(ctrl->base + REG_CYL_LO) == r)
    {
	wprintf(L"Controller not found: %x\n", r);
	/*return false;*/
    }

    DevRegisterIrq(AT_IRQ0, &ctrl->dev);
    ArchMaskIrq(1 << AT_IRQ0, 0);
    
    for (i = 0; i < DRIVES_PER_CONTROLLER; i++)
    {
	ctrl->drives[i].ctrl = ctrl;
	ctrl->drives[i].ldhpref = ldh_init(i);

	wprintf(L"Drive %u: ", i);

	cmd.command = ATA_IDENTIFY;
	ctrl->drives[i].is_atapi = false;
	if (AtaIssueSimpleCommand(ctrl->drives + i, &cmd) != wfiOk)
	{
	    cmd.command = ATAPI_IDENTIFY;
	    wprintf(L"trying ATAPI: ");

	    if (AtaIssueSimpleCommand(ctrl->drives + i, &cmd) != wfiOk)
	    {
		wprintf(L"not detected\n");
		continue;
	    }

	    ctrl->drives[i].is_atapi = true;
	}

	/* Device information. */
	ptr = (uint16_t*) &id;
	for (j = 0; j < 256; j++)
	    ptr[j] = in16(ctrl->base + REG_DATA);
	
	AtaFormatString(id.model, id.model, sizeof(id.model));
	AtaFormatString(id.serial, id.serial, sizeof(id.serial));
	
	/* Preferred CHS translation mode. */
	ctrl->drives[i].pcylinders = id.cylinders;
	ctrl->drives[i].pheads = id.heads;
	ctrl->drives[i].psectors = id.sectors;
	ctrl->drives[i].precomp = 0;
	size = (uint32_t) ctrl->drives[i].pcylinders 
	    * ctrl->drives[i].pheads 
	    * ctrl->drives[i].psectors;

	if (id.pio & 0x0200 && size > 512L*1024*2)
	{
	    /* Drive is LBA capable and is big enough to trust it to
	     * not make a mess of it.
	     */
	    wprintf(L"LBA ");
	    ctrl->drives[i].ldhpref |= LDH_LBA;
	    size = id.capacity;
	}

	if (ctrl->drives[i].lcylinders == 0)
	{
	    /* No BIOS parameters?  Then make some up. */
	    ctrl->drives[i].lcylinders = ctrl->drives[i].pcylinders;
	    ctrl->drives[i].lheads = ctrl->drives[i].pheads;
	    ctrl->drives[i].lsectors = ctrl->drives[i].psectors;
	    while (ctrl->drives[i].lcylinders > 1024)
	    {
		ctrl->drives[i].lheads *= 2;
		ctrl->drives[i].lcylinders /= 2;
	    }
	}

	wprintf(L"CHS = %u:%u:%u size = %luMB \"%.40S\" \"%.20S\"\n",
	    ctrl->drives[i].pcylinders,
	    ctrl->drives[i].pheads,
	    ctrl->drives[i].psectors,
	    size / 2048,
	    id.model,
	    id.serial);

	swprintf(name, IDE_BASE_NAME L"%u", 
	    (num_controllers - 1) * DRIVES_PER_CONTROLLER + i);
	swprintf(ctrl->drives[i].info.description, L"%.40S %.20S (%uMB)", 
	    id.model, id.serial,
	    size / 2048);
	ctrl->drives[i].info.device_class = 0;

	ctrl->drives[i].dev.vtbl = &ata_drive_vtbl;
	ctrl->drives[i].dev.driver = ctrl->dev.driver;
	ctrl->drives[i].dev.info = &ctrl->drives[i].info;
	ctrl->drives[i].ctrl = ctrl;
	/*dev = CcInstallBlockCache(&ctrl->drives[i].dev, ctrl->drives[i].is_atapi ? ATAPI_SECTOR_SIZE : SECTOR_SIZE);*/
	dev = &ctrl->drives[i].dev;
	DevAddDevice(dev, name, NULL);

	if (!ctrl->drives[i].is_atapi)
	    AtaPartitionDevice(dev, name);
    }

    return true;
}

#pragma pack(push, 1)
typedef struct
{
    uint16_t cylinders;
    uint8_t heads;
    uint16_t starting_reduced_write_current_cylinder;
    uint16_t starting_write_precomp_cylinder;
    uint8_t maximum_ECC_data_burst_length;
    uint8_t flags;
} bios_params_t;
#pragma pack(pop)

static const device_vtbl_t ata_controller_vtbl =
{
    AtaCtrlRequest,
    AtaCtrlIsr
};

device_t* AtaAddController(driver_t *drv, const wchar_t *name, 
			   device_config_t *cfg)
{
    ata_ctrl_t* ctrl;
    addr_t phys;
    uint32_t *bda;
    unsigned num_bios_drives;
    /*unsigned mod;
    uint16_t vector[2];
    uint8_t *buf;*/

    phys = 0;
    bda = MemMapTemp(&phys, 1, PRIV_KERN | PRIV_RD | PRIV_PRES);
    assert(bda != NULL);
    num_bios_drives = bda[0x475];
    TRACE1("ata: num_bios_drives = %u\n", num_bios_drives);
    
    /*memcpy(vector, ((uint32_t*) bda)[0x41], sizeof(vector));
    phys = vector[1] << 16 | vector[0];
    mod = phys % PAGE_SIZE;
    phys = PAGE_ALIGN_DOWN(phys);
    buf = MemMapTemp(&phys, 
	mod > PAGE_SIZE - sizeof(bios_params_t) ? 2 : 1,
	PRIV_KERN | PRIV_RD | PRIV_PRES);

    memcpy(vector, ((uint32_t*) bda)[0x46], sizeof(vector));*/
    MemUnmapTemp();

    if (/*num_bios_drives > 0*/ true)
    {
	ctrl = malloc(sizeof(ata_ctrl_t));
	ctrl->dev.vtbl = &ata_controller_vtbl;
	ctrl->dev.driver = drv;
	ctrl->dev.cfg = cfg;
	ctrl->base = 0x1F0;
	SemInit(&ctrl->sem);
	num_controllers++;

	if (!AtaInitController(ctrl))
	{
	    free(ctrl);
	    return NULL;
	}

	return &ctrl->dev;
    }
    else
	return NULL;
}

bool DrvInit(driver_t *drv)
{
    drv->add_device = AtaAddController;
    return true;
}