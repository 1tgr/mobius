/* $Id: isapnp.c,v 1.1.1.1 2002/12/21 09:48:49 pavlovskii Exp $ */

/*
 * Mobius ISAPnP driver
 * 
 * Derived from isa.c:
 * 
 *   Detect/identify ISA (16-bit) boards using Plug 'n Play (PnP).
 *   Chris Giese <geezer@execpc.com>
 *   http://www.execpc.com/~geezer/os
 *   Last revised: May 31, 2000
 * 
 *   This code is based on Linux isapnptools:
 * 
 *      Copyright
 *      =========
 *      These programs are copyright P.J.H.Fox (fox@roestock.demon.co.uk)
 *      and distributed under the GPL (see COPYING).
 * 
 *   Bugs are due to Giese. Or maybe not...
 */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/profile.h>

#include <kernel/debug.h>

#include <errno.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

/* Ports */
#define PNP_ADR_PORT          0x279
#define PNP_WRITE_PORT        0xA79

/* MIN and MAX READ_ADDR must have the bottom two bits set */
#define MIN_READ_ADDR         0x203
#define MAX_READ_ADDR         0x3FF
/* READ_ADDR_STEP must be a multiple of 4 */
#define READ_ADDR_STEP        8

/* Bits */
#define CONFIG_WAIT_FOR_KEY   0x02
#define CONFIG_RESET_CSN      0x04

#define IDENT_LEN             9

/* Small Resource Item Names (SRINs) */
#define ISAPNP_SRIN_VERSION           0x1   // PnP version number
#define ISAPNP_SRIN_LDEVICE_ID        0x2   // Logical device id
#define ISAPNP_SRIN_CDEVICE_ID        0x3   // Compatible device id
#define ISAPNP_SRIN_IRQ_FORMAT        0x4   // IRQ format
#define ISAPNP_SRIN_DMA_FORMAT        0x5   // DMA format
#define ISAPNP_SRIN_START_DFUNCTION   0x6   // Start dependant function
#define ISAPNP_SRIN_END_DFUNCTION     0x7   // End dependant function
#define ISAPNP_SRIN_IO_DESCRIPTOR     0x8   // I/O port descriptor
#define ISAPNP_SRIN_FL_IO_DESCRIPOTOR 0x9   // Fixed location I/O port descriptor
#define ISAPNP_SRIN_VENDOR_DEFINED    0xE   // Vendor defined
#define ISAPNP_SRIN_END_TAG           0xF   // End tag

/* Large Resource Item Names (LRINs) */
#define ISAPNP_LRIN_MEMORY_RANGE      0x81  // Memory range descriptor
#define ISAPNP_LRIN_ID_STRING_ANSI    0x82  // Identifier string (ANSI)
#define ISAPNP_LRIN_ID_STRING_UNICODE 0x83  // Identifier string (UNICODE)
#define ISAPNP_LRIN_VENDOR_DEFINED    0x84  // Vendor defined
#define ISAPNP_LRIN_MEMORY_RANGE32    0x85  // 32-bit memory range descriptor
#define ISAPNP_LRIN_FL_MEMORY_RANGE32 0x86  // 32-bit fixed location memory range descriptor

typedef struct isacard_t isacard_t;
struct isacard_t
{
    isacard_t *prev, *next;
    wchar_t name[20];
    bool in_dependent;
    dev_config_t cfg;
	isa_businfo_t businfo;
};

typedef struct isapnp_t isapnp_t;
struct isapnp_t
{
    device_t *device;
    int port, read_port;
    uint8_t csum, backspaced, backspaced_char;
    unsigned boards_found;
    isacard_t *card_first, *card_last;
};

static __inline void pnp_poke(unsigned char x)
{
    out(PNP_WRITE_PORT, x);
}

static __inline unsigned char pnp_peek(isapnp_t *pnp)
{
    return in(pnp->read_port);
}

static __inline void pnp_wake(unsigned char x)
{
    out(PNP_ADR_PORT, 3);
    out(PNP_WRITE_PORT, x);
}

static __inline void pnp_set_read_port(isapnp_t *pnp, unsigned x)
{
    out(PNP_ADR_PORT, 0);
    out(PNP_WRITE_PORT, x >> 2);
    pnp->read_port = x | 3;
}

static __inline unsigned char pnp_status(isapnp_t *pnp)
{
    out(PNP_ADR_PORT, 5);
    return pnp_peek(pnp);
}

static __inline unsigned char pnp_resource_data(isapnp_t *pnp)
{
    out(PNP_ADR_PORT, 4);
    return pnp_peek(pnp);
}

static int pnp_await_status(isapnp_t *pnp)
{
    unsigned timeout;

    for (timeout = 5; timeout != 0; timeout--)
    {
        ArchMicroDelay(1000);
        if ((pnp_status(pnp) & 1) != 0)
            break;
    }
    if (timeout == 0)
    {
        TRACE0("pnp_await_status: timeout\n");
        return 1;
    }
    return 0;
}

