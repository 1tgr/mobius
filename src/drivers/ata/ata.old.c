#include <kernel/kernel.h>
#include <kernel/driver.h>
/*#include <kernel/cache.h>*/
#include <stdlib.h>
#include <wchar.h>
/*#include <conio.h>*/
#include <printf.h>
#include <stdio.h>
/*#include <os/blkdev.h>*/
#include <errno.h>
#include "ata.h"

/*#include <kernel/debug.h>*/

#define MAX_DRIVES	2

typedef struct atadrive_t atadrive_t;
struct atadrive_t
{
	device_t dev;
	word port;
	byte unit;
	word sectors, heads, cylinders, mult_max;
	wchar_t name[41];
	dword total_sectors;
};

typedef struct atactrl_t atactrl_t;
struct atactrl_t
{
	device_t dev;
	byte irq;
	atadrive_t* cur_drive;
	atadrive_t* drives[MAX_DRIVES];
};

typedef struct atapart_t atapart_t;
struct atapart_t
{
	device_t dev;
	device_t* drive;
	dword start_sector, total_sectors;
};

bool ataWaitStatus(atadrive_t* ctx, word mask, word bits)
{
	dword end;
	word stat;

	end = sysUpTime() + 2000;
	while (((stat = in(ctx->port + ATA_REG_STATUS)) & mask) != bits)
	{
		if (sysUpTime() >= end)
		{
			TRACE1("ATA: wait failed; stat = %x\n", stat);
			return false;
		}
	}

	return true;
}

void ataBlockToChs(atadrive_t* ctx, addr_t block, dword *cyl, dword *head, dword *sect)
{
	*sect = block % ctx->sectors + 1;
	block /= ctx->sectors;
	*head = block % ctx->heads;
	block /= ctx->heads;
	*cyl = block;
}

size_t ataBlockRead(atadrive_t* ctx, addr_t start, size_t blocks, void* buffer)
{
	unsigned short *buf;
	int cyl, head, sect;
	size_t i, read, per = min(blocks, ctx->mult_max);
	
	if (!ctx->heads || !ctx->sectors)
		return 0;

	buf = (unsigned short *) buffer;
	read = 0;

	while (read < blocks)
	{
		ataBlockToChs(ctx, start, &cyl, &head, &sect);
		TRACE4("%d = %d:%d:%d\t", start, cyl, head, sect);
		if (!ataWaitStatus(ctx, ATA_BUSY, 0))
			return read;

		out(ctx->port + ATA_REG_DRVHD, ctx->unit);
		if (!ataWaitStatus(ctx, ATA_BUSY | ATA_READY, ATA_READY))
			return read;
		
		out(ctx->port + ATA_REG_LOCYL, cyl & 0xff);
		out(ctx->port + ATA_REG_HICYL, (cyl >> 8) & 0xff);
		out(ctx->port + ATA_REG_SECTOR, sect);
		out(ctx->port + ATA_REG_DRVHD, head);
		out(ctx->port + ATA_REG_COUNT, per);
		out(ctx->port + ATA_REG_CMD, ATA_CMD_READ);
		
		if (!ataWaitStatus(ctx, ATA_BUSY | ATA_DRQ, ATA_DRQ))
			return read;
		
		for (i = 0; i < 256 * per; i++)
			buf[i] = in16(ctx->port + ATA_REG_DATA);
		
		read += per;
		start += per;
		buf += per * 256;
	}

	return read;
}

bool ataStartRead(atadrive_t* ctx, addr_t start, size_t blocks)
{
	int cyl, head, sect;
	size_t per = min(blocks, ctx->mult_max);
	atactrl_t* ctrl = (atactrl_t*) ctx->dev.config->parent;

	assert(ctrl != NULL);

	if (!ctx->heads || !ctx->sectors)
		return false;

	ataBlockToChs(ctx, start, &cyl, &head, &sect);
	TRACE4("%d = %d:%d:%d\t", start, cyl, head, sect);
	if (!ataWaitStatus(ctx, ATA_BUSY, 0))
		return false;

	out(ctx->port + ATA_REG_DRVHD, ctx->unit);
	if (!ataWaitStatus(ctx, ATA_BUSY | ATA_READY, ATA_READY))
		return false;
	
	out(ctx->port + ATA_REG_LOCYL, cyl & 0xff);
	out(ctx->port + ATA_REG_HICYL, (cyl >> 8) & 0xff);
	out(ctx->port + ATA_REG_SECTOR, sect);
	out(ctx->port + ATA_REG_DRVHD, head);
	out(ctx->port + ATA_REG_COUNT, per);
	out(ctx->port + ATA_REG_CMD, ATA_CMD_READ);
	return true;
}

