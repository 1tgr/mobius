/* $Id$ */
#ifndef HARDWARE_H__
#define HARDWARE_H__

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
    uint16_t config;            /*   0 obsolete stuff */
    uint16_t cylinders;         /*   1 logical cylinders */
    uint16_t _reserved_2;
    uint16_t heads;             /*   3 logical heads */
    uint16_t _vendor_4;
    uint16_t _vendor_5;
    uint16_t sectors;           /*   6 logical sectors */
    uint16_t _vendor_7;
    uint16_t _vendor_8;
    uint16_t _vendor_9;
    char serial[20];            /*  10-19 serial number */
    uint16_t _vendor_20;
    uint16_t _vendor_21;
    uint16_t vend_bytes_long;   /*  22 no. vendor bytes on long cmd */
    char firmware[8];
    char model[40];
    uint16_t mult_support;      /*  47 vendor stuff and multiple cmds */
    uint16_t _reserved_48;
    uint16_t capabilities;
    uint16_t _reserved_50;
    uint16_t pio;
    uint16_t dma;
    uint16_t _reserved_53;
    uint16_t curr_cyls;         /*  54 current logical cylinders */
    uint16_t curr_heads;        /*  55 current logical heads */
    uint16_t curr_sectors;      /*  56 current logical sectors */
    uint32_t capacity;          /*  57,58 capacity in sectors */
    uint16_t _pad1[82-59];      /* don't need this stuff for now */
    uint16_t feature_support[3]; /* 82-84 features supported */
	uint16_t feature_enabled[3]; /* 85-87 features supported */
	uint16_t _pad2[127-88];
	uint16_t rmsn_support;      /* 127 removable media status notification */
	uint16_t _pad3[256-128];
};

#pragma pack(pop)

#define CONFIG_REMOVABLE		0x80

#define FSUP1_REMOVABLE_STATUS	0x10

CASSERT(sizeof(ata_identify_t) == 512);

#endif
