/*
 * fdc.c
 * 
 * floppy controller handler functions
 * 
 * Copyright (C) 1998  Fabian Nunez
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * The author can be reached by email at: fabian@cs.uct.ac.za
 * 
 * or by airmail at: Fabian Nunez
 *					 10 Eastbrooke
 *					 Highstead Road
 *					 Rondebosch 7700
 *					 South Africa
 */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/sys.h>
#include <kernel/cache.h>
#include <stdio.h>
#include <os/blkdev.h>
#include <errno.h>
#include "util.h"
#include "fdc.h"

#include <kernel/debug.h>

//! \ingroup	fdc
//!@{

/* globals */
struct Fdc
{
	device_t dev;

	volatile bool done;
	bool dchange;
	bool motor;
	dword motor_end;
	byte status[7];
	byte statsz;
	byte sr0;
	byte fdc_track;
	DrvGeom geometry;

	long tbaddr;	/* physical address of track buffer located below 1M */
};

/* prototypes */
void sendbyte(byte b);
unsigned getbyte();
void int1c(Fdc* fdc);
bool fdcWait(Fdc* fdc, bool sensei);
bool fdc_rw(Fdc* fdc, int block,byte *blockbuff,bool read);

/* helper functions */

/* deinit driver */
void fdcCleanup(Fdc* fdc)
{
	//set_irq_handler(6,NULL,&oldirq6);
	//wprintf(L"uninstalling IRQ6 handler\n");
	//set_irq_handler(0x1c,NULL,&oldint1c);
	//wprintf(L"uninstalling timer handler\n");

	devRegisterIrq(&fdc->dev, 6, false);
	devRegisterIrq(&fdc->dev, 0, false);

	/* stop motor forcefully */
	out(FDC_DOR, FDC_DOR_ENABLE | FDC_DOR_IRQIO);
}

/* sendbyte() routine from intel manual */
void sendbyte(byte b)
{
	volatile int msr;
	int tmo;

	for (tmo = 0;tmo < 128;tmo++) {
		msr = in(FDC_MSR);
		if ((msr & 0xc0) == 0x80) {
			out(FDC_DATA, b);
			return;
		}
		in(0x80);   /* delay */
	}
}

/* getbyte() routine from intel manual */
unsigned getbyte()
{
	volatile int msr;
	int tmo;

	for (tmo = 0;tmo < 128;tmo++) {
		msr = in(FDC_MSR);
		if ((msr & 0xd0) == 0xd0) {
			return in(FDC_DATA);
		}
		in(0x80);   /* delay */
	}

	return (unsigned) -1;	/* read timeout */
}

/* this waits for FDC command to complete */
bool fdcWait(Fdc* fdc, bool sensei)
{
	dword timeout_end = sysUpTime() + 1000;   /* set timeout to 1 second */
	bool failed;

	failed = false;
	/* wait for IRQ6 handler to signal command finished */
	while (!fdc->done)
		if (sysUpTime() > timeout_end)
		{
			failed = true;
			break;
		}

	/* read in command result bytes */
	fdc->statsz = 0;
	while ((fdc->statsz < 7) && (in(FDC_MSR) & (1<<4))) {
		fdc->status[fdc->statsz++] = getbyte();
	}

	if (sensei) {
		/* send a "sense interrupt status" command */
		sendbyte(CMD_SENSEI);
		fdc->sr0 = getbyte();
		fdc->fdc_track = getbyte();
	}

	fdc->done = false;

	if (failed) {
		/* timed out! */
		if (in(FDC_DIR) & 0x80)  /* check for diskchange */
			fdc->dchange = true;

		return false;
	} else
		return true;
}

/*
 * converts linear block address to head/track/sector
 * 
 * blocks are numbered 0..heads*tracks*spt-1
 * blocks 0..spt-1 are serviced by head #0
 * blocks spt..spt*2-1 are serviced by head 1
 * 
 * WARNING: garbage in == garbage out
 */
void block2hts(Fdc* fdc, int block,int *head,int *track,int *sector)
{
	*head = (block % (fdc->geometry.spt * fdc->geometry.heads)) / (fdc->geometry.spt);
	*track = block / (fdc->geometry.spt * fdc->geometry.heads);
	*sector = block % fdc->geometry.spt + 1;
}

/**** disk operations ****/