static __inline int pnp_read_resource_data(isapnp_t *pnp, unsigned char *result)
{
    if (pnp_await_status(pnp))
        return 1;
    if (pnp->backspaced)
    {
        *result = pnp->backspaced_char;
        pnp->backspaced = 0;
    }
    *result = pnp_resource_data(pnp);
    pnp->csum = (pnp->csum + *result) & 0xFF;
    return 0;
}

static __inline void pnp_unread_resource_data(isapnp_t *pnp, char prev)
{
    pnp->csum = (pnp->csum - prev) & 0xFF;
    pnp->backspaced = 1;
    pnp->backspaced_char = prev;
}

static int pnp_read_one_resource(isapnp_t *pnp, isacard_t *card)
{
    unsigned char buffer[256], res_type;
    unsigned short i, res_len;
    unsigned long adr;

    if (pnp_read_resource_data(pnp, buffer + 0) != 0)
        return 1;

    if (buffer[0] & 0x80)
    {
        /* Large item */
        res_type = buffer[0];
        for (i = 0; i <= 1; i++)
        {
            if (pnp_read_resource_data(pnp, buffer + i) != 0)
                return 1;
        }
        res_len = buffer[1];
        res_len <<= 8;
        res_len |= buffer[0];
        TRACE1("large item of size %3u: ", res_len);
    }
    else
    {
        /* Small item */
        res_type = (buffer[0] >> 3) & 0x0f;
        res_len = buffer[0] & 7;
        TRACE1("small item of size %3u: ", res_len);
    }
    for (i = 0; i < res_len; i++)
    {
        if (pnp_read_resource_data(pnp, buffer + i) != 0)
            return 1;
    }

    switch (res_type)
    {
    case ISAPNP_SRIN_VERSION:
        TRACE0("ISAPNP_SRIN_VERSION\n");
        break;

    case ISAPNP_SRIN_LDEVICE_ID:
        i = (buffer[1] << 8) | buffer[0];
		card->businfo.vendor_id = i;
        card->businfo.device_id = (buffer[3] << 8) | buffer[2];
        swprintf(card->businfo.name, 
			L"%c%c%c%04x",
            ((i & 0x001f) >> 0) + 'A' - 1,
            ((i & 0x03e0) >> 5) + 'A' - 1,
            ((i & 0x7c00) >> 10) + 'A' - 1,
            (buffer[3] << 8) | buffer[2]);
		swprintf(card->name, L"isa:%s:%d",
			card->businfo.name,
			card->cfg.location.number);
        TRACE4("ISAPNP_SRIN_LDEVICE_ID: id=%c%c%c ven=%04x\n", 
            ((i & 0x001f) >> 0) + 'A' - 1,
            ((i & 0x03e0) >> 5) + 'A' - 1,
            ((i & 0x7c00) >> 10) + 'A' - 1,
            (buffer[3] << 8) | buffer[2]);
        break;

    case ISAPNP_SRIN_CDEVICE_ID:
        i = (buffer[1] << 8) | buffer[0];
        TRACE4("ISAPNP_SRIN_CDEVICE_ID: id=%c%c%c ven=%04x\n", 
            ((i & 0x001f) >> 0) + 'A' - 1,
            ((i & 0x03e0) >> 5) + 'A' - 1,
            ((i & 0x7c00) >> 10) + 'A' - 1,
            (buffer[3] << 8) | buffer[2]);
        break;

    case ISAPNP_SRIN_IRQ_FORMAT:
        i = buffer[1];
        i <<= 8;
        i |= buffer[0];
        TRACE1("IRQ_TAG: 0x%X\n", i);
        break;

    case ISAPNP_SRIN_DMA_FORMAT:
        TRACE1("DMA_TAG: 0x%X\n", buffer[0]);
        break;

    case ISAPNP_SRIN_START_DFUNCTION:
        TRACE0("ISAPNP_SRIN_START_DFUNCTION\n");
        card->in_dependent = true;
        break;

    case ISAPNP_SRIN_END_DFUNCTION:
        TRACE0("ISAPNP_SRIN_END_DFUNCTION\n");
        card->in_dependent = false;
        break;

    case ISAPNP_SRIN_IO_DESCRIPTOR:
        i = buffer[2];
        i <<= 8;
        i |= buffer[1];
        TRACE1("IOport_TAG: start=0x%X, ", i);

        i = buffer[4];
        i <<= 8;
        i |= buffer[3];
        i++;
        TRACE3("end=0x%X, step=%u, size=%u\n",
            i, buffer[5], buffer[6]);
        break;

    case ISAPNP_SRIN_FL_IO_DESCRIPOTOR:
        i = buffer[1];
        i <<= 8;
        i |= buffer[0];
        TRACE3("FixedIO_TAG: start=0x%X, size=%u, end=0x%X\n",
            i, buffer[2], i + 1);
        break;

    case ISAPNP_LRIN_MEMORY_RANGE:
        adr = buffer[2];
        adr <<= 8;
        adr |= buffer[1];
        adr <<= 8;
        TRACE1("MemRange_TAG: start=0x%lX, ", adr);

        adr = buffer[4];
        adr <<= 8;
        adr |= buffer[3];
        adr <<= 8;
        adr++;
        TRACE1("end=0x%lX, ", adr);

        i = buffer[6];
        i <<= 8;
        i |= buffer[5];
        TRACE1("step=0x%X, ", i);

        adr = buffer[8];
        adr <<= 8;
        adr |= buffer[7];
        adr <<= 8;
        adr++;
        TRACE1("size=0x%lX, ", adr);
        break;

    case ISAPNP_LRIN_ID_STRING_ANSI:
        TRACE0("ISAPNP_LRIN_ID_STRING_ANSI: '");
        for (i = 0; i < res_len; i++)
            TRACE1("%c", buffer[i]);
        TRACE0("'\n");
        break;

    case ISAPNP_LRIN_ID_STRING_UNICODE:
        TRACE0("ISAPNP_LRIN_ID_STRING_UNICODE: '");
        for (i = 0; i < res_len; i += 2)
            TRACE1("%c", buffer[i] | (buffer[i + 1] << 8));
        TRACE0("'\n");
        break;

#if 0
    case Mem32Range_TAG:
        res->start = res->end = 0;
        /* STUB */
        break;
    case FixedMem32Range_TAG:
        res->start = res->end = 0;
        /* STUB */
        break;
#endif

    case ISAPNP_SRIN_END_TAG:
        TRACE0("End_TAG\n");
        return 1;

    default:
        wprintf(L"isapnp: unknown tag type %u\n", res_type);
        break;
    }
    return 0;
}

