// Ne2000.cpp: implementation of the CNe2000 class.
//
//////////////////////////////////////////////////////////////////////

#include "Ne2000.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNe2000::CNe2000(word port) : m_port(port)
{
	byte prom[16];
	int f;

	memset(&m_stat, 0, sizeof(m_stat));
	m_pstart=0;
	m_pstop=0; 
	m_wordlength=0; 
	m_current_page=0;
	//m_notify=NULL;
	for(f=0;f<MAX_TX;f++)
	{
		m_tx_packet[f]= NULL;
/*			m_tx_packet[f].count=0;
		m_tx_packet[f].buf=NULL;
		m_tx_packet[f].page=0; */
	}
	m_last_tx=NULL;
	m_busy=0;
	m_sending=0;

	Init(prom, NULL);
	sysRegisterIrq(5, Isr, (dword) this);
	Start(true);
}

CNe2000::~CNe2000()
{

}

void CNe2000::Isr(dword ctx, int irq)
{
	dword isr;	/* Illinois Sreet Residence Hall */
	dword overload;
	CNe2000* nic = (CNe2000*) ctx;

	_cputws(L"CNe2000::Isr\n");
	out(nic->m_port, NIC_DMA_DISABLE | NIC_PAGE0);
	overload=MAX_LOAD+1;
	while ((isr=in(nic->m_port + INTERRUPTSTATUS)))
	{
		if((--overload)<=0)
			break;

		if(isr & ISR_OVW)
			nic->Overrun();
		else if(isr & (ISR_PRX | ISR_RXE))
			nic->Receive();

		if(isr & ISR_PTX)
			nic->Transmit();
		else if(isr & ISR_TXE)
			nic->TransmitError();

		if(isr & ISR_CNT)
			nic->Counters();
		if(isr & ISR_RDC)
			out(nic->m_port + INTERRUPTSTATUS, ISR_RDC);

		out(nic->m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);
	}

	if(isr)
	{
		out(nic->m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);

		if(!overload)
		{
			wprintf(L"NE2000: Too many requests in ISR.  ISR:%X  MaxLoad:%X\n",
				isr, MAX_LOAD);
			out(nic->m_port + INTERRUPTSTATUS, ISR_ALL); // clear
		} else {
			wprintf(L"NE2000: Unhandeled interrupt, ISR:%X\n",isr);
			out(nic->m_port + INTERRUPTSTATUS, 0xff);
								// Ack it anyway
		}
	}
}

static unsigned int default_ports[] = { 0x300, 0x280, 0x320, 0x340, 0x360 };

CNe2000* CNe2000::Detect()
{
	byte state, regd;
	int i;
	word port;
	
	for (i = 0; i < countof(default_ports); i++)
	{
		port = default_ports[i];

		state = in(port);
		if (in(port) == 0xff)
			wprintf(L"No NE2000 card found on port %x\n", port);
		else
		{
			out(port, NIC_DMA_DISABLE | NIC_PAGE1 | NIC_STOP);
			regd = in(port + 0x0d);
			out(port + 0x0d, 0xff);
			out(port, NIC_DMA_DISABLE | NIC_PAGE0);
			in(port + FAE_TALLY);	/* reading CNTR0 resets it.*/

			if (in(port + FAE_TALLY))
			{
				/* counter didn't clear so probe fails*/
				out(port, state);	/* restore command state*/
				out(port + 0x0d, regd);
				wprintf(L"NE2000 failure on port %x\n", port);
			}
			else
			{
				wprintf(L"NE2000 card found on port %x\n", port);
				return new CNe2000(port);
			}
		}
	}

	return NULL;
}

/* dumps the prom into a 16 byte buffer and returns the wordlength of
// the card.
// You should be able to make this procedure a wrapper of nic_block_input(). */
int CNe2000::DumpProm(byte *prom)
{
	dword f;
	char wordlength=2;		/* default wordlength of 2 */
	unsigned char dump[32];
	out(m_port + REMOTEBYTECOUNT0, 32);	  /* read 32 bytes from DMA->IO */
	out(m_port + REMOTEBYTECOUNT1, 0);  /*  this is for the PROM dump */
	out(m_port + REMOTESTARTADDRESS0, 0); /* configure DMA for 0x0000 */
	out(m_port + REMOTESTARTADDRESS1, 0);
	out(m_port, NIC_REM_READ | NIC_START);
	for(f=0;f<32;f+=2) {
		dump[f]=in(m_port + NE_DATA);
		dump[f+1]=in(m_port + NE_DATA);
		if(dump[f]!=dump[f+1]) wordlength=1;
	}
	/* if wordlength is 2 bytes, then collapse prom to 16 bytes */
	for(f=0;f<LEN_PROM;f++) prom[f]=dump[f+((wordlength==2)?f:0)];
/*	wprintf(L"NE2000: prom dump - ");
	for(f=0;f<LEN_PROM;f++) wprintf(L"%X",prom[f]);
	wprintf(L"\n");	*/

	return wordlength;
}

