/* $Id: ne2000.c,v 1.1 2002/12/21 09:48:59 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <os/syscall.h>
#include <kernel/net.h>

#include <errno.h>
#include <stdio.h>

typedef struct ne2000_t ne2000_t;
struct ne2000_t
{
    device_t dev;
    uint16_t iobase;

    uint8_t saprom[16];
    uint8_t station_address[6];
    bool word_mode;
    void *recv_pc_buff;
    size_t packet_length;
    uint16_t next_packet;
};

#define COMMAND                 0
#define PAGESTART               1
#define PAGESTOP                2
#define BOUNDARY                3
#define TRANSMITSTATUS          4
#define TRANSMITPAGE            4
#define TRANSMITBYTECOUNT0      5
#define NCR                     5
#define TRANSMITBYTECOUNT1      6
#define INTERRUPTSTATUS         7
#define CURRENT                 7       /* in page 1 */
#define REMOTESTARTADDRESS0     8
#define CRDMA0                  8
#define REMOTESTARTADDRESS1     9
#define CRDMA1                  9
#define REMOTEBYTECOUNT0        10
#define REMOTEBYTECOUNT1        11
#define RECEIVESTATUS           12
#define RECEIVECONFIGURATION    12
#define TRANSMITCONFIGURATION   13
#define FAE_TALLY               13
#define DATACONFIGURATION       14
#define CRC_TALLY               14
#define INTERRUPTMASK           15
#define MISS_PKT_TALLY          15
#define IOPORT                  16
#define PSTART                  0x46
#define PSTOP                   0x80
#define TRANSMITBUFFER          0x40

/* Bits in PGX_CR - Command Register */
#define CR_STP                  0x01    /* Stop chip */
#define CR_STA                  0x02    /* Start chip */
#define CR_TXP                  0x04    /* Transmit a frame */
#define CR_RD0                  0x08    /* Remote read */
#define CR_RD1                  0x10    /* Remote write */
#define CR_RD2                  0x20    /* Abort/complete remote DMA */
#define CR_PAGE0                0x00    /* Select page 0 of chip registers */
#define CR_PAGE1                0x40    /* Select page 1 of chip registers */
#define CR_PAGE2                0x80    /* Select page 2 of chip registers */

#define rcr                     0       /* value for Recv config. reg */
#define tcr                     0       /* value for trans. config. reg */
#define dcr                     0x58    /* value for data config. reg */
#define imr                     0x7f /*0x4b*/    /* value for intr. mask reg */

/* NE2000 specific implementation registers */
#define NE_RESET                0x1f        /* Reset */
#define NE_DATA                 0x10        /* Data port (use for PROM) */

static bool NeReadStationAddress(ne2000_t *ne)
{
    unsigned i;
    uint8_t Buffer[32];
    uint8_t WordLength;

    /* Read Station Address PROM (SAPROM) which is 16 bytes at remote DMA address 0.
       Some cards double the data read which we must compensate for */

    /* Initialize RBCR0 and RBCR1 - Remote Byte Count Registers */
    out(ne->iobase + REMOTEBYTECOUNT0, 0x20);
    out(ne->iobase + REMOTEBYTECOUNT1, 0x00);

    /* Initialize RSAR0 and RSAR1 - Remote Start Address Registers */
    out(ne->iobase + REMOTESTARTADDRESS0, 0x00);
    out(ne->iobase + REMOTESTARTADDRESS1, 0x00);

    /* Select page 0, read and start the NIC */
    out(ne->iobase + COMMAND, CR_STA | CR_RD0 | CR_PAGE0);

    /* Read one byte at a time */
    WordLength = 2; /* Assume a word is two bytes */
    for (i = 0; i < 32; i += 2)
    {
        Buffer[i] = in(ne->iobase + IOPORT);
        Buffer[i + 1] = in(ne->iobase + IOPORT);
		if (Buffer[i] != Buffer[i + 1])
			WordLength = 1; /* A word is one byte long */
	}

    /* If WordLength is 2 the data read before was doubled. We must compensate for this */
    if (WordLength == 2)
    {
        wprintf(L"ne2000: NE2000 or compatible network adapter found.\n");

        ne->word_mode = true;

        /* Move the SAPROM data to the adapter object */
        for (i = 0; i < 16; i++)
            ne->saprom[i] = Buffer[i * 2];

        /* Copy the station address */
        memcpy(&ne->station_address,
            &ne->saprom,
            sizeof(ne->station_address));

        /* Initialize DCR - Data Configuration Register (word mode/4 words FIFO) */
        out(ne->iobase + DATACONFIGURATION, dcr);

        return true;
    }
    else
    {
        wprintf(L"ne2000: NE1000 or compatible network adapter found.\n");
        ne->word_mode = false;
        return false;
    }
}

