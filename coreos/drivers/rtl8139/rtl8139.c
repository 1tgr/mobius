/* $Id: rtl8139.c,v 1.1.1.1 2002/12/21 09:49:01 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/thread.h>
#include <kernel/net.h>
#include <kernel/syscall.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct rxpacket_t rxpacket_t;
struct rxpacket_t
{
    rxpacket_t *prev, *next;
    size_t length;
    uint16_t type;
    uint8_t data[1];
};

typedef struct txpacket_t txpacket_t;
struct txpacket_t
{
    int buffer;
    unsigned timeout;
};

typedef struct rtl8139_t rtl8139_t;
struct rtl8139_t
{
    device_t *device;

    spinlock_t sem;
    uint16_t iobase;
    uint8_t station_address[6];
    bool speed10;
    bool fullduplex;
    unsigned cur_tx;
    unsigned cur_rx;

    addr_t rx_phys, tx_phys;
    uint8_t *rx_ring, *tx_ring;

    rxpacket_t *packet_first, *packet_last;
};

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH      256             /* In bytes, rounded down to 32 byte units. */
#define RX_FIFO_THRESH      4               /* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST        4               /* Maximum PCI burst, '4' is 256 bytes */
#define TX_DMA_BURST        4               /* Calculate as 16<<val. */
#define NUM_TX_DESC         4               /* Number of Tx descriptor registers. */
#define TX_BUF_SIZE         ETH_FRAME_LEN   /* FCS is added by the chip */
#define RX_BUF_LEN_IDX      0               /* 0, 1, 2 is allowed - 8,16,32K rx buffer */
#define RX_BUF_LEN          (PAGE_SIZE << RX_BUF_LEN_IDX)

/* Symbolic offsets to registers. */
enum RTL8139_registers
{
    MAC0=0,             /* Ethernet hardware address. */
    MAR0=8,             /* Multicast filter. */
    TxStatus0=0x10,     /* Transmit status (four 32bit registers). */
    TxAddr0=0x20,       /* Tx descriptors (also four 32bit). */
    RxBuf=0x30, RxEarlyCnt=0x34, RxEarlyStatus=0x36,
    ChipCmd=0x37, RxBufPtr=0x38, RxBufAddr=0x3A,
    IntrMask=0x3C, IntrStatus=0x3E,
    TxConfig=0x40, RxConfig=0x44,
    Timer=0x48,         /* general-purpose counter. */
    RxMissed=0x4C,      /* 24 bits valid, write clears. */
    Cfg9346=0x50, Config0=0x51, Config1=0x52,
    TimerIntrReg=0x54,  /* intr if gp counter reaches this value */
    MediaStatus=0x58,
    Config3=0x59,
    MultiIntr=0x5C,
    RevisionID=0x5E,    /* revision of the RTL8139 chip */
    TxSummary=0x60,
    MII_BMCR=0x62, MII_BMSR=0x64, NWayAdvert=0x66, NWayLPAR=0x68,
    NWayExpansion=0x6A,
    DisconnectCnt=0x6C, FalseCarrierCnt=0x6E,
    NWayTestReg=0x70,
    RxCnt=0x72,         /* packet received counter */
    CSCR=0x74,          /* chip status and configuration register */
    PhyParm1=0x78,TwisterParm=0x7c,PhyParm2=0x80,   /* undocumented */
    /* from 0x84 onwards are a number of power management/wakeup frame
     * definitions we will probably never need to know about.  */
};

enum ChipCmdBits
{
    CmdReset=0x10, CmdRxEnb=0x08, CmdTxEnb=0x04, RxBufEmpty=0x01,
};

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits
{
    PCIErr=0x8000, PCSTimeout=0x4000, CableLenChange= 0x2000,
    RxFIFOOver=0x40, RxUnderrun=0x20, RxOverflow=0x10,
    TxErr=0x08, TxOK=0x04, RxErr=0x02, RxOK=0x01,
    IntrDefault = RxOK | TxOK,
};