/* This initializes the NE2000 card.  If it turns out the card is not
// really a NE2000 after all then it will return ERRNOTFOUND, else NOERR
// It also dumps the prom into buffer prom for the upper layers..
// Pass it a nic with a null m_port and it will initialize the structure for
// you, otherwise it will just reinitialize it. */
bool CNe2000::Init(byte *prom, byte *manual)
{
	dword f;
	wprintf(L"NE2000: reseting NIC card...");
	
	out(m_port + NE_RESET, in(m_port + NE_RESET));	/* reset the NE2000*/
	while(!(in(m_port+INTERRUPTSTATUS) & ISR_RST)) {
		/* TODO insert timeout code here.*/
	}
	wprintf(L"done.\n");
	out(m_port + INTERRUPTSTATUS, 0xff);	/* clear all pending ints*/

	// Initialize registers
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP);	/* enter page 0*/
	out(m_port + DATACONFIGURATION, DCR_DEFAULT);
	out(m_port + REMOTEBYTECOUNT0, 0x00);
	out(m_port + REMOTEBYTECOUNT1, 0x00);
	out(m_port + INTERRUPTMASK, 0x00);	/* mask off all irq's*/
	out(m_port + INTERRUPTSTATUS, 0xff);	/* clear pending ints*/
	out(m_port + RECEIVECONFIGURATION, RCR_MON);	/* enter monitor mode*/
	out(m_port + TRANSMITCONFIGURATION, TCR_INTERNAL_LOOPBACK); /* internal loopback*/
	
	m_wordlength = DumpProm(prom);
	if(prom[14]!=0x57 || prom[15]!=0x57) {
		wprintf(L"NE2000: PROM signature does not match NE2000 0x57.\n");
		return false;
	}
	wprintf(L"NE2000: PROM signature matches NE2000 0x57.\n");

	/* if the wordlength for the NE2000 card is 2 bytes, then
	// we have to setup the DP8390 chipset to be the same or 
	// else all hell will break loose.*/
	if(m_wordlength==2) {
		out(m_port + DATACONFIGURATION, DCR_DEFAULT_WORD);
	}
	m_pstart=(m_wordlength==2) ? PSTARTW : PSTART;
	m_pstop=(m_wordlength==2) ? PSTOPW : PSTOP;

	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP);
	out(m_port + TRANSMITPAGE, m_pstart);	/* setup local buffer*/
	out(m_port + PAGESTART, m_pstart + TXPAGES);
	out(m_port + BOUNDARY, m_pstop - 1);
	out(m_port + PAGESTOP, m_pstop);
	out(m_port + INTERRUPTMASK, 0x00);	/* mask off all irq's*/
	out(m_port + INTERRUPTSTATUS, 0xff);	/* clear pending ints*/
	m_current_page=m_pstart + TXPAGES;
	
	/* put physical address in the registers */
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE1 | NIC_STOP);  /* switch to page 1 */
	if(manual) for(f=0;f<6;f++) out(m_port + PHYSICAL + f, manual[f]);
	else for(f=0;f<LEN_ADDR;f++) out(m_port + PHYSICAL + f, prom[f]);
	wprintf(L"NE2000: Physical Address- ");
	wprintf(L"%X:%X:%X:%X:%X:%X\n",
		in(m_port+PAR0),in(m_port+PAR1),in(m_port+PAR2),
		in(m_port+PAR3),in(m_port+PAR4),in(m_port+PAR5));

	/* setup multicast filter to accept all packets*/
	for(f=0;f<8;f++) out(m_port + MULTICAST + f, 0xFF);

	out(m_port + CURRENT, m_pstart+TXPAGES);
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP); /* switch to page 0 */

	return true;
}

