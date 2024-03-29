/* $Id: fdc.c,v 1.1.1.1 2002/12/21 09:48:48 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/memory.h>
#include <kernel/cache.h>

/*#define DEBUG*/
#include <kernel/debug.h>
#include <kernel/syscall.h>
#include <os/blkdev.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

addr_t	alloc_dma_buffer(void);
void	dma_xfer(unsigned channel, addr_t physaddr, int length, bool read);
void *	sbrk_virtual(size_t diff);

/* Floppy drive controller register offsets */
#define REG_STATUS_A	0	/* status A: read */
#define REG_STATUS_B	1	/* status B: read */
#define REG_DOR			2	/* digital output register: write */
#define REG_MSR			4	/* main status register: read */
#define REG_DSR			4	/* data rate select register: write */
#define REG_DATA		5	/* data register: read/write */
#define REG_DIR			7	/* digital input register: read */
#define REG_CCR			7	/* configuration control register: write */

/* Digital output register flags */
#define DOR_DRIVE_SHIFT	0		/* bottom two bits select drive */
#define DOR_ENABLE		0x04	/* enable controller (= !reset) */
#define DOR_IRQDMA		0x08	/* IRQ & DMA enabled */
#define DOR_MOTORA		0x10	/* motor for drive A */
#define DOR_MOTORB		0x20	/* motor for drive B */
#define DOR_MOTORC		0x40	/* motor for drive C */
#define DOR_MOTORD		0x80	/* motor for drive D */

/* Main status register flags */
#define MSR_ACTA		0x01	/* drive A in positioning mode */
#define MSR_ACTB		0x02	/* drive B in positioning mode */
#define MSR_ACTC		0x04	/* drive C in positioning mode */
#define MSR_ACTD		0x08	/* drive D in positioning mode */
#define MSR_BUSY		0x10	/* device busy */
#define MSR_NDMA		0x20	/* not in DMA mode */
#define MSR_DIO			0x40	/* data input/output is controller->CPU */
#define MSR_MRQ			0x80	/* main request: data register is ready */

/*
 * Default settings for DOR:
 *	Controller & IRQ/DMA enabled
 *	Drive A selected
 *	All motors off
 */
#define DOR_DEFAULT		(DOR_ENABLE | DOR_IRQDMA | (0 << DOR_DRIVE_SHIFT))

typedef struct fdc_geometry_t fdc_geometry_t;
struct fdc_geometry_t
{
	uint8_t heads;
	uint8_t tracks;
	uint8_t spt;		/* sectors per track */
};

/* drive geometries */
#define DG144_HEADS       2     /* heads per drive (1.44M) */
#define DG144_TRACKS     80     /* number of tracks (1.44M) */
#define DG144_SPT        18     /* sectors per track (1.44M) */
#define DG144_GAP3FMT  0x54     /* gap3 while formatting (1.44M) */
#define DG144_GAP3RW   0x1b     /* gap3 while reading/writing (1.44M) */

#define DG168_HEADS       2     /* heads per drive (1.68M) */
#define DG168_TRACKS     80     /* number of tracks (1.68M) */
#define DG168_SPT        21     /* sectors per track (1.68M) */
#define DG168_GAP3FMT  0x0c     /* gap3 while formatting (1.68M) */
#define DG168_GAP3RW   0x1c     /* gap3 while reading/writing (1.68M) */

/* command bytes (these are 765 commands + options such as MFM, etc) */
#define CMD_SPECIFY (0x03)  /* specify drive timings */
#define CMD_WRITE   (0xc5)  /* write data (+ MT,MFM) */
#define CMD_READ    (0xe6)  /* read data (+ MT,MFM,SK) */
#define CMD_RECAL   (0x07)  /* recalibrate */
#define CMD_SENSEI  (0x08)  /* sense interrupt status */
#define CMD_FORMAT  (0x4d)  /* format track (+ MFM) */
#define CMD_SEEK    (0x0f)  /* seek track */
#define CMD_VERSION (0x10)  /* FDC version */

#define FDC_CCR_500K	0
#define FDC_CCR_250K	2

/* Delay while motor spins up, in ms */
#define TIMEOUT_MOTOR_SPINUP	500
/* Delay before motor is shut off, in ms */
#define TIMEOUT_MOTOR_OFF		5000

typedef struct fdc_t fdc_t;
struct fdc_t
{
	device_t *device;
	