enum TxStatusBits
{
    TxHostOwns=0x2000, TxUnderrun=0x4000, TxStatOK=0x8000,
    TxOutOfWindow=0x20000000, TxAborted=0x40000000,
    TxCarrierLost=0x80000000,
};

enum RxStatusBits
{
    RxMulticast=0x8000, RxPhysical=0x4000, RxBroadcast=0x2000,
    RxBadSymbol=0x0020, RxRunt=0x0010, RxTooLong=0x0008, RxCRCErr=0x0004,
    RxBadAlign=0x0002, RxStatusOK=0x0001,
};

enum MediaStatusBits
{
    MSRTxFlowEnable=0x80, MSRRxFlowEnable=0x40, MSRSpeed10=0x08,
    MSRLinkFail=0x04, MSRRxPauseFlag=0x02, MSRTxPauseFlag=0x01,
};

enum MIIBMCRBits
{
    BMCRReset=0x8000, BMCRSpeed100=0x2000, BMCRNWayEnable=0x1000,
    BMCRRestartNWay=0x0200, BMCRDuplex=0x0100,
};

enum CSCRBits
{
    CSCR_LinkOKBit=0x0400, CSCR_LinkChangeBit=0x0800,
    CSCR_LinkStatusBits=0x0f000, CSCR_LinkDownOffCmd=0x003c0,
    CSCR_LinkDownCmd=0x0f3c0,
};

/* Bits in RxConfig. */
enum rx_mode_bits
{
    RxCfgWrap=0x80,
    AcceptErr=0x20, AcceptRunt=0x10, AcceptBroadcast=0x08,
    AcceptMulticast=0x04, AcceptMyPhys=0x02, AcceptAllPhys=0x01,
};

/* Serial EEPROM section. */

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK    0x04    /* EEPROM shift clock. */
#define EE_CS           0x08    /* EEPROM chip select. */
#define EE_DATA_WRITE   0x02    /* EEPROM chip data in. */
#define EE_WRITE_0      0x00
#define EE_WRITE_1      0x02
#define EE_DATA_READ    0x01    /* EEPROM chip data out. */
#define EE_ENB          (0x80 | EE_CS)

/*
    Delay between EEPROM clock transitions.
    No extra delay is needed with 33Mhz PCI, but 66Mhz may change this.
*/

#define eeprom_delay()  in32(ee_addr)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD    (5 << 6)
#define EE_READ_CMD     (6 << 6)
#define EE_ERASE_CMD    (7 << 6)

static unsigned RtlReadEeprom(rtl8139_t *rtl, unsigned location)
{
    int i;
    unsigned int retval = 0;
    long ee_addr = rtl->iobase + Cfg9346;
    int read_cmd = location | EE_READ_CMD;

    out(ee_addr, EE_ENB & ~EE_CS);
    out(ee_addr, EE_ENB);

    /* Shift the read command bits out. */
    for (i = 10; i >= 0; i--)
    {
        int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
        out(ee_addr, EE_ENB | dataval);
        eeprom_delay();
        out(ee_addr, EE_ENB | dataval | EE_SHIFT_CLK);
        eeprom_delay();
    }

    out(ee_addr, EE_ENB);
    eeprom_delay();

    for (i = 16; i > 0; i--)
    {
        out(ee_addr, EE_ENB | EE_SHIFT_CLK);
        eeprom_delay();
        retval = (retval << 1) | ((in(ee_addr) & EE_DATA_READ) ? 1 : 0);
        out(ee_addr, EE_ENB);
        eeprom_delay();
    }

    /* Terminate the EEPROM access. */
    out(ee_addr, ~EE_CS);
    return retval;
}