void NeInit(ne2000_t *ne)
{
    wprintf(L"ne2000: resetting...");
    out(ne->iobase + NE_RESET, in(ne->iobase + NE_RESET));  /* reset the NE2000*/
    while ((in(ne->iobase + INTERRUPTSTATUS) & 0x80) == 0)
        ;

    out(ne->iobase + INTERRUPTSTATUS, 0xff);        /* clear all pending ints*/
    wprintf(L"done\n");

    out(ne->iobase + COMMAND, 0x21);                /* stop mode */
    out(ne->iobase + DATACONFIGURATION, dcr);       /* data configuration register */
    out(ne->iobase + REMOTEBYTECOUNT0, 0);          /* low remote byte count */
    out(ne->iobase + REMOTEBYTECOUNT1, 0);          /* high remote byte count */
    out(ne->iobase + RECEIVECONFIGURATION, rcr);    /* receive configuration register */
    out(ne->iobase + TRANSMITPAGE, 0x20);           /* transmit page start */
    out(ne->iobase + TRANSMITCONFIGURATION, 2);     /* temporarily go into Loopback mode */
    out(ne->iobase + PAGESTART, 0x26);              /* page start */
    out(ne->iobase + BOUNDARY, 0x26);               /* boundary register */
    out(ne->iobase + PAGESTOP, 0x40);               /* page stop */
    out(ne->iobase + COMMAND, 0x61);                /* go to page 1 registers */
    out(ne->iobase + CURRENT, 0x26);                /* current page register */
    out(ne->iobase + COMMAND, CR_STA | CR_RD2);     /* back to page 0, start mode */
    out(ne->iobase + INTERRUPTSTATUS, 0xff);        /* interrupt status register */
    out(ne->iobase + INTERRUPTMASK, imr);           /* interrupt mask register */
    out(ne->iobase + TRANSMITCONFIGURATION, tcr);   /* TCR in norma1 mode, NIC is now
                                                       ready for reception */

    NeReadStationAddress(ne);
}

void NePcToNic(ne2000_t *ne, const void *packet, size_t length, uint16_t page)
{
    const uint8_t *data;
    uint8_t s;

    length = (length + 1) & ~1;                                     /* make even */
    out(ne->iobase + REMOTEBYTECOUNT0, (length >> 0) & 0x00ff);     /* set byte count low byte */
    out(ne->iobase + REMOTEBYTECOUNT1, (length >> 8) & 0x00ff);     /* set byte count high byte */
    out(ne->iobase + REMOTESTARTADDRESS0, (page >> 0) & 0x00ff);    /* set as lo address */
    out(ne->iobase + REMOTESTARTADDRESS1, (page >> 8) & 0x00ff);    /* set as hi address */
    out(ne->iobase + COMMAND, CR_STA | CR_RD1);

    data = packet;
    while (length > 0)
    {
        out(ne->iobase + IOPORT, *data);
        ArchMicroDelay(100);
        data++;
        length--;
    }

    while (((s = in(ne->iobase + INTERRUPTSTATUS)) & 0x40) == 0)
        wprintf(L"<%02x>", s);

    out(ne->iobase + INTERRUPTSTATUS, 0x40);
}

