/* $Id: sb.c,v 1.3 2002/08/17 22:52:06 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <os/syscall.h>
#include <stdio.h>
#include "../sound.h"

typedef struct sb_t sb_t;
struct sb_t
{
    sound_t sound;
    uint16_t iobase;
    uint8_t irq;
    device_t *dev;
};

#define	PEEK_DLY		100
#define	POKE_DLY		100
#define	RESET_DLY		100

#define	SBREG_MIX_ADR	0x04
#define	SBREG_MIX_DATA	0x05
#define	SBREG_RESET		0x06
#define	SBREG_READ		0x0A
#define	SBREG_WRITE		0x0C
#define	SBREG_POLL		0x0E

#define	SBCMD_PLAY		0x14	/* 8-bit DMA low-speed playback */
#define	SBCMD_SET_RATE	0x40	/* set sampling rate */
#define	SBCMD_SPKR_ON	0xD1	/* turn on speaker */
#define	SBCMD_SPKR_OFF	0xD3	/* turn OFF speaker */
#define	SBCMD_VERSION	0xE1	/* get DSP version */
#define	SBCMD_INTERRUPT	0xF2	/* generate an interrupts (for IRQ probe) */

static void delay(unsigned ms)
{
    unsigned end;
    end = SysUpTime() + ms;
    while (SysUpTime() < end)
        ArchProcessorIdle();
}

static bool SbPoke(sb_t *sb, uint8_t val)
{
	unsigned temp;

	for (temp = 0; temp < POKE_DLY; temp++)
	{
		if ((in(sb->iobase + SBREG_WRITE) & 0x80) == 0)
		{
			out(sb->iobase + SBREG_WRITE, val);
			return true;
		}

		ArchMicroDelay(1000);
	}

	return false;
}

static bool SbDetect(sb_t *sb)
{
	unsigned short temp;

    /*
     * SB reset:
     * 1) Write a 1 to the reset port (2x6)
     */
	out(sb->iobase + SBREG_RESET, 0x01);

    /* 2) Wait for 3 microseconds */
	ArchMicroDelay(3);

    /* 3) Write a 0 to the reset port (2x6) */
	out(sb->iobase + SBREG_RESET, 0);

    /* 4) Poll the read-buffer status port (2xE) until bit 7 is set */
	for (temp = 0; temp < 10; temp++)
    {
        delay(1);
		if (in(sb->iobase + SBREG_POLL) & 0x80)
			break;
    }

	if (temp == 10)
		return false;

    /*
     * 5) Poll the read data port (2xA) until you receive an AA
     * 
     *  The DSP usually takes about 100 microseconds to initialized itself.
     *  After this period of time, if the return value is not AA or there
     *  is no data at all, then the SB card may not be installed or an
     *  incorrect I/O address is being used.
     */
	for (temp = 0; temp < 10; temp++)
    {
        delay(1);
		if (in(sb->iobase + SBREG_READ) == 0xAA)
			break;
    }

	if (temp == 10)
		return false;

	SbPoke(sb, SBCMD_VERSION);
	temp = in(sb->iobase + SBREG_READ);
	wprintf(L"\tDSP version: %u.%u\n", temp, in(sb->iobase + SBREG_READ));

	return true;
}

static void SbClose(sound_t *snd)
{
    free(snd);
}

static bool SbIsr(sound_t *snd, uint8_t irq)
{
    sb_t *sb;
    uint8_t status;

    sb = (sb_t*) snd;

    out(sb->iobase + 4, 0x0082);    // Select interrupt reg.
    status = in(sb->iobase + 5);    // Read interrupt reg.

    wprintf(L"SbIsr(%u): iobase = %x, status = %x\n", irq, sb->iobase, status);
    if (status & 0x01)              // Interrupt is from 8-bit DMA.
        in(sb->iobase + 0x000E);    // Acknowledge the interrupt.
    else if (status & 0x02)         // Interrupt is from 16-bit DMA.
        in(sb->iobase + 0x000F);    // Acknowledge the interrupt.
    else
        return false;

    return true;
}

bool SbOpen(sound_t *snd)
{
    static const uint16_t iobase[] =
    {
        0x220, 0x240, 0x260, 0x280
    };

    unsigned i;
    sb_t *sb;

    sb = (sb_t*) snd;
    for (i = 0; i < _countof(iobase); i++)
    {
        wprintf(L"sb: trying port %x...\n", iobase[i]);
        sb->iobase = iobase[i];
        if (SbDetect(sb))
            break;
    }

    if (i == _countof(iobase))
    {
        wprintf(L"sb: no Sound Blaster found\n");
        return false;
    }

    return true;
}

static const sound_vtbl_t sb_vtbl =
{
    SbOpen,
    SbClose,
    SbIsr,
};

sound_t *DllMainCRTStartup(device_t *dev, device_config_t *cfg)
{
    sb_t *sb;

    sb = malloc(sizeof(sb_t));
    if (sb == NULL)
        return NULL;

    memset(sb, 0, sizeof(sb_t));
    sb->sound.vtbl = &sb_vtbl;
    sb->dev = dev;
    return &sb->sound;
}