	volatile enum
	{
		fdcIdle,
		fdcReset,
		fdcSpinUp,
		fdcSeek,
		fdcTransfer,
		fdcRecal
	} op;

	uint16_t base;
	uint8_t irq;
	
	fdc_geometry_t geometry;
	uint64_t total_bytes;
	
	int motor_end;
	bool motor_on;
	
	unsigned statsz;
	uint8_t status[7];
	uint8_t sr0;
	bool sensei;

	void *transfer_buffer;
	addr_t transfer_phys;
	unsigned fdc_track;
};

typedef struct fdc_ioextra_t fdc_ioextra_t;
struct fdc_ioextra_t
{
	unsigned block;
	unsigned retry;
};

/* sendbyte() routine from intel manual */
void FdcSendByte(fdc_t *fdc, uint8_t b)
{
	volatile int msr;
	int tmo;

	for (tmo = 0;tmo < 128;tmo++) {
		msr = in(fdc->base + REG_MSR);
		if ((msr & 0xc0) == 0x80) {
			out(fdc->base + REG_DATA, b);
			return;
		}
		in(0x80);   /* delay */
	}
}

/* getbyte() routine from intel manual */
unsigned FdcGetByte(fdc_t *fdc)
{
	volatile int msr;
	int tmo;

	for (tmo = 0;tmo < 128;tmo++) {
		msr = in(fdc->base + REG_MSR);
		if ((msr & 0xd0) == 0xd0) {
			return in(fdc->base + REG_DATA);
		}
		in(0x80);   /* delay */
	}

	return (unsigned) -1;	/* read timeout */
}

void FdcBlockToHts(fdc_t* fdc, unsigned block, unsigned *head, 
				   unsigned *track, unsigned *sector)
{
	*head = (block % (fdc->geometry.spt * fdc->geometry.heads)) / (fdc->geometry.spt);
	*track = block / (fdc->geometry.spt * fdc->geometry.heads);
	*sector = block % fdc->geometry.spt + 1;
}

void FdcMotorOn(fdc_t *fdc)
{
	fdc->motor_end = WrapSysUpTime() + TIMEOUT_MOTOR_SPINUP;
	out(fdc->base + REG_DOR, DOR_MOTORA | DOR_DEFAULT);
}

void FdcMotorOff(fdc_t *fdc)
{
	fdc->op = fdcIdle;
	fdc->motor_end = WrapSysUpTime() + TIMEOUT_MOTOR_OFF;
}

void msleep(unsigned ms)
{
	unsigned end;
	end = WrapSysUpTime() + ms;
	while (WrapSysUpTime() <= end)
		;
}

addr_t DbgGetPhysicalAddress(const void *ptr)
{
	return (MemTranslate(ptr) & -PAGE_SIZE) + (addr_t) ptr % PAGE_SIZE;
}