void NeNicToPc(ne2000_t *ne, void *packet, size_t length, uint16_t page)
{
    uint8_t *data;
    uint8_t s;

    length = (length + 1) & ~1; /* make even */
    out(ne->iobase + REMOTEBYTECOUNT0, (length >> 0) & 0x00ff);     /* set byte count low byte */
    out(ne->iobase + REMOTEBYTECOUNT1, (length >> 8) & 0x00ff);     /* set byte count high byte */
    out(ne->iobase + REMOTESTARTADDRESS0, (page >> 0) & 0x00ff);    /* set as lo address */
    out(ne->iobase + REMOTESTARTADDRESS1, (page >> 8) & 0x00ff);    /* set as hi address */
    out(ne->iobase + COMMAND, CR_STA | CR_RD0);

    data = packet;
    while (length > 0)
    {
        *data = in(ne->iobase + IOPORT);
        data++;
        length--;
    }

    while (((s = in(ne->iobase + INTERRUPTSTATUS)) & 0x40) == 0)
        wprintf(L"(%02x)", s);

    out(ne->iobase + INTERRUPTSTATUS, 0x40);
}

void NeSend(ne2000_t *ne, const void *packet, size_t length)
{
    uint8_t s;

    /* read NIC command register */
    //wprintf(L"/");
    while ((s = in(ne->iobase + COMMAND)) & CR_TXP)
    {
        /* transmitting? */
        /* if so, queue packet */
        ArchMicroDelay(100);
        //NeQueuePacket(ne);
        //wprintf(L"{%02x}", s);
    }

    //wprintf(L"\\");
    //wprintf(L"NeSend: command = %02x\n", s);
    NePcToNic(ne, packet, length, TRANSMITBUFFER << 8);
    out(ne->iobase + TRANSMITPAGE, TRANSMITBUFFER);
    out(ne->iobase + TRANSMITBYTECOUNT0, length & 0x00ff);
    out(ne->iobase + TRANSMITBYTECOUNT1, length >> 8);
    out(ne->iobase + COMMAND, CR_STA | CR_TXP | CR_RD2);
}

bool NeCheckQueue(ne2000_t *ne, const void **packet, size_t *length)
{
    return false;
}