void RtlReset(rtl8139_t *rtl)
{
    unsigned i;

    out(rtl->iobase + ChipCmd, CmdReset);

    rtl->cur_rx = 0;
    rtl->cur_tx = 0;

    /* Give the chip 10ms to finish the reset. */
    i = WrapSysUpTime() + 10;
    while ((in(rtl->iobase + ChipCmd) & CmdReset) != 0 && 
        WrapSysUpTime() < i)
        /* wait */;

    for (i = 0; i < _countof(rtl->station_address); i++)
        out(rtl->iobase + MAC0 + i, rtl->station_address[i]);

    /* Must enable Tx/Rx before setting transfer thresholds! */
    out(rtl->iobase + ChipCmd, CmdRxEnb | CmdTxEnb);
    out32(rtl->iobase + RxConfig, 
        (RX_FIFO_THRESH<<13) | (RX_BUF_LEN_IDX<<11) | (RX_DMA_BURST<<8)); /* accept no frames yet!  */
    out32(rtl->iobase + TxConfig, (TX_DMA_BURST<<8)|0x03000000);

    /* The Linux driver changes Config1 here to use a different LED pattern
     * for half duplex or full/autodetect duplex (for full/autodetect, the
     * outputs are TX/RX, Link10/100, FULL, while for half duplex it uses
     * TX/RX, Link100, Link10).  This is messy, because it doesn't match
     * the inscription on the mounting bracket.  It should not be changed
     * from the configuration EEPROM default, because the card manufacturer
     * should have set that to match the card.  */

    out32(rtl->iobase + RxBuf, rtl->rx_phys);

    /* Start the chip's Tx and Rx process. */
    out32(rtl->iobase + RxMissed, 0);
    /* set_rx_mode */
    out(rtl->iobase + RxConfig, AcceptBroadcast|AcceptMyPhys);
    /* If we add multicast support, the MAR0 register would have to be
     * initialized to 0xffffffffffffffff (two 32 bit accesses).  Etherboot
     * only needs broadcast (for ARP/RARP/BOOTP/DHCP) and unicast.  */
    out(rtl->iobase + ChipCmd, CmdRxEnb | CmdTxEnb);

    /* Disable all known interrupts by setting the interrupt mask. */
    //out16(rtl->iobase + IntrMask, 0);
    out16(rtl->iobase + IntrMask, IntrDefault);
}

void RtlInit(rtl8139_t *rtl)
{
    unsigned i;

    wprintf(L"rtl8139: resetting... ");

    SpinAcquire(&rtl->sem);

    /* Bring the chip out of low-power mode. */
    out(rtl->iobase + Config1, 0x00);

    if (RtlReadEeprom(rtl, 0) != 0xffff)
    {
        unsigned short *ap = (unsigned short*)rtl->station_address;
        for (i = 0; i < 3; i++)
            *ap++ = RtlReadEeprom(rtl, i + 7);
    }
    else
    {
        unsigned char *ap = (unsigned char*)rtl->station_address;
        for (i = 0; i < 6; i++)
            *ap++ = in(rtl->iobase + MAC0 + i);
    }

    rtl->speed10 = (in(rtl->iobase + MediaStatus) & MSRSpeed10) != 0;
    rtl->fullduplex = (in16(rtl->iobase + MII_BMCR) & BMCRDuplex) != 0;
    wprintf(L"rtl8139: %sMbps %s-duplex\n", 
        rtl->speed10 ? L"10" : L"100",
        rtl->fullduplex ? L"full" : L"half");

    rtl->rx_phys = MemAlloc();
    rtl->tx_phys = MemAlloc();

    rtl->rx_ring = sbrk_virtual(RX_BUF_LEN);
    rtl->tx_ring = sbrk_virtual(TX_BUF_SIZE);

    wprintf(L"rtl8139: rx_ring = %p, tx_ring = %p\n",
        rtl->rx_ring, rtl->tx_ring);

    MemMapRange(rtl->rx_ring, 
        rtl->rx_phys, 
        rtl->rx_ring + RX_BUF_LEN,
        PRIV_RD | PRIV_PRES | PRIV_KERN);
    MemMapRange(rtl->tx_ring, 
        rtl->tx_phys, 
        (uint8_t*) PAGE_ALIGN_UP((addr_t) rtl->tx_ring + TX_BUF_SIZE),
        PRIV_WR | PRIV_PRES | PRIV_KERN);

    RtlReset(rtl);
    SpinRelease(&rtl->sem);
}