void CNe2000::Start(bool promiscuous)
{
	wprintf(L"NE2000: Starting NIC.\n");
	out(m_port + INTERRUPTSTATUS, 0xff);
	out(m_port + INTERRUPTMASK, IMR_DEFAULT);
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);
	out(m_port + TRANSMITCONFIGURATION, TCR_DEFAULT);
	if(promiscuous)
		out(m_port + RECEIVECONFIGURATION, RCR_PRO | RCR_AM);
	else
		out(m_port + RECEIVECONFIGURATION, RCR_DEFAULT);

	/* The following is debugging code! */
#if 1
	wprintf(L"NE2000: Trying to fire off an IRQ.\n");
	out(m_port + INTERRUPTMASK, 0x50);
	out(m_port + REMOTEBYTECOUNT0, 0x00);
	out(m_port + REMOTEBYTECOUNT1, 0x00); 
	out(m_port, NIC_REM_READ | NIC_START);   /* this should fire off */
	out(m_port + INTERRUPTMASK, IMR_DEFAULT);  /* an interrupt... */
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);
	out(m_port + INTERRUPTSTATUS, 0xff); 
#endif
            /* End debugging code */
}

void CNe2000::Overrun()
{
	dword tx_status;
	long starttime;
	dword resend=0;
	wprintf(L"NE2000: Receive packet ring overrun!\n");
	tx_status=in(m_port) & NIC_TRANSMIT;
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_STOP);
	m_stat.errors.rx_overruns++;

	starttime=sysUpTime();
wprintf(L"BEFORE\n");
	//while(sysUpTime()-starttime<=10/*ticks to wait*/) idle();
	msleep(2);
	/* Arrgh!  TODO: Replace this whole crappy code with a decent
	// wait method.  We need to wait at least 1.6ms as per National
	// Semiconductor datasheets, but we should probablly wait a 
	// little more to be safe. */
wprintf(L"AFTER\n");

	out(m_port + REMOTEBYTECOUNT0, 0x00);
	out(m_port + REMOTEBYTECOUNT1, 0x00);
	if(tx_status)
	{
		dword tx_completed=in(m_port + INTERRUPTSTATUS) & 
			(ISR_PTX | ISR_TXE);
		if(!tx_completed) resend=1;
	}

	out(m_port + TRANSMITCONFIGURATION, TCR_INTERNAL_LOOPBACK);
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);
	Receive();	/* cleanup RX ring */
	out(m_port + INTERRUPTSTATUS, ISR_OVW);	/* ACK INT */

	out(m_port + TRANSMITCONFIGURATION, TCR_DEFAULT);
	if(resend)
		out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START | NIC_TRANSMIT);
}

packet_data_t* alloc_buffer_data(size_t len)
{
	return (packet_data_t*) malloc(len);
}

void free_buffer(packet_buffer_t* buf)
{
	free(buf);
}

void free_buffer_data(packet_data_t* buf)
{
	free(buf);
}

/* This is the procedure that markst the transmit buffer as available again */
void CNe2000::Transmit()
{
	dword f;
	dword status;
/*	wprintf(L"nic_tx()\n"); */
	status=in(m_port + TRANSMITSTATUS);

	if(!m_tx_packet[m_send]) {
		wprintf(L"NE2000: ERROR: Invalid transmison packet buffer.\n");
		return;
	}
	if(!m_sending) wprintf(L"NE2000: ERROR: Invalid m_sending value.\n");

	free_buffer(m_tx_packet[m_send]);
	m_tx_packet[m_send]= NULL;
/*	m_tx_packet[m_send].count=0;	 mark buffer as available */
/*	m_tx_packet[m_send].len=0;
	m_tx_packet[m_send].page=0; */

	for(f=0;f<MAX_TX;f++) {
		if(m_tx_packet[f]) {
			wprintf(L"NE2000: DEBUG: transmitting secondary buffer:%X\n",f);
			m_stat.tx_buffered++;
			m_send=f;	m_sending=1;
			Send(f);	/* send a back-to-back buffer */
			break;
		}
	}
	if(f==MAX_TX) m_sending=0;

	if(status & TSR_COL) m_stat.errors.tx_collisions++;
	if(status & TSR_PTX) m_stat.tx_packets++;
	else {
		if(status & TSR_ABT) {
			m_stat.errors.tx_aborts++;
			m_stat.errors.tx_collisions+=16; }
		if(status & TSR_CRS) m_stat.errors.tx_carrier++;
		if(status & TSR_FU) m_stat.errors.tx_fifo++;
		if(status & TSR_CDH) m_stat.errors.tx_heartbeat++;
		if(status & TSR_OWC) m_stat.errors.tx_window++;
	}

	out(m_port + INTERRUPTSTATUS, ISR_PTX);	/* ack int */
}

