/* $Id: ata.c,v 1.1 2002/12/21 09:48:40 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/cache.h>
#include <kernel/io.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <os/syscall.h>

#include <errno.h>
#include <stdio.h>
#include <wchar.h>

/* I/O Ports used by winchester disk controllers. */

/* Read and write registers */
#define REG_BASE0               0x1F0   /* base register of controller 0 */
#define REG_BASE1               0x170   /* base register of controller 1 */
#define REG_DATA                0       /* data register (offset from the base reg.) */
#define REG_PRECOMP             1       /* start of write precompensation */
#define REG_COUNT               2       /* sectors to transfer */
#define REG_REASON              2
#define REG_SECTOR              3       /* sector number */
#define REG_CYL_LO              4       /* low byte of cylinder number */
#define REG_CYL_HI              5       /* high byte of cylinder number */
#define REG_COUNT_LO            REG_CYL_LO
#define REG_COUNT_HI            REG_CYL_HI
#define REG_LDH                 6       /* lba, drive and head */
#define   LDH_DEFAULT           0xA0    /* ECC enable, 512 bytes per sector */
#define   LDH_LBA               0x40    /* Use LBA addressing */
#define   ldh_init(drive)      (LDH_DEFAULT | ((drive) << 4))

/* Read only registers */
#define REG_STATUS              7    /* status */
#define   STATUS_BSY            0x80    /* controller busy */
#define   STATUS_RDY            0x40    /* drive ready */
#define   STATUS_WF             0x20    /* write fault */
#define   STATUS_SC             0x10    /* seek complete (obsolete) */
#define   STATUS_DRQ            0x08    /* data transfer request */
#define   STATUS_CRD            0x04    /* corrected data */
#define   STATUS_IDX            0x02    /* index pulse */
#define   STATUS_ERR            0x01    /* error */
#define REG_ERROR               1       /* error code */
#define   ERROR_BB              0x80    /* bad block */
#define   ERROR_ECC             0x40    /* bad ecc bytes */
#define   ERROR_ID              0x10    /* id not found */
#define   ERROR_AC              0x04    /* aborted command */
#define   ERROR_TK              0x02    /* track zero error */
#define   ERROR_DM              0x01    /* no data address mark */

/* ATAPI error codes */
#define   ERROR_ATAPI_MCR       0x08    /* media change requested */
#define   ERROR_ATAPI_AC        ERROR_AC
#define   ERROR_ATAPI_EOM       0x02    /* end of media detected */
#define   ERROR_ATAPI_ILI       0x01    /* illegal length indication */

/* Table 140, INF-8020 */
#define ATAPI_SENSE_NO_SENSE            0
#define ATAPI_SENSE_RECOVERED_ERROR     1
#define ATAPI_SENSE_NOT_READY           2
#define ATAPI_SENSE_MEDIUM_ERROR        3
#define ATAPI_SENSE_HARDWARE_ERROR      4
#define ATAPI_SENSE_ILLEGAL_REQUEST     5
#define ATAPI_SENSE_UNIT_ATTENTION      6
#define ATAPI_SENSE_ABORTED_COMMAND     11
#define ATAPI_SENSE_MISCOMPARE          12

/* Write only registers */
#define REG_COMMAND             7       /* command */
#define   CMD_IDLE              0x00    /* for w_command: drive idle */
#define   CMD_RECALIBRATE       0x10    /* recalibrate drive */
#define   CMD_READ              0x20    /* read data */
#define   CMD_WRITE             0x30    /* write data */
#define   CMD_READVERIFY        0x40    /* read verify */
#define   CMD_FORMAT            0x50    /* format track */
#define   CMD_SEEK              0x70    /* seek cylinder */
#define   CMD_DIAG              0x90    /* execute device diagnostics */
#define   CMD_SPECIFY           0x91    /* specify parameters */
#define   CMD_PACKET            0xA0    /* ATAPI packet cmd */
#define   ATAPI_IDENTIFY        0xA1    /* ATAPI identify */
#define   ATA_IDENTIFY          0xEC    /* identify drive */
#define REG_CTL                 0x206   /* control register */
#define   CTL_NORETRY           0x80    /* disable access retry */
#define   CTL_NOECC             0x40    /* disable ecc retry */
#define   CTL_EIGHTHEADS        0x08    /* more than eight heads */
#define   CTL_RESET             0x04    /* reset controller */
#define   CTL_INTDISABLE        0x02    /* disable interrupts */