void RtlStartIo(rtl8139_t *rtl)
{
    asyncio_t *io, *ionext;
    unsigned long len;
    void *buf;
    rxpacket_t *packet, *next;
    request_eth_t *req_eth;
    bool foundone;
    txpacket_t *tx;

    for (io = rtl->device->io_first; io != NULL; io = ionext)
    {
        ionext = io->next;
        req_eth = (request_eth_t*) io->req;

        switch (io->req->code)
        {
        case ETH_SEND:
            assert(io->extra == NULL);
            tx = malloc(sizeof(txpacket_t));
            if (tx == NULL)
                continue;

            if (!(in32(rtl->iobase + TxStatus0 + rtl->cur_tx * 4) & TxHostOwns))
            {
                wprintf(L"rtl8139: buffer %u not available\n", rtl->cur_tx);
                continue;
            }

            memcpy(rtl->tx_ring, req_eth->params.eth_send.to, ETH_ALEN);
            memcpy(rtl->tx_ring + ETH_ALEN, rtl->station_address, ETH_ALEN);
            memcpy(rtl->tx_ring + 2 * ETH_ALEN, &req_eth->params.eth_send.type, 2);

            len = min(io->length, ETH_MAX_MTU);
            buf = MemMapPageArray(io->pages);
            memcpy(rtl->tx_ring + ETH_HLEN, buf, len);
            MemUnmapPageArray(io->pages);

            len += ETH_HLEN;
            wprintf(L"rtl8139: sending %d bytes, cur_tx = %u... ", len, rtl->cur_tx);

            /* Note: RTL8139 doesn't auto-pad, send minimum payload (another 4
             * bytes are sent automatically for the FCS, totalling to 64 bytes). */
            while (len < ETH_ZLEN)
                rtl->tx_ring[len++] = '\0';

            tx->buffer = rtl->cur_tx;
            /* xxx -- this isn't used yet */
            tx->timeout = WrapSysUpTime() + 1000;
            io->extra = tx;

            out32(rtl->iobase + TxAddr0 + rtl->cur_tx * 4, rtl->tx_phys);
            out32(rtl->iobase + TxStatus0 + rtl->cur_tx * 4, ((TX_FIFO_THRESH<<11) & 0x003f0000) | len);
            break;

        case ETH_RECEIVE:
            //wprintf(L"RtlStartIo: ETH_RECEIVE\n");

            for (packet = rtl->packet_first; packet != NULL; packet = next)
            {
                next = packet->next;
                foundone = false;

                /*wprintf(L"ETH_RECEIVE: looking for type %04x, got type %04x\n",
                    req_eth->params.eth_receive.type,
                    packet->type);*/

                if (req_eth->params.eth_receive.type == 0xffff ||
                    packet->type == req_eth->params.eth_receive.type)
                {
                    req_eth->params.eth_receive.length = 
                        min(packet->length, req_eth->params.eth_receive.length);

                    buf = MemMapPageArray(io->pages);
                    memcpy(buf, 
                        packet->data, 
                        req_eth->params.eth_receive.length);
                    MemUnmapPageArray(io->pages);

                    if (packet->prev != NULL)
                        packet->prev->next = packet->next;
                    if (packet->next != NULL)
                        packet->next->prev = packet->prev;
                    if (rtl->packet_first == packet)
                        rtl->packet_first = packet->next;
                    if (rtl->packet_last == packet)
                        rtl->packet_last = packet->next;

                    free(packet);
                    DevFinishIo(rtl->device, io, 0);
                    foundone = true;
                }
            }

            break;
        }
    }
}