size_t ataBlockWrite(atadrive_t* ctx, addr_t start, size_t blocks, const void* buffer)
{
	return 0;
}

wchar_t *ataConvertName(const word* in_data, int off_start, int off_end)
{
	static wchar_t ret_val[255];
	int loop, loop1;

	for (loop = off_start, loop1 = 0; loop <= off_end; loop++)
	{
		ret_val [loop1++] = (char) (in_data [loop] / 256);  /* Get High byte */
		ret_val [loop1++] = (char) (in_data [loop] % 256);  /* Get Low byte */
	}

	for (loop1--; loop1 >= 0 && ret_val[loop1] == ' '; loop1--)
		;

	ret_val[loop1 + 1] = '\0';  /* Make sure it ends in a NULL character */
	return ret_val;
}

#pragma pack(push, 1)

typedef struct partition_t partition_t;
struct partition_t
{
	byte bBoot;
	byte bStartHead;
	byte bStartSector;
	byte bStartCylinder;
	byte bSystem;
	byte bEndHead;
	byte bEndSector;
	byte bEndCylinder;
	dword dwStartSector;
	dword dwSectorCount;
};
#pragma pack(pop)

const wchar_t* part_type(int type)
{
	switch (type)
	{
	case 0x00:
		return L"FDISK_TYPE_EMPTY";
	case 0x01:
		return L"FDISK_TYPE_FAT12";
	case 0x04:
		return L"FDISK_TYPE_FAT16_SMALL";
	case 0x05:
		return L"FDISK_TYPE_EXTENDED";
	case 0x06:
		return L"FDISK_TYPE_FAT16_BIG";
	//case 0x07:
	case 0x0C:
		return L"FDISK_TYPE_FAT32";
	case 0x0F:
		return L"FDISK_TYPE_NTFS";
	case 0x82:
		return L"FDISK_TYPE_LINUX_SWAP";
	case 0x83:
		return L"FDISK_TYPE_EXT2";
	case 0xa5:
		return L"FDISK_TYPE_FREEBSD";
	case 0xa6:
		return L"FDISK_TYPE_OPENBSD";
	case 0xeb:
		return L"FDISK_TYPE_BFS";
	default:
		return L"unknown";
	}
}

void nsleep(dword ns)
{
}

bool ataNextRequest(atadrive_t* ctx)
{
	atactrl_t* ctrl;
	request_t *req, *next;
	
	assert(ctx->dev.config != NULL);
	ctrl = (atactrl_t*) ctx->dev.config->parent;
	assert(ctrl != NULL);

	if (ctx->dev.req_first == NULL)
	{
		ctrl->cur_drive = NULL;
		return true;	// All requests are finished
	}
	
	req = ctx->dev.req_first;
	if (ctrl->cur_drive == NULL)
	{
		/* No requests are pending on the controller */
		TRACE1("ata: starting request buf = %p\n", req->params.read.buffer);
		ctrl->cur_drive = ctx;
		req->user_length = req->params.read.length;
		req->params.read.length = 0;
	}
	else
	{
		assert(ctrl->cur_drive == ctx);

		/* There is a request on the controller, and it's ours */

		if (req->params.read.length >= req->user_length)
		{
			TRACE0("ata: finished\n");
			next = req->next;
			devFinishRequest(&ctx->dev, req);
			req = next;
			
			if (ctx->dev.req_first == NULL)
			{
				TRACE0("ata: this drive has finished\n");
				ctrl->cur_drive = NULL;
				return true;
			}

			req = next;
			req->user_length = req->params.read.length;
			req->params.read.length = 0;
		}
	}
	
	if (req->params.read.length < req->user_length)
	{
		size_t toread = req->user_length - req->params.read.length;
		TRACE2("ata: pos = %d read %d\n", 
			(size_t) (req->params.read.pos / 512), 
			toread / 512);
		if (!ataStartRead(ctx, 
			(size_t) (req->params.read.pos / 512),
			toread / 512))
			wprintf(L"ata: ataStartRead failed\n");
		
		/* Controller will generate an interrupt when the op has finished */
	}

	return false;
}

