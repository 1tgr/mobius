/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/debug.h>

#include <errno.h>
#include <string.h>
#include <wchar.h>

#include "interface.h"
#include "hardware.h"


status_t AtaDriveRequest(device_t *device, request_t *req)
{
	request_dev_t *req_dev = (request_dev_t*) req;
	ata_businfo_t *businfo;
	device_t *ctrl;
    ata_ctrlreq_t *ctrl_req;
    io_callback_t cb;

	businfo = device->cfg->businfo;
	ctrl = device->cfg->bus;

    switch (req->code)
    {
    case DEV_READ:
    case DEV_WRITE:
        ctrl_req = malloc(sizeof(ata_ctrlreq_t));
        if (businfo->is_atapi)
        {
			uint8_t Pkt[12] = { 0 };
            uint16_t Count;
            addr_t block;

            if (req->code != DEV_READ)
                return EINVALID;

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

            TRACE2("AtaDriveRequest(DEV_READ): atapi packet: block = %u count = %u\n",
                block, Count);
            ctrl_req->header.code = ATAPI_PACKET;
            ctrl_req->params.atapi_packet.businfo = businfo;
            ctrl_req->params.atapi_packet.count = Count;
            ctrl_req->params.atapi_packet.block = block;
            memcpy(ctrl_req->params.atapi_packet.packet, Pkt, sizeof(Pkt));
            ctrl_req->params.atapi_packet.pages = req_dev->params.dev_read.pages;
        }
        else
        {
            ata_command_t *cmd;

            if (req->code == DEV_WRITE)
                wprintf(L"AtaDriveRequest: DEV_WRITE(%lx)\n",
                    (uint32_t) req_dev->params.dev_read.offset / SECTOR_SIZE);

			ctrl_req->header.code = ATA_COMMAND;
			cmd = &ctrl_req->params.ata_command;
			cmd->businfo = businfo;
            cmd->count = req_dev->params.dev_read.length / SECTOR_SIZE;
            cmd->block = req_dev->params.dev_read.offset / SECTOR_SIZE;
            cmd->command = (req->code == DEV_WRITE) ? CMD_WRITE : CMD_READ;
			cmd->pages = req_dev->params.dev_read.pages;
        }

        ctrl_req->header.param = req;
        cb.type = IO_CALLBACK_DEVICE;
        cb.u.dev = device;
        return IoRequest(&cb, ctrl, &ctrl_req->header);
    }

    return ENOTIMPL;
}

void AtaDriveFinishIo(device_t *device, request_t *req)
{
    request_t *originator;

    assert(req->param != NULL);

    originator = req->param;
	originator->result = req->result;
	/* xxx -- need to update originator with byte length read */
    TRACE4("AtaDriveFinishIo(%p): callback = %u/%p, result = %d\n",
        originator,
        originator->callback.type,
        originator->callback.u.function,
		req->result);

    IoNotifyCompletion(originator);
    free(req);
}

static const device_vtbl_t ata_drive_vtbl =
{
	NULL,				/* delete_device */
    AtaDriveRequest,    /* request */
    NULL,               /* isr */
    AtaDriveFinishIo,   /* finishio */
};


device_t *AtaAddDrive(driver_t *drv, const wchar_t *name, dev_config_t *cfg)
{
	return DevAddDevice(drv,
		&ata_drive_vtbl,
		0,
		name,
		cfg,
		NULL);
}