void FdcStartIo(fdc_t *fdc)
{
	asyncio_t *io;
	unsigned head, track, sector;
	fdc_ioextra_t *extra;
	request_dev_t *req_dev;
	uint8_t *buf;
	status_t ret;

	io = fdc->device->io_first;
	assert(io != NULL);

	extra = io->extra;
	req_dev = (request_dev_t*) io->req;

	/*
	 * Handle the floppy drive controller state machine.
	 * A state will leave this function if a state transition involves an IRQ.
	 * A state will return to 'start:' otherwise.
	 */
start:

	FdcBlockToHts(fdc, extra->block, &head, &track, &sector);
	/*wprintf(L"fdc: state = %u code = %x addr = %p => %lx\n", 
		fdc->op, 
		req_dev->header.code, 
		&req_dev->header.code, 
		DbgGetPhysicalAddress(&req_dev->header.code));*/
	switch (fdc->op)
	{
	case fdcReset:
		assert(fdc->op != fdcReset);
		break;

	case fdcIdle:
		/* Controller is idle, so we need to start a request */
		fdc->op = fdcSpinUp;
		if (fdc->motor_on)
			goto start;
		else
		{
			TRACE0("fdc: starting motor\n");
			FdcMotorOn(fdc);
		}
		break;

	case fdcSpinUp:
		/* Motor is turned on, so we can start seeking */

		/* We don't want to switch the motor off in the middle of a request */
		fdc->motor_end = -1;

		fdc->op = fdcSeek;
		if (fdc->fdc_track == track)
		{
			/* It's already there, so we can skip to the transfer */

			/* Simulate a successful seek */
			fdc->sr0 = 0x20;
			fdc->sensei = false;
			goto start;
		}
		else
		{
			TRACE1("fdc: seeking to track %u\n", track);
			fdc->sensei = true;
			FdcSendByte(fdc, CMD_SEEK);
			FdcSendByte(fdc, 0);
			FdcSendByte(fdc, track);
			/*fdc->fdc_track = track;*/
		}
		break;

	case fdcSeek:
		if (fdc->sensei)
			/* Let head settle just after seeking */
			/*msleep(15);*/
			;
		
		/* Motor has finished seeking, so we can start to transfer data */
		if (fdc->sr0 != 0x20 || fdc->fdc_track != track)
		{
			wprintf(L"fdc: seek to track %u failed (fdc_track = %u, sr0 = %x)\n", 
				track, fdc->fdc_track, fdc->sr0);
			ret = -1;
			goto finished;
		}

		fdc->op = fdcTransfer;
		fdc->sensei = false;

		/* program data rate (500K/s) */
		out(fdc->base + REG_CCR, FDC_CCR_500K);

		/* send command */
		if (req_dev->header.code == DEV_READ)
		{
			dma_xfer(2, fdc->transfer_phys, 512, false);
			FdcSendByte(fdc, CMD_READ);
		}
		else if (req_dev->header.code == DEV_WRITE)
		{
			/*buf = DevMapBuffer(io) + io->mod_buffer_start + io->length;*/
            buf = MemMapPageArray(io->pages) + io->length;
			TRACE3("fdc: write block %u: copying from user %p to buffer %p\n", 
				extra->block, buf, fdc->transfer_buffer);
			memcpy(fdc->transfer_buffer, buf, 512);
			MemUnmapPageArray(io->pages);

			dma_xfer(2, fdc->transfer_phys, 512, true);
			FdcSendByte(fdc, CMD_WRITE);
		}
		else
		{
			wprintf(L"floppy: got weird code (%x)\n", req_dev->header.code);
			/*ret = EHARDWARE;
			goto finished;*/
		}
		
		FdcSendByte(fdc, head << 2);
		FdcSendByte(fdc, track);
		FdcSendByte(fdc, head);
		FdcSendByte(fdc, sector);
		FdcSendByte(fdc, 2);				/* 512 bytes/sector */
		FdcSendByte(fdc, fdc->geometry.spt);
		if (fdc->geometry.spt == DG144_SPT)
			FdcSendByte(fdc, DG144_GAP3RW);	/* gap 3 size for 1.44M read/write */
		else
			FdcSendByte(fdc, DG168_GAP3RW);	/* gap 3 size for 1.68M read/write */
		FdcSendByte(fdc, 0xff);				/* DTL = unused */
		break;

	case fdcTransfer:
		if (fdc->status[0] & 0xc0)
		{
			/* Something went wrong: retry */
			extra->retry++;
			if (extra->retry >= 3)
			{
				wprintf(L"fdc: retry = %u, giving up\n", extra->retry);
				ret = E_MEDIUM_REMOVED;
				goto finished;
			}
			else
			{
				wprintf(L"fdc: retry = %u, continuing\n", extra->retry);
				fdc->op = fdcRecal;
				fdc->sensei = true;
				FdcSendByte(fdc, CMD_RECAL);
				FdcSendByte(fdc, 0);

				/* Will return to fdcSpinUp on receipt of an IRQ */
			}
		}
		else
		{
			/* Transfer of one sector went OK */

			extra->retry = 0;
			if (req_dev->header.code == DEV_READ)
			{
				/*buf = DevMapBuffer(io) + io->mod_buffer_start + io->length;*/
				buf = MemMapPageArray(io->pages) + io->length;
				/*wprintf(L"fdc: read block %u: %p => %p: length = %u/%u: ", 
					extra->block, fdc->transfer_buffer, buf, 
					io->length, req_dev->params.buffered.length);*/
				memcpy(buf, fdc->transfer_buffer, 512);
				MemUnmapPageArray(io->pages);
			}
			
			io->length += 512;

			if (io->length < req_dev->params.buffered.length)
			{
				TRACE0("more\n");
				extra->block++;
				fdc->op = fdcSpinUp;
				goto start;
			}
			else
			{
				TRACE0("finished\n");
				ret = 0;
				goto finished;
			}
		}

		break;

	case fdcRecal:
		/* Recalibration has finished: go back and seek again */
		fdc->op = fdcSpinUp;
		goto start;
	}

	return;

	/*
	 * States exit via 'finished:' if the operation is completed
	 *	The controller is returned to the 'idle' state.
	 *	The motor is scheduled to be turned off.
	 *	The caller is given a return code (from 'ret')
	 */
finished:
	/* Drive has finished reading/writing, so we can turn off the motor */
	TRACE1("fdc: finishing with code %u\n", ret);
	FdcMotorOff(fdc);
	free(io->extra);
	req_dev->params.buffered.length = io->length;
	DevFinishIo(fdc->device, io, ret);
}