/* this gets the FDC to a known state */
void fdcReset(Fdc* fdc)
{
	/* stop the motor and disable IRQ/DMA */
	out(FDC_DOR, 0);

	fdc->motor_end = -1;
	fdc->motor = false;

	/* program data rate (500K/s) */
	//out(FDC_DRS,0);

	/* re-enable interrupts */
	out(FDC_DOR, FDC_DOR_ENABLE | FDC_DOR_IRQIO);

	/* resetting triggered an interrupt - handle it */
	fdc->done = true;
	fdcWait(fdc, true);

	/* specify drive timings (got these off the BIOS) */
	sendbyte(CMD_SPECIFY);
	sendbyte(0xdf);	/* SRT = 3ms, HUT = 240ms */
	sendbyte(0x02);	/* HLT = 16ms, ND = 0 */

	/* clear "disk change" status */
	fdcSeek(fdc, 1);
	fdcRecalibrate(fdc);

	fdc->dchange = false;
}

/* this returns whether there was a disk change */
bool fdcDiskChange(Fdc* fdc)
{
	return fdc->dchange;
}

/* this turns the motor on */
void fdcMotorOn(Fdc* fdc)
{
	if (!fdc->motor)
	{
		out(FDC_DOR, FDC_DOR_MOTOR0 | FDC_DOR_ENABLE | FDC_DOR_IRQIO);
		msleep(500); /* delay 500ms for motor to spin up */
		fdc->motor = true;
		//TRACE0("floppy: motor on\n");
	}

	fdc->motor_end = -1;	   /* stop motor kill countdown */
}

/* this turns the motor off */
void fdcMotorOff(Fdc* fdc)
{
	if (fdc->motor) {
		fdc->motor_end = sysUpTime() + 2000;	 /* start motor kill countdown: 36 ticks ~ 2s */
	}
}

/* recalibrate the drive */
void fdcRecalibrate(Fdc* fdc)
{
	/* turn the motor on */
	fdcMotorOn(fdc);

	/* send actual command bytes */
	sendbyte(CMD_RECAL);
	sendbyte(0);

	/* wait until seek finished */
	fdcWait(fdc, true);

	/* turn the motor off */
	fdcMotorOff(fdc);
}

/* seek to track */
bool fdcSeek(Fdc* fdc, int track)
{
	if (fdc->fdc_track == track)  /* already there? */
		return true;

	fdcMotorOn(fdc);

	/* send actual command bytes */
	sendbyte(CMD_SEEK);
	sendbyte(0);
	sendbyte(track);

	/* wait until seek finished */
	if (!fdcWait(fdc, true))
		return false;	   /* timeout! */

	/* now let head settle for 15ms */
	msleep(15);

	fdcMotorOff(fdc);

	/* check that seek worked */
	if ((fdc->sr0 != 0x20) || (fdc->fdc_track != track))
		return false;
	else
		return true;
}

/* checks drive geometry - call this after any disk change */
bool fdcLogDisk(Fdc* fdc, DrvGeom *g)
{
	/* get drive in a known status before we do anything */
	fdcReset(fdc);

	/* assume disk is 1.68M and try and read block #21 on first track */
	fdc->geometry.heads = DG168_HEADS;
	fdc->geometry.tracks = DG168_TRACKS;
	fdc->geometry.spt = DG168_SPT;

	if (fdcReadBlock(fdc, 20,NULL)) {
		/* disk is a 1.68M disk */
		if (g) {
			g->heads = fdc->geometry.heads;
			g->tracks = fdc->geometry.tracks;
			g->spt = fdc->geometry.spt;
		}
		return true;
	}

	/* OK, not 1.68M - try again for 1.44M reading block #18 on first track */
	fdc->geometry.heads = DG144_HEADS;
	fdc->geometry.tracks = DG144_TRACKS;
	fdc->geometry.spt = DG144_SPT;

	if (fdcReadBlock(fdc, 17,NULL)) {
		/* disk is a 1.44M disk */
		if (g) {
			g->heads = fdc->geometry.heads;
			g->tracks = fdc->geometry.tracks;
			g->spt = fdc->geometry.spt;
		}
		return true;
	}

	/* it's not 1.44M or 1.68M - we don't support it */
	return false;
}

/* read block (blockbuff is 512 byte buffer) */
bool fdcReadBlock(Fdc* fdc, int block,byte *blockbuff)
{
	return fdc_rw(fdc, block,blockbuff,true);
}

/* write block (blockbuff is 512 byte buffer) */
bool fdcWriteBlock(Fdc* fdc, int block,byte *blockbuff)
{
	return fdc_rw(fdc, block,blockbuff,false);
}

/*
 * since reads and writes differ only by a few lines, this handles both.  This
 * function is called by read_block() and write_block()
 */