/* ATAPI packet command bytes */
#define    ATAPI_CMD_START_STOP 0x1B    /* eject/load */
#define    ATAPI_CMD_READ10     0x28    /* read data sector(s) */
#define    ATAPI_CMD_READTOC    0x43    /* read audio table-of-contents */
#define    ATAPI_CMD_PLAY       0x47    /* play audio */
#define    ATAPI_CMD_PAUSE      0x4B    /* pause/continue audio */

/* Interrupt request lines. */
#define AT_IRQ0                 14      /* interrupt number for controller 0 */
#define AT_IRQ1                 15      /* interrupt number for controller 1 */

#define ATA_TIMEOUT             2000
#define ATA_TIMEOUT_IDENTIFY    500

#define SECTOR_SIZE             512
#define ATAPI_SECTOR_SIZE       2048
#define DRIVES_PER_CONTROLLER   2
//#define IDE_BASE_NAME           L"ide"

static const wchar_t *ata_drive_names[DRIVES_PER_CONTROLLER] =
{
    L"master",
    L"slave"
};

#define waitfor(ctrl, mask, value)    \
    ((in(ctrl->base + REG_STATUS) & (mask)) == (value) \
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
    unsigned rw_multiple;
    unsigned retries;
    device_config_t cfg;
};

typedef struct ata_ctrl_t ata_ctrl_t;
struct ata_ctrl_t
{
    device_t dev;
    uint16_t base;
    spinlock_t sem;
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
    uint16_t config;            /* obsolete stuff */
    uint16_t cylinders;         /* logical cylinders */
    uint16_t _reserved_2;
    uint16_t heads;             /* logical heads */
    uint16_t _vendor_4;
    uint16_t _vendor_5;
    uint16_t sectors;           /* logical sectors */
    uint16_t _vendor_7;
    uint16_t _vendor_8;
    uint16_t _vendor_9;
    char serial[20];            /* serial number */
    uint16_t _vendor_20;
    uint16_t _vendor_21;
    uint16_t vend_bytes_long;   /* no. vendor bytes on long cmd */
    char firmware[8];
    char model[40];
    uint16_t mult_support;      /* vendor stuff and multiple cmds */
    uint16_t _reserved_48;
    uint16_t capabilities;
    uint16_t _reserved_50;
    uint16_t pio;
    uint16_t dma;
    uint16_t _reserved_53;
    uint16_t curr_cyls;         /* current logical cylinders */
    uint16_t curr_heads;        /* current logical heads */
    uint16_t curr_sectors;      /* current logical sectors */
    uint32_t capacity;          /* capacity in sectors */
    uint16_t _pad[256-59];      /* don't need this stuff for now */
};

#pragma pack(pop)

CASSERT(sizeof(ata_identify_t) == 512);

typedef struct ata_part_t ata_part_t;
struct ata_part_t
{
    device_t dev;
    uint32_t start_sector;
    uint32_t sector_count;
    device_t *drive;
    device_config_t cfg;
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
    page_array_t *pages;
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
            page_array_t *pages;
        } ata_command;

        atapi_packet_t atapi_packet;
    } params;
};

#define ATA_COMMAND     REQUEST_CODE(0, 0, 'a', 'c')
#define ATAPI_PACKET    REQUEST_CODE(0, 0, 'a', 'p')

unsigned num_controllers;

typedef struct ata_ioextra_t ata_ioextra_t;
struct ata_ioextra_t
{
    size_t bytes_this_mult;
};

__inline void ins16(uint16_t port, void *buf, size_t length)
{
    unsigned i;
    for (i = 0; i < length / 2; i++)
        *((uint16_t*) buf + i) = in16(port);
}

