/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/debug.h>

#include <string.h>
#include <stdio.h>
#include <wchar.h>

#include "hardware.h"

typedef struct ata_part_t ata_part_t;
struct ata_part_t
{
	dev_config_t cfg;
    uint32_t start_sector;
    uint32_t sector_count;
};


static void AtaDeleteDevice(device_t *device)
{
}


static status_t AtaPartitionRequest(device_t *device, request_t *req)
{
    ata_part_t *part = device->cookie;
    request_dev_t *req_dev = (request_dev_t*) req;

    switch (req->code)
    {
    case DEV_WRITE:
    case DEV_READ:
        req_dev->params.dev_read.offset += part->start_sector * SECTOR_SIZE;

    default:
        return device->cfg->bus->vtbl->request(device->cfg->bus, req);
    }
}

static const device_vtbl_t ata_partition_vtbl =
{
	AtaDeleteDevice,
    AtaPartitionRequest,
    NULL
};


void AtaPartitionDevice(device_t *drive)
{
	static const wchar_t key_name[] = L"ATA/Volume";
    wchar_t name[5];
    unsigned i;
    ata_part_t *part;
    uint8_t bytes[512];
    partition_t *parts;

    parts = (partition_t*) (bytes + 0x1be);
    TRACE1("AtaPartitionDevice: bytes = %p\n", bytes);
    if (IoReadSync(drive, 0, bytes, sizeof(bytes)) == sizeof(bytes) &&
        bytes[510] == 0x55 &&
        bytes[511] == 0xaa)
    {
        TRACE0("AtaPartitionDevice: read finished\n");

        for (i = 0; i < 4; i++)
            if (parts[i].system != 0)
            {
                part = malloc(sizeof(ata_part_t));
                memset(part, 0, sizeof(ata_part_t));

                part->start_sector = parts[i].start_sector_abs;
                part->sector_count = parts[i].sector_count;
                wprintf(L"partition %c: start = %lu, count = %lu\n",
                    i + 'a', part->start_sector, part->sector_count);

                swprintf(name, L"%u", i);

				part->cfg.bus = drive;
				part->cfg.bus_type = DEV_BUS_ATA_DRIVE;
				part->cfg.num_resources = 0;
				part->cfg.resources = NULL;
				part->cfg.profile_key = _wcsdup(key_name);
                part->cfg.device_class = 0x0081;
				part->cfg.businfo = NULL;
				part->cfg.location.ptr = NULL;
                DevAddDevice(drive->driver, 
					&ata_partition_vtbl, 
					0, 
					name, 
					&part->cfg, 
					part);
            }
    }
    /*else
    {
        TRACE0("AtaPartitionDevice: read failed\n");

        part = malloc(sizeof(ata_part_t));
        memset(part, 0, sizeof(ata_part_t));

        part->start_sector = 0;
        part->sector_count = drive->pcylinders 
            * drive->pheads 
            * drive->psectors;
        part->drive = drive->device;
        wprintf(L"partition a: start = %lu, count = %lu\n",
            part->start_sector, part->sector_count);

        part->cfg.parent = drive->device;
        part->cfg.device_class = 0x0081;
		part->cfg.profile_key = _wcsdup(key_name);
        part->device = DevAddDevice(drive->device->driver, &ata_partition_vtbl, 
			0, L"0", &part->cfg, part);
    }*/
}