void CNe2000::TransmitError()
{
	unsigned char tsr;
	tsr=in(m_port);
	wprintf(L"NE2000: ERROR: TX error: ");
	if(tsr & TSR_ABT) wprintf(L"Too many collisions.\n");
	if(tsr & TSR_ND) wprintf(L"Not deffered.\n");
	if(tsr & TSR_CRS) wprintf(L"Carrier lost.\n");
	if(tsr & TSR_FU) wprintf(L"FIFO underrun.\n");
	if(tsr & TSR_CDH) wprintf(L"Heart attack!\n");

	out(m_port + INTERRUPTSTATUS, ISR_TXE);
	if(tsr & (TSR_ABT | TSR_FU)) {
		wprintf(L"NE2000: DEBUG: Attempting to retransmit packet.\n");
		Transmit();
	}
}

void CNe2000::GetHeader(dword page, buffer_header_t *header)
{
	dword f;
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);
	out(m_port + REMOTEBYTECOUNT0, sizeof(buffer_header_t));
	out(m_port + REMOTEBYTECOUNT1, 0);		/* read the header */
	out(m_port + REMOTESTARTADDRESS0, 0); 	/* page boundary */
	out(m_port + REMOTESTARTADDRESS1, page); 	/* from this page */
	out(m_port, NIC_REM_READ | NIC_START);	/* start reading */

	if(m_wordlength==2) for(f=0;f<(sizeof(buffer_header_t)>>1);f++)
		((unsigned short *)header)[f]=in16(m_port+NE_DATA);
	else for(f=0;f<sizeof(buffer_header_t);f++)
		((unsigned char *)header)[f]=in(m_port+NE_DATA);
					/* Do these need to be *_p variants??? */
	out(m_port + INTERRUPTSTATUS, ISR_RDC);	/* finish DMA cycle */
}

void CNe2000::BlockInput(byte *buf, dword len, dword offset)
{
	dword f;
	dword xfers=len;
	dword timeout=TIMEOUT_DMAMATCH;	dword addr;
/*	kprintf("NE2000: RX: Length:%x  Offset:%x  ",len,offset); */
	out(m_port, NIC_DMA_DISABLE | NIC_PAGE0 | NIC_START);
	out(m_port + REMOTEBYTECOUNT0, len & 0xff);
	out(m_port + REMOTEBYTECOUNT1, len >> 8);
	out(m_port + REMOTESTARTADDRESS0, offset & 0xff);
	out(m_port + REMOTESTARTADDRESS1, offset >> 8);
	out(m_port, NIC_REM_READ | NIC_START);

            /* allow for no buffer */
        if(buf){
            if(m_wordlength==2) {
		for(f=0;f<(len>>1);f++)
                    ((unsigned short *)buf)[f]=in16(m_port+NE_DATA);
		if(len&0x01) {
                    ((unsigned char *)buf)[len-1]=in(m_port+NE_DATA);
                    xfers++;
		}
            } else {
                for(f=0;f<len;f++)
                    ((unsigned char *)buf)[f]=in(m_port+NE_DATA);
            }
        } else {
            if(m_wordlength==2) {
		for(f=0;f<(len>>1);f++)
                    in16(m_port+NE_DATA);
		if(len&0x01) {
                    in(m_port+NE_DATA);
                    xfers++;
		}
            } else {
                for(f=0;f<len;f++)
                    in(m_port+NE_DATA);
            }
        }
					/* Do these need to be *_p variants??? */

/*	for(f=0;f<15;f++) kprintf("%X",buf[f]); kprintf("\n"); */
	/* TODO: make this timeout a constant */
	for(f=0;f<timeout;f++) {
		dword high=in(m_port + REMOTESTARTADDRESS1);
		dword low=in(m_port + REMOTESTARTADDRESS0);
		addr=(high<<8)+low;
		if(((offset+xfers)&0xff)==low)	break;
	}
            /*
	if(f>=timeout)
		kprintf("NE2000: Remote DMA address invalid.  expected:%x - actual:%x\n",
			offset+xfers, addr); HUH WHAT? BJS*/
	
	out(m_port + INTERRUPTSTATUS, ISR_RDC);	/* finish DMA cycle */
}