bool FdcIsr(device_t *device, uint8_t irq)
{
	fdc_t *fdc;

	fdc = device->cookie;
	assert(irq == 0 || irq == fdc->irq);
	if (irq == 0)
	{
		if (fdc->motor_end != -1 &&
			WrapSysUpTime() >= fdc->motor_end)
		{
			switch (fdc->op)
			{
			case fdcIdle:
				/* Drive is idle, and the motor needs to be turned off */
				fdc->motor_end = -1;
				fdc->motor_on = false;
				out(fdc->base + REG_DOR, DOR_DEFAULT);
				wprintf(L"m");
				break;
			case fdcReset:
				/* Drive motor has finished spinning up during reset */
				wprintf(L"M");
				fdc->motor_end = -1;
				fdc->motor_on = true;
				fdc->op = fdcIdle;
				break;
			case fdcSpinUp:
				/* Drive motor has finished spinning up during a request */
				wprintf(L"M");
				fdc->motor_end = -1;
				fdc->motor_on = true;
				FdcStartIo(fdc);
				break;
			default:
				wprintf(L"fdcIsr: state = %u when it shouldn't be\n",
					fdc->op);
				assert(false);
				break;
			}
		}

		return false;
	}

	/* read in command result bytes */
	fdc->statsz = 0;
	while (fdc->statsz < 7 && 
		(in(fdc->base + REG_MSR) & (1<<4)))
		fdc->status[fdc->statsz++] = FdcGetByte(fdc);
	
	if (fdc->sensei)
	{
		FdcSendByte(fdc, CMD_SENSEI);
		fdc->sr0 = FdcGetByte(fdc);
		fdc->fdc_track = FdcGetByte(fdc);
	}

	switch (fdc->op)
	{
	case fdcIdle:
		wprintf(L"fdc: spurious IRQ\n");
		/* fall through */

	case fdcReset:
        //wprintf(L"fdc: IRQ during reset\n");
		fdc->op = fdcIdle;
		return true;

	case fdcSpinUp:
		/* We shouldn't get an IRQ while spinning up */
		assert(fdc->op != fdcSpinUp);
		
	case fdcSeek:
	case fdcTransfer:
	case fdcRecal:
		FdcStartIo(fdc);
		return true;
	}

	return false;
}

static void FdcDeleteDevice(device_t *device)
{
	fdc_t *fdc;
	fdc = device->cookie;
	MemFree(fdc->transfer_phys);
	/* xxx - need to free fdc->transfer_buffer */
	free(fdc);
	/* DevUnregisterIrq(0, &fdc->dev); */
	/* DevUnregisterIrq(6, &fdc->dev); */
}


static status_t FdcRequest(device_t *device, request_t *req)
{
	request_dev_t *req_dev;
	asyncio_t *io;
	fdc_ioextra_t *extra;
	fdc_t *fdc;
	block_size_t *size;

	fdc = device->cookie;
	req_dev = (request_dev_t*) req;
	switch (req->code)
	{
	case DEV_READ:
	case DEV_WRITE:
		if ((req_dev->params.buffered.length & 511) ||
			(req_dev->params.buffered.offset & 511))
			return EINVALID;

		if (req_dev->params.buffered.offset >= fdc->total_bytes)
			return EEOF;

		if (req_dev->params.buffered.offset + req_dev->params.buffered.length 
			>= fdc->total_bytes)
			req_dev->params.buffered.length = 
				fdc->total_bytes - req_dev->params.buffered.offset;
		
		io = DevQueueRequest(device, req, sizeof(request_dev_t),
			req_dev->params.buffered.pages,
			req_dev->params.buffered.length);
		if (io == NULL)
			return errno;
		
		io->length = 0;
		io->extra = extra = malloc(sizeof(fdc_ioextra_t));
		assert(io->extra != NULL);
		extra->block = req_dev->params.buffered.offset / 512;
		extra->retry = 0;

		if (fdc->op == fdcIdle)
			FdcStartIo(fdc);
		return SIOPENDING;

	case BLK_GETSIZE:
	    //size = req_dev->params.buffered.buffer;
        size = MemMapPageArray(req_dev->params.buffered.pages);
	    size->block_size = 512;
	    size->total_blocks = 
		    fdc->geometry.heads * fdc->geometry.tracks * fdc->geometry.spt;
        MemUnmapPageArray(req_dev->params.buffered.pages);
	    return 0;
	}

	return ENOTIMPL;
}