void RtlHandleRx(rtl8139_t *rtl)
{
    uint32_t ring_offs, rx_size, rx_status;
    rxpacket_t *packet;

    ring_offs = rtl->cur_rx % RX_BUF_LEN;
    rx_status = *(uint32_t*) (rtl->rx_ring + ring_offs);
    rx_size = rx_status >> 16;
    rx_status &= 0xffff;

    if ((rx_status & (RxBadSymbol | RxRunt | RxTooLong | RxCRCErr | RxBadAlign)) ||
        (rx_size < ETH_ZLEN) || 
        (rx_size > ETH_FRAME_LEN + 4))
    {
        wprintf(L"rx error 0x%x\n", rx_status);
        RtlReset(rtl);  /* this clears all interrupts still pending */
        RtlStartIo(rtl);
        return;
    }

    packet = malloc(sizeof(rxpacket_t) - 1 + rx_size - 4);
    if (packet == NULL)
        return;

    packet->length = rx_size - 4;   /* no one cares about the FCS */
    /* Received a good packet */
    if (ring_offs + 4 + rx_size - 4 > RX_BUF_LEN)
    {
        int semi_count = RX_BUF_LEN - ring_offs - 4;

        memcpy(packet->data, rtl->rx_ring + ring_offs + 4, semi_count);
        memcpy(packet->data + semi_count, rtl->rx_ring, rx_size - 4 - semi_count);
        //wprintf(L"rx packet %d+%d bytes", semi_count,rx_size - 4 - semi_count);
    }
    else
    {
        memcpy(packet->data, rtl->rx_ring + ring_offs + 4, packet->length);
        //wprintf(L"rx packet %d bytes", rx_size-4);
    }

    /*wprintf(L" at %X type %02X%02X rxstatus %hX\n",
        (unsigned long)(rtl->rx_ring + ring_offs+4),
        packet->data[12], packet->data[13], rx_status);*/

    packet->type = *(unsigned short*) (packet->data + 12);
    LIST_ADD(rtl->packet, packet);

    rtl->cur_rx = (rtl->cur_rx + rx_size + 4 + 3) & ~3;
    out16(rtl->iobase + RxBufPtr, rtl->cur_rx - 16);

    RtlStartIo(rtl);
}

void RtlHandleTx(rtl8139_t *rtl)
{
    asyncio_t *io, *ionext;
    txpacket_t *tx;

    rtl->cur_tx = (rtl->cur_tx + 1) % NUM_TX_DESC;

    assert(rtl->device->io_first != NULL);
    for (io = rtl->device->io_first; io != NULL; io = ionext)
    {
        ionext = io->next;

        if (io->req->code == ETH_SEND)
        {
            tx = io->extra;
            assert(tx != NULL);
            assert(tx->buffer != -1);
            if ((in32(rtl->iobase + TxStatus0 + tx->buffer * 4) & TxHostOwns))
            {
                wprintf(L"buffer %u (%p) sent\n", tx->buffer, tx);
                free(tx);
                DevFinishIo(rtl->device, io, 0);
            }
            else
                wprintf(L"host doesn't own buffer %u (%p)\n", tx->buffer, tx);
        }
    }

    RtlStartIo(rtl);
}


static void RtlDeleteDevice(device_t *device)
{
	free(device->cookie);
}