bool ataRequest(device_t* dev, request_t* req)
{
	atadrive_t* ctx = (atadrive_t*) dev;
	block_size_t* size;

	switch (req->code)
	{
	case DEV_REMOVE:
		hndFree(ctx);
		hndSignal(req->event, true);
		return true;

	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_READ:
	case DEV_WRITE:
		TRACE0("ata: start request\n");
		devStartRequest(dev, req);
		ataNextRequest(ctx);
		return true;

	case BLK_GETSIZE:
		size = (block_size_t*) req->params.buffered.buffer;
		size->block_size = 512;
		size->total_blocks = ctx->total_sectors;
		hndSignal(req->event, true);
		return true;

	default:
		wprintf(L"ataRequest: %c%c\n", req->code / 256, req->code % 256);
	}
	
	req->result = ENOTIMPL;
	return false;
}

bool ataPartitionRequest(device_t* dev, request_t* req)
{
	atapart_t* ctx = (atapart_t*) dev;
	block_size_t* size;

	switch (req->code)
	{
	case DEV_REMOVE:
		hndFree(ctx);
		hndSignal(req->event, true);
		return true;

	case BLK_GETSIZE:
		size = (block_size_t*) req->params.buffered.buffer;
		size->block_size = 512;
		size->total_blocks = ctx->total_sectors;
		hndSignal(req->event, true);
		return true;

	case DEV_READ:
	case DEV_WRITE:
		req->params.read.pos += ctx->start_sector * 512;
	default:
		return ataRequest(ctx->drive, req);
	}
}

