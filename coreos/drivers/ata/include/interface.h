/* $Id$ */
#ifndef INTERFACE_H__
#define INTERFACE_H__

typedef struct ata_businfo_t ata_businfo_t;
struct ata_businfo_t
{
	unsigned cylinders;
    unsigned sectors;
    unsigned heads;
    uint8_t ldhpref;
    bool is_atapi;
    unsigned rw_multiple;
    unsigned retries;
};

typedef struct atapi_packet_t atapi_packet_t;
struct atapi_packet_t
{
    ata_businfo_t *businfo;
    uint8_t packet[12];
    page_array_t *pages;
    uint32_t block;
    uint8_t count;
};

typedef struct ata_command_t ata_command_t;
struct ata_command_t
{
    ata_businfo_t *businfo;
    uint8_t count;
    uint32_t block;
    uint8_t command;
    page_array_t *pages;
};

typedef struct ata_ctrlreq_t ata_ctrlreq_t;
struct ata_ctrlreq_t
{
    request_t header;
    union
    {
        ata_command_t ata_command;
        atapi_packet_t atapi_packet;
    } params;
};

#define ATA_COMMAND     REQUEST_CODE(0, 0, 'a', 'c')
#define ATAPI_PACKET    REQUEST_CODE(0, 0, 'a', 'p')

void AtaAddController(driver_t *drv, const wchar_t *name, dev_config_t *cfg);
device_t *AtaAddDrive(driver_t *drv, const wchar_t *name, dev_config_t *cfg);
void AtaPartitionDevice(device_t *drive);

#endif