__inline void outs16(uint16_t port, const void *buf, size_t length)
{
    unsigned i;
    const uint16_t *ptr;
    ptr = buf;
    for (i = 0; i < length / 2; i++)
        out16(port, ptr[i]);
}

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
    out(ctrl->base + REG_COUNT, min(cmd->count, drive->rw_multiple / SECTOR_SIZE));
    out(ctrl->base + REG_SECTOR, sector);
    out(ctrl->base + REG_CYL_LO, cyl_lo);
    out(ctrl->base + REG_CYL_HI, cyl_hi);
    SpinAcquire(&ctrl->sem);
    out(ctrl->base + REG_COMMAND, cmd->command);
    ctrl->command = cmd->command;
    ctrl->status = STATUS_BSY;
    SpinRelease(&ctrl->sem);

    return true;
}

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
    drive->retries = 0;

    SpinAcquire(&ctrl->sem);
    ctrl->command = CMD_PACKET;
    ctrl->status = STATUS_BSY;
    out(ctrl->base + REG_COMMAND, CMD_PACKET);
    SpinRelease(&ctrl->sem);

    if (!waitfor(ctrl, STATUS_BSY | STATUS_DRQ, STATUS_DRQ))
    {
        wprintf(L"AtapiIssuePacket: drive not responding to command\n");
        return false;
    }

    for (i = 0; i < 6; i++)
        out16(ctrl->base + REG_DATA, ((uint16_t*) pkt)[i]);

    return true;
}

enum { wfiOk, wfiGeneric, wfiTimeout, wfiBadSector };

int AtaWaitForInterrupt(volatile ata_ctrl_t *ctrl, unsigned timeout)
{
    /* Wait for a task completion interrupt and return results. */

    int r;
    unsigned time_end;

    time_end = SysUpTime() + timeout;
    /* Wait for an interrupt that sets w_status to "not busy". */
    while (ctrl->status & STATUS_BSY)
    {
        ArchProcessorIdle();
        wprintf(L"%02x\b\b", ctrl->status);
        if (SysUpTime() > time_end)
            return wfiTimeout;
    }

    /* Check status. */
    SpinAcquire((spinlock_t*) &ctrl->sem);
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
    
    SpinRelease((spinlock_t*) &ctrl->sem);
    return r;
}

int AtaIssueSimpleCommand(ata_drive_t *drive, ata_command_t *cmd, unsigned timeout)
{
    /* A simple controller command, only one interrupt and no data-out phase. */
    int r;

    if (AtaIssueCommand(drive, cmd))
    {
        r = AtaWaitForInterrupt(drive->ctrl, timeout);

        if (r == wfiTimeout)
            AtaReset(drive->ctrl);
    }
    else
        r = wfiGeneric;

    drive->ctrl->command = CMD_IDLE;
    return r;
}

void AtaDoWrite(ata_ctrl_t *ctrl, ata_ctrlreq_t *req, asyncio_t *io)
{
    uint8_t *buf;
    ata_ioextra_t *extra;

    assert(req->header.code == ATA_COMMAND);
    assert(req->params.ata_command.cmd.command == CMD_WRITE);

    extra = io->extra;
    TRACE3("AtaDoWrite: %u bytes at block %u: bytes_this_mult = %u\n",
        SECTOR_SIZE, req->params.ata_command.cmd.block, extra->bytes_this_mult);
    buf = DevMapBuffer(io);
    assert(buf != NULL);

    outs16(ctrl->base + REG_DATA, buf + io->length, SECTOR_SIZE);
    DevUnmapBuffer();

    io->length += SECTOR_SIZE;
    req->params.ata_command.cmd.block++;

    extra->bytes_this_mult += SECTOR_SIZE;
}