static const device_vtbl_t fdc_vtbl =
{
	FdcDeleteDevice,
	FdcRequest,
	FdcIsr
};


void FdcAddDevice(driver_t *drv, const wchar_t *name, dev_config_t *cfg)
{
	fdc_t *fdc;

	fdc = malloc(sizeof(*fdc));
	if (fdc == NULL)
		return;

	memset(fdc, 0, sizeof(*fdc));
	fdc->base = 0x3f0;
	fdc->irq = 6;
	fdc->statsz = 0;
	fdc->fdc_track = -1;
	fdc->motor_end = -1;
	fdc->motor_on = false;

	fdc->transfer_phys = alloc_dma_buffer();
	fdc->transfer_buffer = sbrk_virtual(PAGE_SIZE);
	MemMapRange(fdc->transfer_buffer, 
		fdc->transfer_phys, 
		fdc->transfer_buffer + PAGE_SIZE,
		PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
	TRACE2("DMA transfer buffer is at 0x%x => %p\n", 
		fdc->transfer_phys, fdc->transfer_buffer);

	fdc->geometry.heads = DG144_HEADS;
	fdc->geometry.tracks = DG144_TRACKS;
	fdc->geometry.spt = DG144_SPT;
	fdc->total_bytes = fdc->geometry.heads 
		* fdc->geometry.tracks 
		* fdc->geometry.spt * 512;
	/*wprintf(L"fdc: total_bytes = %lu\n", (uint32_t) fdc->total_bytes);*/

	if (cfg->device_class == 0)
		cfg->device_class = 0x0102;

	fdc->device = DevAddDevice(drv, &fdc_vtbl, 0, name, cfg, fdc);

	DevRegisterIrq(0, fdc->device);
	DevRegisterIrq(fdc->irq, fdc->device);

	/* Reset the controller */
	fdc->op = fdcReset;
	fdc->sensei = true;
	out(fdc->base + REG_DOR, 0);
	out(fdc->base + REG_DOR, DOR_DEFAULT);

	/*while (fdc->op != fdcIdle)
		;*/
	while ((in(fdc->base + REG_MSR) & MSR_MRQ) == 0)
		;

	/* Specify drive timings (got these off the BIOS) */
	FdcSendByte(fdc, CMD_SPECIFY);
	FdcSendByte(fdc, 0xdf);	/* SRT = 3ms, HUT = 240ms */
	FdcSendByte(fdc, 0x02);	/* HLT = 16ms, ND = 0 */

	/* Recalibrate the drive */
	fdc->op = fdcReset;
	FdcMotorOn(fdc);
	while (fdc->op != fdcIdle)
		;

	fdc->op = fdcReset;
	fdc->sensei = true;
	FdcSendByte(fdc, CMD_SEEK);
	FdcSendByte(fdc, 0);
	FdcSendByte(fdc, 1);
	while (fdc->op != fdcIdle)
		;

	fdc->op = fdcReset;
	fdc->sensei = true;
	FdcSendByte(fdc, CMD_RECAL);
	FdcSendByte(fdc, 0);
	while (fdc->op != fdcIdle)
		;

	if (fdc->sr0 & 0xC0)
	{
		wprintf(L"fdc: recalibrate failed: sr0 = %x\n", fdc->sr0);
		FdcDeleteDevice(fdc->device);
		return;
	}

	fdc->fdc_track = 0;
	FdcMotorOff(fdc);
}

bool DrvInit(driver_t *drv)
{
	drv->add_device = FdcAddDevice;
	drv->mount_fs = NULL;
	return true;
}