static void pnp_read_board(isapnp_t *pnp, isacard_t *card, unsigned csn, unsigned char serial_id0)
{
    unsigned char temp, dummy;
    unsigned i;

    pnp_wake(csn);

    /*
     * Check for broken cards that don't reset their resource pointer properly.
     * Get the first byte
     */
    if (pnp_read_resource_data(pnp, &temp) != 0)
        return;

    /* Just check the first byte, if these match we assume it's ok */
    if (temp != serial_id0)
    {
        /* Assume the card is broken and this is the start of the resource data. */
        pnp_unread_resource_data(pnp, temp);
        goto broken;
    }

    /* Read resource data past serial identifier */
    for (i = 1; i < IDENT_LEN; i++)
    {
        if (pnp_read_resource_data(pnp, &dummy) != 0)
            return;
    }

    /* Now for the actual resource data */
    pnp->csum = 0;
broken:
    do
    {
        i = pnp_read_one_resource(pnp, card);
    } while (!i);
}

static int pnp_isolate(isapnp_t *pnp, isacard_t *card)
{
    unsigned char checksum = 0, good_adr = 0;
    unsigned char c1, c2, bit, new_bit, serial_id[IDENT_LEN];
    unsigned short byte;

    checksum = 0x6A;

    /* Assume we will find one */
    pnp->boards_found++;
    pnp_wake(0);
    pnp_set_read_port(pnp, pnp->port);
    ArchMicroDelay(1000);
    out(PNP_ADR_PORT, 0x01); /* SERIALISOLATION */
    ArchMicroDelay(1000);
    for (byte = 0; byte < IDENT_LEN - 1; byte++)
    {
        /* xxx - tighten this up */
        for (bit = 8; bit != 0; bit--)
        {
            new_bit = 0;
            ArchMicroDelay(250);
            c1 = pnp_peek(pnp);
            ArchMicroDelay(250);
            c2 = pnp_peek(pnp);

            if (c1 == 0x55)
            {
                if (c2 == 0xAA)
                {
                    good_adr = 1;
                    new_bit = 0x80;
                }
                else
                    good_adr = 0;
            }

            serial_id[byte] >>= 1;
            serial_id[byte] |= new_bit;

            /* Update checksum */
            if (((checksum >> 1) ^ checksum) & 1)
                new_bit ^= 0x80;
            checksum >>= 1;
            checksum |= new_bit;
        }
    }

    for (bit = 8; bit != 0; bit--)
    {
        new_bit = 0;
        ArchMicroDelay(250);
        c1 = pnp_peek(pnp);
        ArchMicroDelay(250);
        c2 = pnp_peek(pnp);
        if (c1 == 0x55)
        {
            if (c2 == 0xAA)
            {
                good_adr = 1;
                new_bit = 0x80;
            }
        }
        serial_id[byte] >>= 1;
        serial_id[byte] |= new_bit;
    }

    if (good_adr && (checksum == serial_id[byte]))
    {
        out(PNP_ADR_PORT, 0x06); /* CARDSELECTNUMBER */
        pnp_poke(pnp->boards_found);

        TRACE1("found board #%u\n", pnp->boards_found);
        pnp_read_board(pnp, card, pnp->boards_found, serial_id[0]);
        return 1;
    }

    /* We didn't find one */
    pnp->boards_found--;
    return 0;
}