bool fdc_rw(Fdc* fdc, int block,byte *blockbuff,bool read)
{
	int head,track,sector,tries,i;

	/* convert logical address into physical address */
	block2hts(fdc, block,&head,&track,&sector);
	TRACE4("block %d = %d:%02d:%02d\n",block,head,track,sector);

	/* spin up the disk */
	fdcMotorOn(fdc);

	if (!read && blockbuff)
	{
		/* copy data from data buffer into track buffer */
		//movedata(_my_ds(),(long)blockbuff,_dos_ds,tbaddr,512);
		memcpy((void*) fdc->tbaddr, blockbuff, 512);
	}

	for (tries = 0;tries < 3;tries++)
	{
		TRACE1("attempt %d\t", tries);
		/* check for diskchange */
		if (in(FDC_DIR) & 0x80)
		{
			fdc->dchange = true;
			fdcSeek(fdc, 1);  /* clear "disk change" status */
			fdcRecalibrate(fdc);
			fdcMotorOff(fdc);
			return false;
		}
		 
		/* move head to right track */
		TRACE0("seek\t");
		if (!fdcSeek(fdc, track))
		{
			fdcMotorOff(fdc);
			return false;
		}

		/* program data rate (500K/s) */
		out(FDC_CCR, FDC_CCR_500K);

		/* send command */
		if (read)
		{
			TRACE0("dma\t");
			dma_xfer(2,fdc->tbaddr,512,false);
			TRACE0("read\t");
			sendbyte(CMD_READ);
		} else
		{
			TRACE0("dma\t");
			dma_xfer(2,fdc->tbaddr,512,true);
			TRACE0("write\t");
			sendbyte(CMD_WRITE);
		}

		sendbyte(head << 2);
		sendbyte(track);
		sendbyte(head);
		sendbyte(sector);
		sendbyte(2);				 /* 512 bytes/sector */
		sendbyte(fdc->geometry.spt);
		if (fdc->geometry.spt == DG144_SPT)
			sendbyte(DG144_GAP3RW);  /* gap 3 size for 1.44M read/write */
		else
			sendbyte(DG168_GAP3RW);  /* gap 3 size for 1.68M read/write */
		sendbyte(0xff);			 /* DTL = unused */

		/* wait for command completion */
		/* read/write don't need "sense interrupt status" */
		TRACE0("wait\t");
		if (!fdcWait(fdc, false))
		{
			TRACE0("failed\n");
			return false;	/* timed out! */
		}

		TRACE0("finished\n");
		if ((fdc->status[0] & 0xc0) == 0) break;	 /* worked! outta here! */

		fdcRecalibrate(fdc);	/* oops, try again... */
	}

	/* stop the motor */
	fdcMotorOff(fdc);

	if (read && blockbuff) {
		/* copy data from track buffer into data buffer */
		//movedata(_dos_ds,tbaddr,_my_ds(),(long)blockbuff,512);
		memcpy(blockbuff, (void*) fdc->tbaddr, 512);
	}

	TRACE0("status bytes: ");
	for (i = 0;i < fdc->statsz;i++)
		TRACE1("%02x ",fdc->status[i]);

	TRACE0("\n");

	return (tries != 3);
}

/* this formats a track, given a certain geometry */
bool fdcFormatTrack(Fdc* fdc, byte track,DrvGeom *g)
{
	int i,h,r,r_id,split;
	byte tmpbuff[256];

	/* check geometry */
	if (g->spt != DG144_SPT && g->spt != DG168_SPT)
		return false;

	/* spin up the disk */
	fdcMotorOn(fdc);

	/* program data rate (500K/s) */
	out(FDC_CCR,0);

	fdcSeek(fdc, track);  /* seek to track */

	/* precalc some constants for interleave calculation */
	split = g->spt / 2;
	if (g->spt & 1) split++;

	for (h = 0;h < g->heads;h++) {
		/* for each head... */
	
		/* check for diskchange */
		if (in(FDC_DIR) & 0x80) {
			fdc->dchange = true;
			fdcSeek(fdc, 1);  /* clear "disk change" status */
			fdcRecalibrate(fdc);
			fdcMotorOff(fdc);
			return false;
		}

		i = 0;   /* reset buffer index */
		for (r = 0;r < g->spt;r++) {
			/* for each sector... */

			/* calculate 1:2 interleave (seems optimal in my system) */
			r_id = r / 2 + 1;
			if (r & 1) r_id += split;

			/* add some head skew (2 sectors should be enough) */
			if (h & 1) {
				r_id -= 2;
				if (r_id < 1) r_id += g->spt;
			}

			/* add some track skew (1/2 a revolution) */
			if (track & 1) {
				r_id -= g->spt / 2;
				if (r_id < 1) r_id += g->spt;
			}

			/**** interleave now calculated - sector ID is stored in r_id ****/

			/* fill in sector ID's */
			tmpbuff[i++] = track;
			tmpbuff[i++] = h;
			tmpbuff[i++] = r_id;
			tmpbuff[i++] = 2;
		}

		/* copy sector ID's to track buffer */
		//movedata(_my_ds(),(long)tmpbuff,_dos_ds,tbaddr,i);
		memcpy((void*) fdc->tbaddr, tmpbuff, i);

		/* start dma xfer */
		dma_xfer(2,fdc->tbaddr,i,true);

		/* prepare "format track" command */
		sendbyte(CMD_FORMAT);
		sendbyte(h << 2);
		sendbyte(2);
		sendbyte(g->spt);
		if (g->spt == DG144_SPT)		
			sendbyte(DG144_GAP3FMT);	/* gap3 size for 1.44M format */
		else
			sendbyte(DG168_GAP3FMT);	/* gap3 size for 1.68M format */
		sendbyte(0);	   /* filler byte */

		/* wait for command to finish */
		if (!fdcWait(fdc, false))
			return false;

		if (fdc->status[0] & 0xc0) {
			fdcMotorOff(fdc);
			return false;
		}
	}

	fdcMotorOff(fdc);

	return true;
}