static status_t RtlRequest(device_t *device, request_t* req)
{
    rtl8139_t *rtl;
    request_eth_t *req_eth;
    request_net_t *req_net;
    asyncio_t *io;
    bool was_empty;
    net_hwaddr_t *addr;

    rtl = device->cookie;
    req_eth = (request_eth_t*) req;
    req_net = (request_net_t*) req;
    switch (req->code)
    {
    case ETH_SEND:
    case ETH_RECEIVE:
        if (req->code == ETH_SEND)
        {
            was_empty = true;
            for (io = device->io_first; io != NULL; io = io->next)
                if (io->req->code == ETH_SEND)
                    was_empty = false;
        }
        else
            was_empty = true;

        io = DevQueueRequest(device, 
            &req_eth->header, 
            sizeof(*req_eth),
            req_eth->params.buffered.pages,
            req_eth->params.buffered.length);

        if (io == NULL)
        {
            req->result = errno;
            return false;
        }

        io->extra = NULL;
        if (was_empty)
        {
            SpinAcquire(&rtl->sem);
            RtlStartIo(rtl);
            SpinRelease(&rtl->sem);
        }

        return true;

    case NET_GET_HW_INFO:
        addr = req_net->params.net_hw_info.addr;
        if (addr != NULL)
        {
            if (req_net->params.net_hw_info.addr_data_size < 6)
            {
                req->result = EBUFFER;
                req_net->params.net_hw_info.addr_data_size = 6;
                return false;
            }

            memcpy(addr->u.ethernet, rtl->station_address, 6);
            addr->type = NET_HW_ETHERNET;
            addr->data_size = 6;
        }

        req_net->params.net_hw_info.addr_data_size = 6;
        req_net->params.net_hw_info.mtu = ETH_MAX_MTU;
        return true;
    }

    req->result = ENOTIMPL;
    return false;
}

bool RtlIsr(device_t *device, uint8_t irq)
{
    rtl8139_t *rtl;
    uint16_t status;

    rtl = device->cookie;

    SpinAcquire(&rtl->sem);
    out16(rtl->iobase + IntrMask, 0);

    status = in16(rtl->iobase + IntrStatus);
    out16(rtl->iobase + IntrStatus, status & ~(RxFIFOOver | RxOverflow | RxOK));

    if (status & (RxOK | RxErr))
        RtlHandleRx(rtl);
    else if (status & TxOK)
        RtlHandleTx(rtl);
    else
        wprintf(L"rtl8139: unknown interrupt: isr = %04x\n", status);

    out16(rtl->iobase + IntrStatus, status & (RxFIFOOver | RxOverflow | RxOK));
    out16(rtl->iobase + IntrMask, IntrDefault);
    SpinRelease(&rtl->sem);

    return true;
}

static const device_vtbl_t rtl_vtbl = 
{
	RtlDeleteDevice,
    RtlRequest,
    RtlIsr,
    NULL,
};

extern net_protocol_t proto_ip, proto_arp;

void RtlAddDevice(driver_t *drv, const wchar_t *name, dev_config_t *cfg)
{
    rtl8139_t* rtl;
    unsigned i;
    uint8_t irq;

    rtl = malloc(sizeof(rtl8139_t));
    memset(rtl, 0, sizeof(rtl8139_t));
    SpinInit(&rtl->sem);

    irq = 5;
    rtl->iobase = 0x300;
    for (i = 0; i < cfg->num_resources; i++)
    {
        if (cfg->resources[i].cls == resIrq)
            irq = cfg->resources[i].u.irq;
        else if (cfg->resources[i].cls == resIo)
            rtl->iobase = cfg->resources[i].u.io.base & ~1;
    }

    rtl->device = DevAddDevice(drv, &rtl_vtbl, 0, name, cfg, rtl);
    DevRegisterIrq(irq, rtl->device);

    wprintf(L"rtl8139: starting card: io=0x%x, irq=%u\n", rtl->iobase, irq);
    RtlInit(rtl);
    wprintf(L"rtl8139: station address is %02x-%02x-%02x-%02x-%02x-%02x\n",
        rtl->station_address[0], rtl->station_address[1], rtl->station_address[2], 
        rtl->station_address[3], rtl->station_address[4], rtl->station_address[5]);

    EthBindProtocol(&proto_ip, rtl->device, htons(ETH_FRAME_IP));
    EthBindProtocol(&proto_arp, rtl->device, htons(ETH_FRAME_ARP));
}

bool DrvInit(driver_t *drv)
{
    drv->add_device = RtlAddDevice;
    return true;
}