void AtaServiceCtrlRequest(ata_ctrl_t *ctrl)
{
    asyncio_t *io = (asyncio_t*) ctrl->dev.io_first;
    status_t result;
    ata_ctrlreq_t *req;
    ata_ioextra_t *extra;

    result = EHARDWARE;
    if (io)
    {
        req = (ata_ctrlreq_t*) io->req;
        extra = io->extra;
        switch (req->header.code)
        {
        case ATA_COMMAND:
            if (!AtaIssueCommand(req->params.ata_command.drive, 
                &req->params.ata_command.cmd))
                goto failure;

            extra->bytes_this_mult = 0;
            if (req->params.ata_command.cmd.command == CMD_WRITE)
            {
                if (!waitfor(ctrl, STATUS_BSY, 0))
                {
                    wprintf(L"AtaIssueCommand: drive not ready for write data\n");
                    goto failure;
                }

                if (in(ctrl->base + REG_STATUS) & STATUS_ERR)
                {
                    wprintf(L"AtaIssueCommand: drive error = %x\n", in(ctrl->base + REG_ERROR));
                    goto failure;
                }

                AtaDoWrite(ctrl, req, io);
            }

            break;

        case ATAPI_PACKET:
            if (!AtapiIssuePacket(req->params.atapi_packet.drive,
                req->params.atapi_packet.packet))
            {
                result = EHARDWARE;
                goto failure;
            }
            break;

        default:
            wprintf(L"AtaServiceCtrlRequest: invalid code (%4.4S)\n",
                &req->header.code);
            result = EINVALID;
            goto failure;
        }
    }

    return;

failure:
    free(extra);
    DevFinishIo(&ctrl->dev, io, result);
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
    uint16_t *ptr, w;

    Got = AtapiTransferCount(ctrl);
    //wprintf(L"AtapiReadAndDiscard: Want %u bytes, Got %u\n", Want, Got);
    Count = min(Want, Got);

    for (i = 0, ptr = (uint16_t*) Buffer; i < Count; i += 2)
    {
        w = in16(ctrl->base + REG_DATA);
        if (ptr != NULL)
            *ptr++ = w;
    }
    
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

/*
    ATA_REG_STAT & 0x08 (DRQ)    ATA_REG_REASON        "phase"
    0                            0                    ATAPI_PH_ABORT
    0                            1                    bad
    0                            2                    bad
    0                            3                    ATAPI_PH_DONE
    8                            0                    ATAPI_PH_DATAOUT
    8                            1                    ATAPI_PH_CMDOUT
    8                            2                    ATAPI_PH_DATAIN
    8                            3                    bad
b0 of ATA_REG_REASON is C/nD (0=data, 1=command)
b1 of ATA_REG_REASON is IO (0=out to drive, 1=in from drive)
*/

/* ATAPI data/command transfer 'phases' */
#define    ATAPI_PH_ABORT       0    /* other possible phases */
#define    ATAPI_PH_DONE        3    /* (1, 2, 11) are invalid */
#define    ATAPI_PH_DATAOUT     8
#define    ATAPI_PH_CMDOUT      9
#define    ATAPI_PH_DATAIN      10

/******************************************************************************
 * AtaCtrl/Atapi
 */

bool AtapiPacketInterrupt(ata_ctrl_t *ctrl, asyncio_t *io, 
                          ata_ctrlreq_t *req_ctrl)
{
    unsigned count, Phase;
    atapi_packet_t *ap;
    uint8_t *buffer;

    ap = &req_ctrl->params.atapi_packet;
    Phase = ctrl->status & STATUS_DRQ;
    Phase |= (in(ctrl->base + REG_REASON) & 3);
    wprintf(L"[%u]", Phase);

    if (ctrl->status & STATUS_ERR)
    {
        count = AtapiTransferCount(ctrl);
        wprintf(L"AtapiPacketInterrupt: error: count = %u, status = %x, retries = %u, error = %x\n", 
            count, ctrl->status, ap->drive->retries, in(ctrl->base + REG_ERROR));

        ap->drive->retries++;
        if (ap->drive->retries >= 3)
        {
            req_ctrl->header.result = EHARDWARE;
            return true;
        }
        else
        {
            AtaServiceCtrlRequest(ctrl);
            return false;
        }
    }

    switch (Phase)
    {
    case ATAPI_PH_ABORT:
        /* drive aborted the command */
        wprintf(L"error: drive aborted cmd\n");
        req_ctrl->header.result = EHARDWARE;
        return true;

    case ATAPI_PH_DONE:
        if (ap->count == 0)
        {
            //wprintf(L"AtapiPacketInterrupt: finished\n");
            return true;
        }

        /* fall through */

    case ATAPI_PH_DATAIN:
        /* read data, advance pointers */
        ap->drive->retries = 0;

        count = AtapiTransferCount(ctrl);
        if (io->length + count <= io->pages->num_pages * PAGE_SIZE)
        {
            buffer = MemMapPageArray(io->pages, PRIV_KERN | PRIV_PRES | PRIV_WR);
            wprintf(L"buffer = %p io->length = %u count = %u\n", buffer, io->length, count);
            count = AtapiReadAndDiscard(ctrl, buffer + io->length, 0xFFFFu);
            //wprintf(L"ATAPI_PH_DATAIN: %u bytes\n", count);
            MemUnmapTemp();
        }
        else
        {
            count = AtapiReadAndDiscard(ctrl, NULL, 0xFFFFu);
            wprintf(L"ATAPI_PH_DATAIN: excess data (%u bytes)\n", count);
        }

        io->length += count;

        /* XXX - count had better be a multiple of 2048... */
        count /= ATAPI_SECTOR_SIZE;
        ap->count -= count;
        ap->block += count;

        if (Phase == ATAPI_PH_DONE)
        {
            wprintf(L"finished, %u bytes read\n", io->length);
            return true;
        }
        else
        {
            wprintf(L"%u ", ap->block);
            return false;
        }

        break;

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
    uint8_t *buf;
    size_t mult, total;
    ata_ioextra_t *extra;
    
    ctrl->status = in(ctrl->base + REG_STATUS);
    /*wprintf(L"AtaCtrlIsr: status = %x\n", ctrl->status);*/

    io = ctrl->dev.io_first;
    if (io != NULL)
    {
        bool finish;

        extra = io->extra;
        req_ctrl = (ata_ctrlreq_t*) io->req;
        finish = false;
        req_ctrl->header.result = 0;
        if (ctrl->status & STATUS_ERR)
        {
            wprintf(L"AtaCtrlIsr: drive error %x\n", in(ctrl->base + REG_ERROR));
            req_ctrl->header.result = EHARDWARE;
            finish = true;
        }
        else if (req_ctrl->header.code == ATA_COMMAND)
        {
            total = req_ctrl->params.ata_command.cmd.count * SECTOR_SIZE;
            mult = min(req_ctrl->params.ata_command.drive->rw_multiple, 
                total - io->length);

#ifdef DEBUG
            //if (req_ctrl->params.ata_command.cmd.command == CMD_WRITE)
                wprintf(L"ATA: count = %u(%u) length = %u length_pages = %u phys = %x ",
                    req_ctrl->params.ata_command.cmd.count,
                    mult,
                    total,
                    io->pages->num_pages,
                    io->pages->pages[PAGE_ALIGN(io->length) / PAGE_SIZE]);
#endif

            if (req_ctrl->params.ata_command.cmd.command == CMD_READ)
            {
                buf = DevMapBuffer(io);
                assert(buf != NULL);

                //TRACE1("buf = %p: ", buf);
                ins16(ctrl->base + REG_DATA, buf + io->length, SECTOR_SIZE);
                DevUnmapBuffer();

                io->length += SECTOR_SIZE;
                req_ctrl->params.ata_command.cmd.block++;
                extra->bytes_this_mult += SECTOR_SIZE;
            }

            if (io->length >= total)
            {
                /* This chunk too us to the end of the request */

                //if (req_ctrl->params.ata_command.cmd.command == CMD_WRITE)
                    TRACE0("finished\n");
                finish = true;
                req_ctrl->header.result = 0;
            }
            else
            {
                size_t bytes_to_go;

                bytes_to_go = req_ctrl->params.ata_command.cmd.count * SECTOR_SIZE - io->length;
                //if (req_ctrl->params.ata_command.cmd.command == CMD_WRITE)
                    TRACE1("%u bytes to go\n", bytes_to_go);
                finish = false;

                if (extra->bytes_this_mult >= req_ctrl->params.ata_command.drive->rw_multiple)
                {
                    /* Finished all the sectors in this chunk: queue the next lot */
                    //if (req_ctrl->params.ata_command.cmd.command == CMD_WRITE)
                        TRACE0("AtaCtrlIsr: requeueing command\n");
                    AtaServiceCtrlRequest(ctrl);
                }
                else if (req_ctrl->params.ata_command.cmd.command == CMD_WRITE)
                {
                    /* Drive needs some more bytes */
                    AtaDoWrite(ctrl, req_ctrl, io);
                }
            }
        }
        else if (req_ctrl->header.code == ATAPI_PACKET)
            finish = AtapiPacketInterrupt(ctrl, io, req_ctrl);
        else
        {
            TRACE1("AtaCtrlIsr: got code %x\n", req_ctrl->header.code);
            assert(req_ctrl->header.code == ATA_COMMAND ||
                req_ctrl->header.code == ATAPI_PACKET);
        }

        if (finish)
        {
            ctrl->command = CMD_IDLE;
            free(extra);
            DevFinishIo(dev, io, req_ctrl->header.result);
            AtaServiceCtrlRequest(ctrl);
        }
    }
    else
        wprintf(L"AtaCtrlIsr: no requests queued, status = %x\n", ctrl->status);

    return true;
}

bool AtaCtrlRequest(device_t *dev, request_t *req)
{
    ata_ctrl_t *ctrl = (ata_ctrl_t*) dev;
    ata_ctrlreq_t *req_ctrl;
    asyncio_t *io;
    ata_ioextra_t *extra;

    switch (req->code)
    {
    case DEV_REMOVE:
        free(ctrl);
        return true;

    case ATAPI_PACKET:
        req_ctrl = (ata_ctrlreq_t*) req;
        io = DevQueueRequest(dev, req, sizeof(ata_ctrlreq_t),
            req_ctrl->params.atapi_packet.pages, 
            ATAPI_SECTOR_SIZE * req_ctrl->params.atapi_packet.count);
        io->length = 0;
        extra = malloc(sizeof(ata_ioextra_t));
        extra->bytes_this_mult = 0;
        io->extra = extra;
        if (ctrl->command == CMD_IDLE)
            AtaServiceCtrlRequest(ctrl);
        return true;

    case ATA_COMMAND:
        req_ctrl = (ata_ctrlreq_t*) req;
        io = DevQueueRequest(dev, req, sizeof(ata_ctrlreq_t),
            req_ctrl->params.ata_command.pages,
            SECTOR_SIZE * req_ctrl->params.ata_command.cmd.count);
        io->length = 0;
        extra = malloc(sizeof(ata_ioextra_t));
        extra->bytes_this_mult = 0;
        io->extra = extra;
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
    ata_ctrlreq_t *ctrl_req;
    request_dev_t *req_dev = (request_dev_t*) req;
    io_callback_t cb;
    
    switch (req->code)
    {
    case DEV_READ:
    case DEV_WRITE:
        ctrl_req = malloc(sizeof(ata_ctrlreq_t));
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

            wprintf(L"AtaDriveRequest(DEV_READ): atapi packet: block = %u count = %u\n",
                block, Count);
            ctrl_req->header.code = ATAPI_PACKET;
            ctrl_req->params.atapi_packet.drive = drive;
            ctrl_req->params.atapi_packet.count = Count;
            ctrl_req->params.atapi_packet.block = block;
            memcpy(ctrl_req->params.atapi_packet.packet, Pkt, sizeof(Pkt));
            ctrl_req->params.atapi_packet.pages = 
                req_dev->params.dev_read.pages;
        }
        else
        {
            ata_command_t cmd;

            /*cmd.precomp = drive->precomp;*/
            if (req->code == DEV_WRITE)
                wprintf(L"AtaDriveRequest: DEV_WRITE(%lx)\n",
                    (uint32_t) req_dev->params.dev_read.offset / SECTOR_SIZE);

            cmd.count = req_dev->params.dev_read.length / SECTOR_SIZE;
            cmd.block = req_dev->params.dev_read.offset / SECTOR_SIZE;
            cmd.command = (req->code == DEV_WRITE) ? CMD_WRITE : CMD_READ;

            ctrl_req->header.code = ATA_COMMAND;
            ctrl_req->params.ata_command.drive = drive;
            ctrl_req->params.ata_command.cmd = cmd;
            ctrl_req->params.ata_command.pages = 
                req_dev->params.dev_read.pages;
        }

        /*ctrl_req->header.original = req;*/
        ctrl_req->header.param = req;
        cb.type = IO_CALLBACK_DEVICE;
        cb.u.dev = dev;
        if (IoRequest(&cb, &drive->ctrl->dev, &ctrl_req->header))
        {
            /* Be sure to mirror SIOPENDING result to originator */
            req->result = ctrl_req->header.result;
            return true;
        }
        else
        {
            req->result = ctrl_req->header.result;
            return false;
        }
    }

    req->result = ENOTIMPL;
    return false;
}

void AtaDriveFinishIo(device_t *dev, request_t *req)
{
    request_t *originator;

    assert(req->param != NULL);

    originator = req->param;
    TRACE3("AtaDriveFinishIo(%p): callback = %u/%p\n",
        originator,
        originator->callback.type,
        originator->callback.u.function);

    IoNotifyCompletion(originator);
    free(req);
}

static const device_vtbl_t ata_drive_vtbl =
{
    AtaDriveRequest,    /* request */
    NULL,               /* isr */
    AtaDriveFinishIo,   /* finishio */
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
    case DEV_WRITE:
    case DEV_READ:
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

void AtaPartitionDevice(ata_drive_t *drive)
{
    wchar_t name[5];
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
    TRACE1("AtaPartitionDevice: bytes = %p\n", bytes);
    if (IoReadSync(&drive->dev, 0, bytes, sizeof(bytes)) == sizeof(bytes) &&
        bytes[510] == 0x55 &&
        bytes[511] == 0xaa)
    {
        TRACE0("AtaPartitionDevice: read finished\n");

        for (i = 0; i < 4; i++)
            if (parts[i].system != 0)
            {
                part = malloc(sizeof(ata_part_t));
                memset(part, 0, sizeof(ata_part_t));

                part->dev.vtbl = &ata_partition_vtbl;
                part->dev.driver = drive->dev.driver;

                part->start_sector = parts[i].start_sector_abs;
                part->sector_count = parts[i].sector_count;
                part->drive = &drive->dev;
                wprintf(L"partition %c: start = %lu, count = %lu\n",
                    i + 'a', part->start_sector, part->sector_count);

                swprintf(name, L"%u", i);

                part->cfg.parent = &drive->dev;
                part->cfg.device_class = 0x0081;
                DevAddDevice(&part->dev, name, &part->cfg);
            }
    }
    else
    {
        TRACE0("AtaPartitionDevice: read failed\n");

        part = malloc(sizeof(ata_part_t));
        memset(part, 0, sizeof(ata_part_t));

        part->dev.vtbl = &ata_partition_vtbl;
        part->dev.driver = drive->dev.driver;

        part->start_sector = 0;
        part->sector_count = drive->pcylinders 
            * drive->pheads 
            * drive->psectors;
        part->drive = &drive->dev;
        wprintf(L"partition a: start = %lu, count = %lu\n",
            part->start_sector, part->sector_count);

        part->cfg.parent = &drive->dev;
        part->cfg.device_class = 0x0081;
        DevAddDevice(&part->dev, L"0", &part->cfg);
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

static const device_vtbl_t ata_controller_vtbl =
{
    AtaCtrlRequest,
    AtaCtrlIsr
};

void AtaInitController(driver_t *drv, const wchar_t *name, uint16_t base, 
                       uint8_t irq, device_config_t *cfg)
{
#define id_byte(n)    (((uint8_t*) tmp_buf) + (n))
#define id_word(n)    (tmp_buf + (n))

    ata_ctrl_t *ctrl;
    uint8_t r;
    unsigned i, j;
    /*uint16_t tmp_buf[256];*/
    ata_identify_t id;
    uint16_t *ptr;
    ata_command_t cmd;
    uint32_t size;
    wchar_t drive_name[50];

    ctrl = malloc(sizeof(ata_ctrl_t));
    if (ctrl == NULL)
        return;

    memset(ctrl, 0, sizeof(ata_ctrl_t));
    ctrl->dev.vtbl = &ata_controller_vtbl;
    ctrl->dev.driver = drv;
    ctrl->dev.cfg = cfg;
    ctrl->base = base;
    SpinInit(&ctrl->sem);
    num_controllers++;

    wprintf(L"AtaInitController: initialising controller on %x\n", ctrl->base);

    /* Check for controller */
    AtaReset(ctrl);
    r = in(ctrl->base + REG_CYL_LO);
    out(ctrl->base + REG_CYL_LO, ~r);
    if (in(ctrl->base + REG_CYL_LO) == r)
    {
        wprintf(L"Controller at %x not found: %x\n", base, r);
        /*return false;*/
    }

    DevRegisterIrq(irq, &ctrl->dev);
    ArchMaskIrq(1 << irq, 0);
    DevAddDevice(&ctrl->dev, name, cfg);

    for (i = 0; i < DRIVES_PER_CONTROLLER; i++)
    {
        ctrl->drives[i].ctrl = ctrl;
        ctrl->drives[i].ldhpref = ldh_init(i);

        wprintf(L"Drive %u: ", i);

        cmd.command = ATA_IDENTIFY;
        ctrl->drives[i].is_atapi = false;
        if (AtaIssueSimpleCommand(ctrl->drives + i, &cmd, 
            ATA_TIMEOUT_IDENTIFY) != wfiOk)
        {
            cmd.command = ATAPI_IDENTIFY;
            wprintf(L"trying ATAPI: ");

            if (AtaIssueSimpleCommand(ctrl->drives + i, &cmd,
                ATA_TIMEOUT_IDENTIFY) != wfiOk)
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

        /* xxx - handle id.mult_support later */
        if (ctrl->drives[i].is_atapi)
            ctrl->drives[i].rw_multiple = ATAPI_SECTOR_SIZE;
        else
        {
            /*id.mult_support = 2;*/
            if ((id.mult_support & 0xff) != 0)
                ctrl->drives[i].rw_multiple = (id.mult_support & 0xff) * SECTOR_SIZE;
            else
                ctrl->drives[i].rw_multiple = SECTOR_SIZE;
        }

        wprintf(L"CHS = %u:%u:%u size = %luMB \"%.40S\" \"%.20S\" mult_support = %u\n",
            ctrl->drives[i].pcylinders,
            ctrl->drives[i].pheads,
            ctrl->drives[i].psectors,
            size / 2048,
            id.model,
            id.serial,
            id.mult_support & 0xff);

        wcscpy(drive_name, ata_drive_names[i]);
        swprintf(ctrl->drives[i].info.description, L"%.40S %.20S (%uMB)", 
            id.model, id.serial,
            size / 2048);
        ctrl->drives[i].info.device_class = 0;

        ctrl->drives[i].dev.vtbl = &ata_drive_vtbl;
        ctrl->drives[i].dev.driver = ctrl->dev.driver;
        /*ctrl->drives[i].dev.info = &ctrl->drives[i].info;*/
        ctrl->drives[i].ctrl = ctrl;

        ctrl->drives[i].cfg.device_class = 0x0180;
        ctrl->drives[i].cfg.parent = &ctrl->dev;

        DevAddDevice(&ctrl->drives[i].dev, drive_name, &ctrl->drives[i].cfg);
        AtaPartitionDevice(&ctrl->drives[i]);
    }
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

void AtaAddController(driver_t *drv, const wchar_t *name, 
                      device_config_t *cfg)
{
    /*if (cfg == NULL)
        wprintf(L"ata: no PnP configuration present\n");
    else
    {
        unsigned i;

        wprintf(L"ata: has %u PnP resources\n", cfg->num_resources);
        for (i = 0; i < cfg->num_resources; i++)
        {
            switch (cfg->resources[i].cls)
            {
            case resMemory:
                wprintf(L"\tmemory: 0x%x bytes at 0x%x\n", 
                    cfg->resources[i].u.memory.length,
                    cfg->resources[i].u.memory.base);
                break;

            case resIo:
                wprintf(L"\tio: %u ports at 0x%x\n", 
                    cfg->resources[i].u.io.length,
                    cfg->resources[i].u.io.base);
                break;

            case resIrq:
                wprintf(L"\tirq: %u\n", cfg->resources[i].u.irq);
                break;
            }
        }
    }*/

    uint16_t ports[] = { REG_BASE0, REG_BASE1 };
    uint8_t irqs[] = { AT_IRQ0, AT_IRQ1 };

    if (num_controllers < 2)
        AtaInitController(drv, name, ports[num_controllers], 
            irqs[num_controllers], cfg);
}

bool DrvInit(driver_t *drv)
{
    drv->add_device = AtaAddController;
    return true;
}