bool fdcRequest(device_t* dev, request_t* req)
{
	Fdc* fdc = (Fdc*) dev;
	int tries;
	DrvGeom geom;
	dword pos;
	block_size_t* size;

	//if (req->code != DEV_ISR)
		//TRACE2("#%c%c", req->code / 256, req->code % 256);

	switch (req->code)
	{
	case DEV_REMOVE:
		fdcCleanup(fdc);
		hndFree(fdc);
	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_ISR:
		switch (req->params.isr.irq)
		{
		case 0:
			if (fdc->motor_end != -1 &&
				sysUpTime() >= fdc->motor_end &&
				fdc->motor)
			{
				//TRACE0("floppy: turning off motor\n");
				out(FDC_DOR, FDC_DOR_ENABLE | FDC_DOR_IRQIO);  /* turn off floppy motor */
				fdc->motor = false;
				fdc->motor_end = -1;
			}

			return false;
		case 6:
			fdc->done = true;
			return true;
		}

		return false;

	case DEV_READ:
	case DEV_WRITE:
		if (req->params.buffered.length < 512)
		{
			req->result = EINVALID;
			return false;
		}

		req->user_length = req->params.buffered.length;
		req->params.buffered.length = 0;
		pos = req->params.buffered.pos / 512;
		while (req->params.buffered.length < req->user_length)
		{
			for (tries = 0; tries < 2; tries++)
			{
				if (fdc_rw(fdc, 
					pos, 
					(byte*) req->params.buffered.buffer + 
						req->params.buffered.length,
					req->code == DEV_READ))
				{
					pos++;
					req->params.buffered.length += 512;
					break;
				}
				else if (!fdcLogDisk(fdc, &geom))
				{
					req->result = EINVALID;
					return false;
				}
				else
					TRACE3("floppy: new disk geometry: %%d:%02d:%02d\n",
						geom.heads, geom.tracks, geom.spt);
			}
		}

		hndSignal(req->event, true);
		return true;

	case BLK_GETSIZE:
		size = (block_size_t*) req->params.buffered.buffer;
		size->block_size = 512;
		size->total_blocks = 
			fdc->geometry.heads * fdc->geometry.tracks * fdc->geometry.spt;
		hndSignal(req->event, true);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t* fdcAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	Fdc *fdc;
	int i;

	fdc = hndAlloc(sizeof(Fdc), NULL);
	memset(fdc, 0, sizeof(Fdc));
	fdc->dev.request = fdcRequest;
	fdc->fdc_track = 0xff;
	fdc->geometry.heads = DG144_HEADS;
	fdc->geometry.tracks = DG144_TRACKS;
	fdc->geometry.spt = DG144_SPT;

	devRegisterIrq(&fdc->dev, 6, true);
	devRegisterIrq(&fdc->dev, 0, true);

	/* allocate track buffer (must be located below 1M) */
	fdc->tbaddr = alloc_dma_buffer();

	fdcReset(fdc);

	/* get floppy controller version */
	sendbyte(CMD_VERSION);
	i = getbyte();

	if (i == 0x80)
		wprintf(L"NEC765 controller found\n");
	else
		wprintf(L"enhanced controller found\n");

	return ccInstallBlockCache(&fdc->dev, 512);
	//return &fdc->dev;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = fdcAddDevice;
	return true;
}

//@}