bool NeRequest(device_t* dev, request_t* req)
{
	switch (req->code)
	{
	case DEV_REMOVE:
		free(dev);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

bool NeIsr(device_t *dev, uint8_t irq)
{
    ne2000_t *ne;
    uint8_t isr;

    ne = (ne2000_t*) dev;
    while (true)
    {
        isr = in(ne->iobase + INTERRUPTSTATUS);
        wprintf(L"ne2000: isr = %02x\n", isr);
        if (isr & 1)        /* packet received? */
        {
            uint8_t boundary, cur_reg;
            unsigned i;

            wprintf(L"ne2000: packet received\n");
            if (isr & 0x10) /* overflow */
            {
                out(ne->iobase + COMMAND, CR_RD2 | CR_STP);         /* put NIC in stop mode */
                out(ne->iobase + REMOTEBYTECOUNT0, 0);
                out(ne->iobase + REMOTEBYTECOUNT1, 0);
                for (i = 0; i < 0x7fff; i++)
                    if (in(ne->iobase + INTERRUPTSTATUS) & 0x80)    /* look for RST bit to be set */
                    {
                        /*
                         * if we fall thru this loop, the RST bit may not get 
                         * set because the NIC was currently transmitting
                         */
                        break;
                    }

                out(ne->iobase + TRANSMITCONFIGURATION, 2);         /* into loopback mode 1 */
                out(ne->iobase + COMMAND, CR_RD2 | CR_STP);         /* into stop mode */
                NeNicToPc(ne, ne->recv_pc_buff, ne->packet_length, ne->next_packet);
                out(ne->iobase + INTERRUPTSTATUS, 0x10);            /* clear Overflow bit */
                out(ne->iobase + TRANSMITCONFIGURATION, tcr);       /* put TCR back to normal mode */
            }

            do
            {
                out(ne->iobase + INTERRUPTSTATUS, 1);
                NeNicToPc(ne, ne->recv_pc_buff, ne->packet_length, ne->next_packet);

                /* xxx - Inform upper layer software of a received packet to be processed */

                /* checking to see if receive buffer ring is empty */
                boundary = in(ne->iobase + BOUNDARY);       /* save BOUNDARY in ah */
                out(ne->iobase + COMMAND, CR_PAGE1 | CR_RD2 | CR_STA); /* switched to pg 1 of NIC */
                cur_reg = in(ne->iobase + CURRENT);         /* bh 4 CURRENT PAGE register */
                out(ne->iobase + COMMAND, CR_RD2 | CR_STA); /* switched back to pg 0 */
            } while (cur_reg != boundary);                  /* recv buff ring empty? */
        }
        else if (isr & 0xa) /* packet transmitted? */
        {
            const void *buf;
            size_t length;

            wprintf(L"ne2000: packet transmitted\n");
            out(ne->iobase + INTERRUPTSTATUS, 0x0a);        /* reset PTX and TXE bits in ISR */
            if (in(ne->iobase + TRANSMITSTATUS) & 0x38)     /* check for erroneous TX */
            {
                /* is FU, CRS, or ABT bits set in TSR */
                /* xxx - Inform upper layer software of erroneous transmission */
                wprintf(L"ne2000: erroneous transmission\n");
            }

            while (NeCheckQueue(ne, &buf, &length))         /* see if a packet is in queue */
                NeSend(ne, buf, length);

            /* xxx - Inform upper layer software of successful transmission */
        }
        else
            break;
    }

    out(ne->iobase + INTERRUPTMASK, imr);
    return true;
}

static const device_vtbl_t ne_vtbl = 
{
    NeRequest,
    NeIsr,
    NULL,
};

typedef struct 
{
    net_ether ether;
    net_arp arp;
} arp_packet;

static unsigned char bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

void issue_rarp_request(ne2000_t *ne)
{
    arp_packet req;

    req.ether.type = htons(0x8035);    
    memcpy(&req.ether.src, ne->station_address, 6);
    memcpy(&req.ether.dst, bcast, 6);
    req.arp.arp_hard_type = htons(1);
    req.arp.arp_prot_type = htons(0x0800);
    req.arp.arp_hard_size = 6;
    req.arp.arp_prot_size = 4;            
    req.arp.arp_op = htons(RARP_OP_REQUEST);
    memcpy(&req.arp.arp_enet_sender, ne->station_address, 6);
    memcpy(&req.arp.arp_ip_sender, bcast, 4);
    memcpy(&req.arp.arp_enet_target, ne->station_address, 6);
    memcpy(&req.arp.arp_ip_target, bcast, 4);

    NeSend(ne, &req, sizeof(req));
}

void NeAddDevice(driver_t *drv, const wchar_t *name, device_config_t *cfg)
{
	ne2000_t* ne;

	ne = malloc(sizeof(device_t));
	ne->dev.vtbl = &ne_vtbl;
	ne->dev.driver = drv;
    ne->iobase = 0x300;

    if (cfg != NULL)
        cfg->device_class = 0x0200;
	DevAddDevice(&ne->dev, name, cfg);
    DevRegisterIrq(12, &ne->dev);

    wprintf(L"ne2000: starting card\n");
    NeInit(ne);
    wprintf(L"ne2000: station address is %02x-%02x-%02x-%02x-%02x-%02x\n",
        ne->station_address[0], ne->station_address[1], ne->station_address[2], 
        ne->station_address[3], ne->station_address[4], ne->station_address[5]);

    while (true)
        issue_rarp_request(ne);
}

bool DrvInit(driver_t *drv)
{
	drv->add_device = NeAddDevice;
	return true;
}