void CNe2000::Receive()
{
	dword packets=0;	dword frame;	dword rx_page;	dword rx_offset;
	dword len;	dword next_pkt;	dword numpages;
	buffer_header_t header;
	while(packets<MAX_RX) {
		out(m_port, NIC_DMA_DISABLE | NIC_PAGE1); /*curr is on page 1 */
		rx_page=in(m_port + CURRENT);	/* get current page */
		out(m_port, NIC_DMA_DISABLE | NIC_PAGE0);
		frame=in(m_port + BOUNDARY)+1;
			/* we add one becuase boundary is a page behind
			// as pre NS notes to help in overflow problems */
		if(frame>=m_pstop) frame=m_pstart+TXPAGES;
							/* circual buffer */

		if(frame != m_current_page) {
			wprintf(L"NE2000: ERROR: mismatched read page pointers!\n");
			wprintf(L"NE2000: NIC-Boundary:%x  dev-current_page:%x\n",
				frame, m_current_page); }

/*		wprintf(L"Boundary-%x  Current-%x\n",frame-1,rx_page); */
		if(frame==rx_page) break;	/* all frames read */

		rx_offset=frame << 8;  /* current ptr in bytes(not pages) */

		GetHeader(frame,&header);
		len=header.count - sizeof(buffer_header_t);
							/* length of packet */
		next_pkt=frame + 1 + ((len+4)>>8); /* next packet frame */

		numpages=m_pstop-(m_pstart+TXPAGES);
		if(	   (header.next!=next_pkt)
			&& (header.next!=next_pkt + 1)
			&& (header.next!=next_pkt - numpages)
			&& (header.next != next_pkt +1 - numpages)){
				wprintf(L"NE2000: ERROR: Index mismatch.   header.next:%X  next_pkt:%X frame:%X\n",
					header.next,next_pkt,frame);
				m_current_page=frame;
				out(m_port + BOUNDARY, m_current_page-1);
				m_stat.errors.rx++;
				continue;
		}

		if(len<60 || len>1518) {
			wprintf(L"NE2000: invalid packet size:%d\n",len);
			m_stat.errors.rx_size++;
		} else if((header.status & 0x0f) == RSR_PRX) {
			/* We have a good packet, so let's recieve it! */

			packet_data_t *newpacket=alloc_buffer_data(len);
			if(!newpacket) {
				wprintf(L"NE2000: ERROR: out of memory!\n");
				m_stat.errors.rx_dropped++;
                                BlockInput(NULL,len,rx_offset+
                                                sizeof(buffer_header_t));
			} else {
                            BlockInput(newpacket->ptr,newpacket->len,
                                            rx_offset+sizeof(buffer_header_t));
								/* read it */
                            
                            //if(m_notify) m_notify(m_kore,newpacket);
                            //else
								free_buffer_data(newpacket);
                                /* NOTE: you are responsible for deleting this buffer. */

                            m_stat.rx_packets++;
                        }
		} else {
			wprintf(L"NE2000: ERROR: bad packet.  header-> status:%X next:%X len:%x.\n",
				header.status,header.next,header.count);
			if(header.status & RSR_FO) m_stat.errors.rx_fifo++;
		}
/*		wprintf(L"frame:%x  header.next:%x  next_pkt:%x\n",
			frame,header.next,next_pkt); */
		next_pkt=header.next;

		if(next_pkt >= m_pstop) {
			wprintf(L"NE2000: ERROR: next frame beyond local buffer!  next:%x.\n",
				next_pkt);
			next_pkt=m_pstart+TXPAGES;
		}

		m_current_page=next_pkt;
		out(m_port + BOUNDARY, next_pkt-1);
	}
	out(m_port + INTERRUPTSTATUS, ISR_PRX | ISR_RXE);	/* ack int */
}

void CNe2000::Counters()
{
	m_stat.errors.frame_alignment+=in(m_port+FAE_TALLY);
	m_stat.errors.crc+=in(m_port+CRC_TALLY);
	m_stat.errors.missed_packets+=in(m_port+MISS_PKT_TALLY);
	/* reading the counters on the DC8390 clears them. */
	out(m_port + INTERRUPTSTATUS, ISR_CNT); /* ackkowledge int */
}

int CNe2000::Send(dword buf)
{
	return 0;
}

HRESULT CNe2000::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IDevice))
	{
		*ppvObject = (IDevice*) this;
		AddRef();
		return S_OK;
	}
	
	return E_FAIL;
}

HRESULT CNe2000::GetInfo(device_t* buf)
{
	if (buf->size < sizeof(device_t))
		return E_FAIL;

	wcsncpy(buf->name, L"NE2000-compatible network adapter", buf->name_max);
	return S_OK;
}

HRESULT CNe2000::DeviceOpen()
{
	return S_OK;
}