bool ataControllerRequest(device_t* dev, request_t* req)
{
	atactrl_t* ctx = (atactrl_t*) dev;
	atadrive_t* drive;
	word *buf, per;
	int i;
	addr_t start;
	request_t* drvreq;
	size_t toread;

	switch (req->code)
	{
	case DEV_REMOVE:
		devRegisterIrq(dev, ctx->irq, false);
		hndFree(ctx);
		hndSignal(req->event, true);
		return true;

	case DEV_ISR:
		assert(req->params.isr.irq == ctx->irq);
		TRACE1("DEV_ISR: %d\t", req->params.isr.irq);

		drive = ctx->cur_drive;
		if (drive)
		{
			drvreq = drive->dev.req_first;
			if (drvreq)
			{
				toread = drvreq->user_length - drvreq->params.read.length;
				per = min(toread / 512, drive->mult_max);
				start = (size_t) (drvreq->params.read.length / 512);
				buf = (word*) (drvreq->params.read.buffer + start * 512);
				
				for (i = 0; i < 256 * per; i++)
					buf[i] = in16(drive->port + ATA_REG_DATA);
				
				drvreq->params.read.length += per * 512;
				drvreq->params.read.pos += per * 512;
			}

			while (ataNextRequest(drive))
			{
				TRACE0("atactrl: Drive finished, looking for another\n");
				ctx->cur_drive = NULL;

				drive = NULL;
				for (i = 0; i < MAX_DRIVES; i++)
					if (ctx->drives[i] &&
						ctx->drives[i]->dev.req_first)
					{
						drive = ctx->drives[i];
						break;
					}

				if (drive == NULL)
				{
					TRACE0("atactrl: No more drives; all finished!\n");
					break;
				}
			}
		}

		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

bool ataWaitTimeout(word port, byte mask, byte match, dword timeout)
{
	dword end = sysUpTime() + timeout;
	byte stat;

	while (((stat = in(port)) & mask) != match)
	{
		if (sysUpTime() > end)
		{
			TRACE1("ata: wait failed, stat = 0x%02x\n", stat);
			return false;
		}
	}

	return true;
}

device_t* ataAddControllerDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	word dd[256];
	int dd_off, i, j;
	atadrive_t *drive;
	atapart_t *part;
	atactrl_t *ctrl;
	wchar_t str[10];
	partition_t *parts;
	word port, irq;
	byte units[MAX_DRIVES] = { 0xA0, 0xB0 };
	device_config_t *dcfg;
	device_t *cached;
	
	parts = (partition_t*) (dd + 0xdf);
	i = devFindResource(cfg, dresIo, 0);
	if (i > -1)
		port = cfg->resources[i].u.io.base;
	else
		port = 0x1F0;

	i = devFindResource(cfg, dresIrq, 0);
	if (i > -1)
		irq = cfg->resources[i].u.irq;
	else
		irq = 14;

	TRACE2("IDE controller: port %x irq %d\n", port, irq);

	/* soft reset both drives on this I/F (selects master) */
	out(port + ATA_REG_DEVCTRL, 0x06);
	nsleep(400);
	/* release soft reset AND enable interrupts from drive */
	out(port + ATA_REG_DEVCTRL, 0x00);
	nsleep(400);
	/* wait up to 2 seconds for status =
	BUSY=0  READY=1  DF=?  DSC=?		DRQ=?  CORR=?  IDX=?  ERR=0 */

	if (!ataWaitTimeout(port + 7, 0xC1, 0x40, 5000))
	{
		wprintf(L"ata: no master detected\n");
		return NULL;
	}

	ctrl = hndAlloc(sizeof(atactrl_t), NULL);
	ctrl->dev.driver = drv;
	ctrl->dev.request = ataControllerRequest;
	ctrl->dev.req_first = ctrl->dev.req_last = NULL;
	ctrl->dev.config = cfg;
	ctrl->irq = irq;
	ctrl->cur_drive = NULL;

	for (i = 0; i < countof(units); i++)  /* Loop through drives on this controller */
    {
		ctrl->drives[i] = NULL;

		TRACE0("Wait not busy...");
		if (!ataWaitTimeout(port + 7, 0xff, 0x50, 2000))
			continue;
		TRACE0("done\n");

		out(port + 6, units[i]); /* Get first/second drive */
		out(port + 7, 0xEC);          /* Get drive info data */

		TRACE0("Wait ready...");
		if (!ataWaitTimeout(port + 7, 0xff, 0x58, 2000))
			continue;
		TRACE0("done\n");

		TRACE1("locyl = %x ", in(port + ATA_REG_LOCYL));
		TRACE1("hicyl = %x\n", in(port + ATA_REG_HICYL));

		for (dd_off = 0; dd_off != 256; dd_off++) /* Read "sector" */
			dd[dd_off] = in16(port);
		
		drive = hndAlloc(sizeof(atadrive_t), NULL);
		drive->dev.driver = drv;
		drive->dev.request = ataRequest;
		drive->dev.req_first = drive->dev.req_last = NULL;
		drive->dev.config = NULL;
		drive->port = port;
		drive->unit = units[i];
		drive->cylinders = dd[1];
		drive->heads = dd[3];
		drive->sectors = dd[6];
		drive->total_sectors = drive->sectors * drive->heads * drive->cylinders;

		ctrl->drives[i] = drive;

		if (((dd[119] & 1) != 0) && (dd[118] != 0))
			drive->mult_max = dd[94];
		else
			drive->mult_max = 1;

		wcscpy(drive->name, ataConvertName(dd, 27, 46));
		wcscat(drive->name, L" ");
		wcscat(drive->name, ataConvertName(dd, 10, 19));
		swprintf(str, L"ide%d", i);

		dcfg = hndAlloc(sizeof(device_config_t), NULL);
		dcfg->parent = &ctrl->dev;
		dcfg->num_resources = 0;
		dcfg->resources = NULL;
		dcfg->device_id = dcfg->vendor_id = 0xffff;
		dcfg->subsystem = 0xffffffff;
		
		//cached = &drive->dev;
		devRegister(str, &drive->dev, dcfg);
		wprintf(L"Registered %s => %s [ ", str, drive->name);

		if (ataBlockRead(drive, 0, 1, dd))
		{
			for (j = 0; j < 4; j++)
			{
				word c, h, s;

				c = parts[j].bStartCylinder | 
					((word) parts[j].bStartSector & 0xc0) << 2;
				h = parts[j].bStartHead;
				s = parts[j].bStartSector & 0x3f;

				if (parts[j].bSystem)
				{
					part = hndAlloc(sizeof(atapart_t), NULL);
					part->dev.driver = drv;
					part->dev.request = ataPartitionRequest;
					part->drive = &drive->dev;
										
					part->start_sector = parts[j].dwStartSector;
					part->total_sectors = parts[j].dwSectorCount;
					swprintf(str, L"ide%d%c", i, j + 'a');

					dcfg = hndAlloc(sizeof(device_config_t), NULL);
					dcfg->parent = &ctrl->dev;
					dcfg->num_resources = 0;
					dcfg->resources = NULL;
					dcfg->device_id = dcfg->vendor_id = 0xffff;
					dcfg->subsystem = 0xffffffff;

					cached = ccInstallBlockCache(&part->dev, 512);
					devRegister(str, cached, dcfg);
					wprintf(L"%s ", part_type(parts[j].bSystem));
				}
			}
		}

		_cputws(L"]\n");
	}
	
	//wprintf(L"Finished detection!\n");

	devRegisterIrq(&ctrl->dev, ctrl->irq, true);
	return &ctrl->dev;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = ataAddControllerDevice;
	return true;
}