static void pnp_send_key(void)
{
    static const char i_data[] =
    {
        0x6A, 0xB5, 0xDA, 0xED, 0xF6, 0xFB, 0x7D, 0xBE,
        0xDF, 0x6F, 0x37, 0x1B, 0x0D, 0x86, 0xC3, 0x61,
        0xB0, 0x58, 0x2C, 0x16, 0x8B, 0x45, 0xA2, 0xD1,
        0xE8, 0x74, 0x3A, 0x9D, 0xCE, 0xE7, 0x73, 0x39
    };
    unsigned short temp;

    out(PNP_ADR_PORT, 0);
    out(PNP_ADR_PORT, 0);
    for (temp = 0; temp < sizeof(i_data) / sizeof(char); temp++)
        out(PNP_ADR_PORT, i_data[temp]);

    ArchMicroDelay(2000);
}

status_t PnpRequest(device_t* device, request_t* req)
{
    switch (req->code)
    {
    case DEV_REMOVE:
        free(device);
        return 0;
    }

    return ENOTIMPL;
}

static const device_vtbl_t pnp_vtbl =
{
	NULL,
    PnpRequest,
    NULL,       /* isr */
    NULL,       /* finishio */
};

void PnpInitCard(isapnp_t *pnp, isacard_t *card)
{
    memset(card, 0, sizeof(isacard_t));
    card->cfg.bus_type = DEV_BUS_ISA;
    card->cfg.location.number = pnp->boards_found;
    card->cfg.bus = pnp->device;
	card->cfg.businfo = &card->businfo;
}

void PnpAddCard(isapnp_t *pnp, isacard_t *card)
{
    wchar_t key[50];
    const wchar_t *driver, *device;

    LIST_ADD(pnp->card, card);

    swprintf(key, L"ISA/%s", card->businfo.name);
	card->cfg.profile_key = _wcsdup(key);
    wprintf(L"isapnp: %s\n", key);
    driver = ProGetString(key, L"Driver", card->businfo.name);
    //device = ProGetString(key, L"Device", card->businfo.name);
	device = card->name;
    DevInstallDevice(driver, device, &card->cfg);
}

void PnpAddDevice(driver_t *drv, const wchar_t *name, dev_config_t *cfg)
{
    isapnp_t *pnp;
    isacard_t *card;

    pnp = malloc(sizeof(isapnp_t));
    memset(pnp, 0, sizeof(isapnp_t));
    pnp->boards_found = 0;
    pnp->device = DevAddDevice(drv, &pnp_vtbl, 0, name, cfg, pnp);

    TRACE0("Detecting ISAPnP devices\n");
    pnp->read_port = MIN_READ_ADDR;

    /* All cards now isolated, read the first one */
    pnp->port = MIN_READ_ADDR;
    card = malloc(sizeof(isacard_t));
    for (; pnp->port <= MAX_READ_ADDR; pnp->port += READ_ADDR_STEP)
    {

        /* Make sure all cards are in Wait For Key */
        out(PNP_ADR_PORT, 0x02); /* CONFIGCONTROL */
        pnp_poke(CONFIG_WAIT_FOR_KEY);

        /* Make them listen */
        pnp_send_key();

        /* Reset the cards */
        out(PNP_ADR_PORT, 0x02); /* CONFIGCONTROL */
        pnp_poke(CONFIG_RESET_CSN | CONFIG_WAIT_FOR_KEY);
        ArchMicroDelay(2000);

        /* Send the key again */
        pnp_send_key();
        TRACE1("Trying port address %04x\r", pnp->port);

        /* Look for a PnP board */
        PnpInitCard(pnp, card);
        if (pnp_isolate(pnp, card))
        {
            TRACE1("Found one board on port %04x\n", pnp->port);
            break;
        }
    }

    if (pnp->port > MAX_READ_ADDR)
        TRACE0("No boards found\n");
    else
    {
        /*
         * found one board: print info then isolate the other boards
         * and print info for them
         */
        do
        {
            PnpAddCard(pnp, card);
            card = malloc(sizeof(isacard_t));
            PnpInitCard(pnp, card);
        } while (pnp_isolate(pnp, card));
    }

    /* The last pnp_isolate failed, so this card is not needed */
    free(card);
}

bool DrvInit(driver_t *drv)
{
    drv->add_device = PnpAddDevice;
    return true;
}
