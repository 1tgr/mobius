/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/syscall.h>
#include <kernel/debug.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <wchar.h>

#include "interface.h"
#include "hardware.h"

typedef struct ata_ctrl_t ata_ctrl_t;
struct ata_ctrl_t
{
    device_t *device;
    uint16_t base;
    spinlock_t sem;

	struct
	{
		device_t *device;
		ata_businfo_t businfo;
		dev_config_t config;
	} drives[DRIVES_PER_CONTROLLER];

    uint8_t command;
    uint8_t status;
    bool need_reset;
};


#define waitfor(ctrl, mask, value)    \
    ((in(ctrl->base + REG_STATUS) & (mask)) == (value) \
        || AtaWaitForStatus(ctrl, mask, value))

typedef struct ata_ioextra_t ata_ioextra_t;
struct ata_ioextra_t
{
    size_t bytes_this_mult;
};

static unsigned num_controllers;

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

    time_end = WrapSysUpTime() + 5000;
    do
    {
        status = in(ctrl->base + REG_STATUS);
        if ((status & mask) == value)
            return true;
    } while (WrapSysUpTime() < time_end);

    AtaNeedReset(ctrl);    /* Controller gone deaf. */
    return false;
}


bool AtaIssueCommand(ata_ctrl_t *ctrl, ata_command_t *cmd)
{
    uint8_t sector, cyl_lo, cyl_hi, ldh;

    ldh = cmd->businfo->ldhpref;
    if (cmd->businfo->ldhpref & LDH_LBA)
    {
        sector = (cmd->block >>  0) & 0xFF;
        cyl_lo = (cmd->block >>  8) & 0xFF;
        cyl_hi = (cmd->block >> 16) & 0xFF;
        ldh |= ((cmd->block >> 24) & 0xF);
    }
    else if (cmd->businfo->heads != 0 && 
		cmd->businfo->sectors != 0)
    {
        unsigned secspcyl, cylinder, head;

        secspcyl = cmd->businfo->heads * cmd->businfo->sectors;
        cylinder = cmd->block / secspcyl;
        head = (cmd->block % secspcyl) / cmd->businfo->sectors;
        sector = (cmd->block % cmd->businfo->sectors) + 1;
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
    
    /* Select cmd->businfo. */
    out(ctrl->base + REG_LDH, ldh);

    if (!waitfor(ctrl, STATUS_BSY, 0))
    {
        wprintf(L"AtaIssueCommand: cmd->businfo not ready\n");
        return false;
    }

    out(ctrl->base + REG_CTL, /*ctrl->pheads >= 8 ? */CTL_EIGHTHEADS/* : 0*/);
    out(ctrl->base + REG_PRECOMP, 0);
    out(ctrl->base + REG_COUNT, min(cmd->count, cmd->businfo->rw_multiple / SECTOR_SIZE));
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


bool AtapiIssuePacket(ata_ctrl_t *ctrl, atapi_packet_t *cmd)
{
    unsigned i;

    if (!waitfor(ctrl, STATUS_BSY, 0))
    {
        wprintf(L"AtaIssuePacket: controller not ready\n");
        return false;
    }

    out(ctrl->base + REG_LDH, LDH_LBA | cmd->businfo->ldhpref);

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
    cmd->businfo->retries = 0;

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
        out16(ctrl->base + REG_DATA, ((uint16_t*) cmd->packet)[i]);

    return true;
}

enum { wfiOk, wfiGeneric, wfiTimeout, wfiBadSector };

int AtaWaitForInterrupt(volatile ata_ctrl_t *ctrl, unsigned timeout)
{
    /* Wait for a task completion interrupt and return results. */

    int r;
    unsigned time_end;

    time_end = WrapSysUpTime() + timeout;
    /* Wait for an interrupt that sets w_status to "not busy". */
    while (ctrl->status & STATUS_BSY)
    {
        ArchProcessorIdle();
        //wprintf(L"%02x\b\b", ctrl->status);
        if (WrapSysUpTime() > time_end)
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


int AtaIssueSimpleCommand(ata_ctrl_t *ctrl, ata_command_t *cmd, unsigned timeout)
{
    /* A simple controller command, only one interrupt and no data-out phase. */
    int r;

    if (AtaIssueCommand(ctrl, cmd))
    {
        r = AtaWaitForInterrupt(ctrl, timeout);

        if (r == wfiTimeout)
            AtaReset(ctrl);
    }
    else
        r = wfiGeneric;

    ctrl->command = CMD_IDLE;
    return r;
}


void AtaDoWrite(ata_ctrl_t *ctrl, ata_ctrlreq_t *req, asyncio_t *io)
{
    uint8_t *buf;
    ata_ioextra_t *extra;

    assert(req->header.code == ATA_COMMAND);
    assert(req->params.ata_command.command == CMD_WRITE);

    extra = io->extra;
    TRACE3("AtaDoWrite: %u bytes at block %u: bytes_this_mult = %u\n",
        SECTOR_SIZE, req->params.ata_command.block, extra->bytes_this_mult);
    buf = MemMapPageArray(io->pages);
    assert(buf != NULL);

    outs16(ctrl->base + REG_DATA, buf + io->length, SECTOR_SIZE);
    MemUnmapPageArray(io->pages);

    io->length += SECTOR_SIZE;
    req->params.ata_command.block++;

    extra->bytes_this_mult += SECTOR_SIZE;
}


void AtaServiceCtrlRequest(ata_ctrl_t *ctrl)
{
    asyncio_t *io = (asyncio_t*) ctrl->device->io_first;
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
            if (!AtaIssueCommand(ctrl,
				&req->params.ata_command))
                goto failure;

            extra->bytes_this_mult = 0;
            if (req->params.ata_command.command == CMD_WRITE)
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
            if (!AtapiIssuePacket(ctrl, &req->params.atapi_packet))
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
    DevFinishIo(ctrl->device, io, result);
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
    TRACE1("[%u]", Phase);

    if (ctrl->status & STATUS_ERR)
    {
        count = AtapiTransferCount(ctrl);
        wprintf(L"AtapiPacketInterrupt: error: count = %u, status = %x, retries = %u, error = %x\n", 
            count, ctrl->status, ap->businfo->retries, in(ctrl->base + REG_ERROR));

        ap->businfo->retries++;
        if (ap->businfo->retries >= 3)
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

	case ATAPI_PH_CMDOUT:
		return false;

    case ATAPI_PH_DONE:
        if (ap->count == 0)
        {
            //wprintf(L"AtapiPacketInterrupt: finished\n");
            return true;
        }

        /* fall through */

    case ATAPI_PH_DATAIN:
        /* read data, advance pointers */
        ap->businfo->retries = 0;

        count = AtapiTransferCount(ctrl);
        if (io->length + count <= io->pages->num_pages * PAGE_SIZE)
        {
            buffer = MemMapPageArray(io->pages);
            count = AtapiReadAndDiscard(ctrl, buffer + io->length, 0xFFFFu);
            MemUnmapPageArray(io->pages);
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
            TRACE1("finished, %u bytes read\n", io->length);
            return true;
        }
        else
        {
            TRACE1("%u ", ap->block);
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

bool AtaCtrlIsr(device_t *device, uint8_t irq)
{
    ata_ctrl_t *ctrl = device->cookie;
    ata_ctrlreq_t *req_ctrl;
    asyncio_t *io;
    uint8_t *buf;
    size_t mult, total;
    ata_ioextra_t *extra;
    
    ctrl->status = in(ctrl->base + REG_STATUS);
    /*wprintf(L"AtaCtrlIsr: status = %x\n", ctrl->status);*/

    io = device->io_first;
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
            total = req_ctrl->params.ata_command.count * SECTOR_SIZE;
            mult = min(req_ctrl->params.ata_command.businfo->rw_multiple, 
                total - io->length);

#ifdef DEBUG
            //if (req_ctrl->params.ata_command.command == CMD_WRITE)
                /*wprintf(L"ATA: count = %u(%u) length = %u length_pages = %u phys = %x ",
                    req_ctrl->params.ata_command.count,
                    mult,
                    total,
                    io->pages->num_pages,
                    io->pages->pages[PAGE_ALIGN(io->length) / PAGE_SIZE]);*/
#endif

            if (req_ctrl->params.ata_command.command == CMD_READ)
            {
                buf = MemMapPageArray(io->pages);
                assert(buf != NULL);

                //TRACE1("buf = %p: ", buf);
                ins16(ctrl->base + REG_DATA, buf + io->length, SECTOR_SIZE);
                MemUnmapPageArray(io->pages);

                io->length += SECTOR_SIZE;
                req_ctrl->params.ata_command.block++;
                extra->bytes_this_mult += SECTOR_SIZE;
            }

            if (io->length >= total)
            {
                /* This chunk too us to the end of the request */

                //if (req_ctrl->params.ata_command.command == CMD_WRITE)
                    //TRACE0("finished\n");
                finish = true;
                req_ctrl->header.result = 0;
            }
            else
            {
                size_t bytes_to_go;

                bytes_to_go = req_ctrl->params.ata_command.count * SECTOR_SIZE - io->length;
                //if (req_ctrl->params.ata_command.command == CMD_WRITE)
                    //TRACE1("%u bytes to go\n", bytes_to_go);
                finish = false;

                if (extra->bytes_this_mult >= req_ctrl->params.ata_command.businfo->rw_multiple)
                {
                    /* Finished all the sectors in this chunk: queue the next lot */
                    //if (req_ctrl->params.ata_command.command == CMD_WRITE)
                        //TRACE0("AtaCtrlIsr: requeueing command\n");
                    AtaServiceCtrlRequest(ctrl);
                }
                else if (req_ctrl->params.ata_command.command == CMD_WRITE)
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
            DevFinishIo(device, io, req_ctrl->header.result);
            AtaServiceCtrlRequest(ctrl);
        }
    }
    else
	{
        TRACE1("AtaCtrlIsr: no requests queued, status = %x\n", ctrl->status);
	}

    return true;
}

status_t AtaCtrlRequest(device_t *device, request_t *req)
{
    ata_ctrl_t *ctrl = device->cookie;
    ata_ctrlreq_t *req_ctrl;
    asyncio_t *io;
    ata_ioextra_t *extra;

    switch (req->code)
    {
    case DEV_REMOVE:
        free(ctrl);
        return 0;

    case ATAPI_PACKET:
		req_ctrl = (ata_ctrlreq_t*) req;
        io = DevQueueRequest(device, req, sizeof(ata_ctrlreq_t),
            req_ctrl->params.atapi_packet.pages, 
            ATAPI_SECTOR_SIZE * req_ctrl->params.atapi_packet.count);
        io->length = 0;
        extra = malloc(sizeof(ata_ioextra_t));
        extra->bytes_this_mult = 0;
        io->extra = extra;
        if (ctrl->command == CMD_IDLE)
            AtaServiceCtrlRequest(ctrl);
        return SIOPENDING;

    case ATA_COMMAND:
		req_ctrl = (ata_ctrlreq_t*) req;
        io = DevQueueRequest(device, req, sizeof(ata_ctrlreq_t),
            req_ctrl->params.ata_command.pages,
            SECTOR_SIZE * req_ctrl->params.ata_command.count);
        io->length = 0;
        extra = malloc(sizeof(ata_ioextra_t));
        extra->bytes_this_mult = 0;
        io->extra = extra;
        if (ctrl->command == CMD_IDLE)
            AtaServiceCtrlRequest(ctrl);
        return SIOPENDING;
    }

    return ENOTIMPL;
}

static const device_vtbl_t ata_controller_vtbl =
{
	NULL,
    AtaCtrlRequest,
    AtaCtrlIsr
};


static const wchar_t *ata_drive_names[DRIVES_PER_CONTROLLER] =
{
    L"master",
    L"slave"
};

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

void AtaInitController(driver_t *drv, const wchar_t *name, uint16_t base, 
                       uint8_t irq, dev_config_t *cfg)
{
#define id_byte(n)    (((uint8_t*) tmp_buf) + (n))
#define id_word(n)    (tmp_buf + (n))

    ata_ctrl_t *ctrl;
    uint8_t r;
    unsigned i, j;
    ata_identify_t id;
	ata_businfo_t *businfo;
    uint16_t *ptr;
    ata_command_t cmd;
    uint32_t size;
    wchar_t drive_name[50];

    ctrl = malloc(sizeof(ata_ctrl_t));
    if (ctrl == NULL)
        return;

    memset(ctrl, 0, sizeof(ata_ctrl_t));
    ctrl->base = base;
    SpinInit(&ctrl->sem);
    num_controllers++;

    TRACE1("AtaInitController: initialising controller on %x\n", ctrl->base);

    /* Check for controller */
    AtaReset(ctrl);
    r = in(ctrl->base + REG_CYL_LO);
    out(ctrl->base + REG_CYL_LO, ~r);
    if (in(ctrl->base + REG_CYL_LO) == r)
    {
        wprintf(L"Controller at %x not found: %x\n", base, r);
        /*return false;*/
    }

    ctrl->device = DevAddDevice(drv, &ata_controller_vtbl, 0, name, cfg, ctrl);
	DevRegisterIrq(irq, ctrl->device);
    ArchMaskIrq(1 << irq, 0);

    for (i = 0; i < DRIVES_PER_CONTROLLER; i++)
    {
		businfo = &ctrl->drives[i].businfo;

        businfo->ldhpref = ldh_init(i);

        wprintf(L"Drive %u: ", i);

        cmd.command = ATA_IDENTIFY;
		cmd.businfo = businfo;
        businfo->is_atapi = false;
        if (AtaIssueSimpleCommand(ctrl, &cmd, ATA_TIMEOUT_IDENTIFY) != wfiOk)
        {
            cmd.command = ATAPI_IDENTIFY;
            wprintf(L"trying ATAPI: ");

            if (AtaIssueSimpleCommand(ctrl, &cmd, ATA_TIMEOUT_IDENTIFY) != wfiOk)
            {
                wprintf(L"not detected\n");
                continue;
            }

            businfo->is_atapi = true;
        }

        /* Device information. */
        ptr = (uint16_t*) &id;
        for (j = 0; j < 256; j++)
            ptr[j] = in16(ctrl->base + REG_DATA);
        
        AtaFormatString(id.model, id.model, sizeof(id.model));
        AtaFormatString(id.serial, id.serial, sizeof(id.serial));

        /* Preferred CHS translation mode. */
        businfo->cylinders = id.cylinders;
        businfo->heads = id.heads;
        businfo->sectors = id.sectors;
        size = (uint32_t) businfo->cylinders 
            * businfo->heads 
            * businfo->sectors;

        if (id.pio & 0x0200 && size > 512L*1024*2)
        {
            /* Drive is LBA capable and is big enough to trust it to
             * not make a mess of it.
             */
            wprintf(L"LBA ");
            businfo->ldhpref |= LDH_LBA;
            size = id.capacity;
        }

        if (businfo->is_atapi)
            businfo->rw_multiple = ATAPI_SECTOR_SIZE;
        else
        {
            if ((id.mult_support & 0xff) != 0)
                businfo->rw_multiple = (id.mult_support & 0xff) * SECTOR_SIZE;
            else
                businfo->rw_multiple = SECTOR_SIZE;
        }

        wprintf(L"CHS = %u:%u:%u size = %luMB \"%.40S\" \"%.20S\" mult_support = %u rmsn = %02x features = %02x%02x%02x\n",
            businfo->cylinders,
            businfo->heads,
            businfo->sectors,
            size / 2048,
            id.model,
            id.serial,
            id.mult_support & 0xff,
			id.rmsn_support,
			id.feature_support[0], id.feature_support[1], id.feature_support[2]);

        wcscpy(drive_name, ata_drive_names[i]);

		ctrl->drives[i].config.bus = ctrl->device;
		ctrl->drives[i].config.bus_type = DEV_BUS_ATA;
		ctrl->drives[i].config.num_resources = 0;
		ctrl->drives[i].config.resources = NULL;
        if (businfo->is_atapi)
		{
			ctrl->drives[i].config.device_class = 0x0082;
			ctrl->drives[i].config.profile_key = _wcsdup(L"ATA/AtapiDrive");
		}
		else
		{
			ctrl->drives[i].config.device_class = 0x0180;
			ctrl->drives[i].config.profile_key = _wcsdup(L"ATA/AtaDrive");
		}

		ctrl->drives[i].config.businfo = businfo;
		ctrl->drives[i].config.location.number = i;

        ctrl->drives[i].device = AtaAddDrive(drv, 
			drive_name, 
			&ctrl->drives[i].config);

		AtaPartitionDevice(ctrl->drives[i].device);
    }
}


void AtaAddController(driver_t *drv, const wchar_t *name, 
                      dev_config_t *cfg)
{
    static const uint16_t ports[] = { REG_BASE0, REG_BASE1 };
	static const uint8_t irqs[] = { AT_IRQ0, AT_IRQ1 };

    if (num_controllers < 2)
        AtaInitController(drv, name, ports[num_controllers], 
            irqs[num_controllers], cfg);
}
