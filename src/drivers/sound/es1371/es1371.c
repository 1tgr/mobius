/* $Id: es1371.c,v 1.1 2002/08/17 22:52:06 pavlovskii Exp $ */

/*
 * This is driver partially based on the BeOS R4 ES1370 driver, and partially
 *  on the FreeBSD ES1371 driver.
 */
/* 
	es1370.c - Device driver for the Ensoniq AudioPCI ES1370 device.

    Copyright (C) 1998 HockYiung Huang (leclec@pacific.net.sg)
    Port to R4 (C) 1999 Marc Schefer (mschefer@iprolink.ch)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/*
 * Support Sound Cards based on the Ensoniq/Creative Labs ES1371/1373 
 *
 * Copyright (c) 1999 by Russell Cattelan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Russell Cattelan.
 *
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pci/es1371.c,v 1.1.2.2 1999/11/16 22:04:45 roger Exp $
 */

#include <kernel/driver.h>
#include <stdio.h>
#include <errno.h>
#include "../sound.h"
#include "es1371.h"

typedef struct es_t es_t;
struct es_t
{
    sound_t sound;
    uint16_t iobase;
    uint8_t irq;
    device_t *dev;
    spinlock_t sem_play;
    unsigned write_count, play_count;
    page_array_t *pages;
    enum { stopped = 0, playing, recording } state;
};

typedef struct audio_control_t audio_control_t;
struct audio_control_t
{
    const wchar_t *name;
    uint8_t index;
    uint8_t shift_left;
    uint8_t shift_right;
    uint8_t width_left;
    uint8_t width_right;
    bool has_mute;
    uint8_t default_left;
    uint8_t default_right;
    bool default_mute;
};

static audio_control_t es1371_defaults[] =
{
    /*  Name                Index SL SR WL WR Mute?  DL  DR  DefMute */
    { L"Master Volume",     0x02, 8, 0, 5, 5, true,  15, 15, false },
    { L"AUX Out Volume",    0x04, 8, 0, 5, 5, true,  15, 15, false },
    { L"Mono Volume",       0x06, 0, 0, 0, 5, true,  0,  15, false },
    { L"Bass",              0x08, 0, 8, 0, 4, false, 0,  0,  false },
    { L"Treble",            0x08, 0, 0, 0, 4, false, 0,  0,  false },
    { L"PC Beep Volume",    0x0a, 0, 1, 0, 4, true,  0,  0,  false },
    { L"Phone Volume",      0x0c, 0, 0, 0, 5, true,  0,  15, false },
    { L"Mic Volume",        0x0e, 0, 0, 0, 5, true,  0,  15, false },
    { L"Line In Volume",    0x10, 8, 0, 5, 5, true,  15, 15, false },
    { L"CD Volume",         0x12, 8, 0, 5, 5, true,  15, 15, false },
    { L"Video Volume",      0x14, 8, 0, 5, 5, true,  15, 15, false },
    { L"AUX In Volume",     0x16, 8, 0, 5, 5, true,  15, 15, false },
    { L"PCM Out Volume",    0x18, 8, 0, 5, 5, true,  15, 15, false },
};

/*
 * Write a word to one of the sample rate converter registers
 */

static void EsSrcWrite(es_t *es, uint16_t index, uint16_t data)
{
    uint32_t s;

    while ((s = in32(es->iobase + ES1371_REG_SRC)) & SRC_RAM_BUSY)
        wprintf(L"EsSrcWrite: busy (%x)\n", s);

    out32(es->iobase + ES1371_REG_SRC, 
        SRC_RAM_WE | SRC_DIS_REC | SRC_DIS_P1 | SRC_DIS_P2 | SRC_DISABLE
        | ((index << SRC_RAM_ADR_SHIFT) & SRC_RAM_ADR_MASK)
        | data);
}

/*
 * Read a word from one of the sample rate converter registers
 */

static uint16_t EsSrcRead(es_t *es, uint16_t index)
{
    uint32_t s;

    while ((s = in32(es->iobase + ES1371_REG_SRC)) & SRC_RAM_BUSY)
        wprintf(L"EsSrcRead: busy (%x)\n", s);

    out32(es->iobase + ES1371_REG_SRC, 
        SRC_DIS_REC | SRC_DIS_P1 | SRC_DIS_P2 | SRC_DISABLE
        | ((index << SRC_RAM_ADR_SHIFT) & SRC_RAM_ADR_MASK));
    return in32(es->iobase + ES1371_REG_SRC) & SRC_RAM_DATA_MASK;
}

/*
 * Calculate a compatible sample rate from the one given, and optionally
 *  program the ADC with it
 */

static unsigned EsAdcRate(es_t *es, unsigned rate, bool do_set)
{
    unsigned n, truncm, freq, result;

    if (rate > 48000) rate = 48000;
    if (rate < 4000) rate = 4000;
    n = rate / 3000;
    if ((1 << n) & ((1 << 15) | (1 << 13) | (1 << 11) | (1 << 9)))
        n--;
    truncm = (21 * n - 1) | 1;
    freq = ((48000UL << 15) / rate) * n;
    result = (48000UL << 15) / (freq / n);
    if (do_set)
    {
        if (rate >= 24000)
        {
            if (truncm > 239)
                truncm = 239;
            EsSrcWrite(es, SRC_REG_ADC + SRC_REG_PLAY_TRUNC_N,
                (((239 - truncm) >> 1) << 9) | (n << 4));
        }
        else
        {
            if (truncm > 119)
                truncm = 119;
            EsSrcWrite(es, SRC_REG_ADC + SRC_REG_PLAY_TRUNC_N,
                0x8000 | (((119 - truncm) >> 1) << 9) | (n << 4));
        }

        EsSrcWrite(es, SRC_REG_ADC + SRC_REG_PLAY_INT_REGS,
            (EsSrcRead(es, SRC_REG_ADC + SRC_REG_PLAY_INT_REGS) & 0x00ff) 
            | ((freq >> 5) & 0xfc00));
        EsSrcWrite(es, SRC_REG_ADC + SRC_REG_PLAY_VFREQ_FRAC, freq & 0x7fff);
        EsSrcWrite(es, SRC_REG_VOL_ADC_LEFT,  n << 8);
        EsSrcWrite(es, SRC_REG_VOL_ADC_RIGHT, n << 8);
    }

    return result;
}

/*
 * Calculate a compatible sample rate from the one given, and optionally
 *  program the one of the DACs with it
 */

static unsigned EsDacRate(es_t *es, unsigned rate, int set)
{
    unsigned freq, r, result, dac, dis;
    uint32_t s;

    if (rate > 48000) rate = 48000;
    if (rate < 4000) rate = 4000;
    freq = (rate << 15) / 3000;
    result = (freq * 3000) >> 15;
    if (set != 0)
    {
        dac = (set == 1) ? SRC_REG_DAC1 : SRC_REG_DAC2;
        dis = (set == 1) ? SRC_DIS_P2 : SRC_DIS_P1;

        /* Wait until SRC is not accessing its RAM */
        while ((s = in32(es->iobase + ES1371_REG_SRC)) & SRC_RAM_BUSY)
            wprintf(L"EsDacRate: busy (%x)\n", s);

        /* Disable all output */
        r = (s & (SRC_DISABLE | SRC_DIS_P1 | SRC_DIS_P2 | SRC_DIS_REC));
        out32(es->iobase + ES1371_REG_SRC, r);

        /*
         * Write the integer and real parts of the frequency to the relevant 
         *  SRC registers
         */
        EsSrcWrite(es, dac + SRC_REG_PLAY_INT_REGS,
            (EsSrcRead(es, dac + SRC_REG_PLAY_INT_REGS) & 0x00ff) 
            | ((freq >> 5) & 0xfc00));
        EsSrcWrite(es, dac + SRC_REG_PLAY_VFREQ_FRAC, freq & 0x7fff);

        /* Wait until SRC is not accessing its RAM */
        while ((s = in32(es->iobase + ES1371_REG_SRC)) & SRC_RAM_BUSY)
            wprintf(L"EsDacRate: busy (%x)\n", s);

        /* Re-enable the appropriate DAC if we disabled it */
        r = (s & (SRC_DISABLE | dis | SRC_DIS_REC));
        out32(es->iobase + ES1371_REG_SRC, r);
    }

    return result;
}

/*
 * Give DAC2 the address and size of a playback buffer in physical memory
 *  Maximum size is 262144 bytes = (0xffff + 1) * 4
 */

static void EsSetPlaybackBuffer(es_t *es, addr_t phys, uint32_t size)
{
    /* Memory page register = 1100b. */
    out32(es->iobase + ES1371_REG_PAGE, PAGE_DAC_FRAME);

    /* Program the DAC2 frame register 1 */
    out32(es->iobase + ES1371_REG_MEMORY + ES1371_MEM_DAC2_FRAME_1, phys);

    /*
     * Program the DAC2 frame register 2 -- the current sample count (upper 
     *  half of longword) gets zeroed
     */
    size /= 4;
    out32(es->iobase + ES1371_REG_MEMORY + ES1371_MEM_DAC2_FRAME_2, size - 1);
}

/*
 * Set the format (mono vs. stereo, 8-bit vs. 16-bit) for DAC2
 */

static void EsSetPlaybackFormat(es_t *es, bool format_stereo, bool format_16_bit)
{
    uint32_t mask;
    uint8_t bits;
    uint32_t control;

    mask = SER_FMT_STEREO | SER_FMT_16BIT;
    mask <<= SER_P2_FMT_SHIFT;
    bits = format_stereo ? SER_FMT_STEREO : 0;
    bits |= format_16_bit ? SER_FMT_16BIT : 0;
    bits <<= SER_P2_FMT_SHIFT;

    control = in32(es->iobase + ES1371_REG_SER_CONTROL);
    control &= ~mask;

    out32(es->iobase + ES1371_REG_SER_CONTROL, control | bits);
}

/*
 * Set the number of samples, minus one, that DAC2 will play back
 */

void EsSetSampleCount(es_t *es, uint32_t sample_count)
{
    out32(es->iobase + ES1371_REG_DAC2_SAMPLE_COUNT, sample_count);
}

/*
 * Starts playback on DAC2
 */

static void EsStartPlayback(es_t *es)
{
    uint32_t control;

    /* P2_END_INC ?? huh... where is next buffer 8bit : +1; 16bit : +2; */
    control = in32(es->iobase + ES1371_REG_SER_CONTROL);
    control &= ~SER_P2_ST_INC_MASK;

    /* need frame alignment ??? */
    if (control & SER_P2_EB)
        /* 16 bits per sample */
        control |= 2 << SER_P2_END_INC_SHIFT;
    else
        /* 8 bits per sample */
        control |= 1 << SER_P2_END_INC_SHIFT;

    out32(es->iobase + ES1371_REG_SER_CONTROL, control);

    /* need frame alignment ??? */
    /* P2_ST_INC ?? zero lah... */
    control = in32(es->iobase + ES1371_REG_SER_CONTROL);
    control &= ~SER_P2_END_INC_MASK;
    out32(es->iobase + ES1371_REG_SER_CONTROL, control);

    control = in32(es->iobase + ES1371_REG_SER_CONTROL);
    control &= ~SER_P2_LOOP_SEL;    /* loop mode, not stop mode */
    control &= ~SER_P2_PAUSE;       /* don't pause */
    control &= ~SER_P2_DAC_SEN;     /* play silence in stop mode */
    out32(es->iobase + ES1371_REG_SER_CONTROL, control);

    /* clear interrupt */
    control = in32(es->iobase + ES1371_REG_SER_CONTROL);
    control &= ~SER_P2_INT_EN;
    out32(es->iobase + ES1371_REG_SER_CONTROL, control);
    control |= SER_P2_INT_EN;
    out32(es->iobase + ES1371_REG_SER_CONTROL, control);

    /* enable DAC2 */
    control = in32(es->iobase + ES1371_REG_ICS_CONTROL);
    control &= ~ICS_DAC2_EN;
    out32(es->iobase + ES1371_REG_ICS_CONTROL, control);
    control |= ICS_DAC2_EN;
    out32(es->iobase + ES1371_REG_ICS_CONTROL, control);
}

/*
 * Stops playback on DAC2
 */

void EsStopPlayback(es_t *es)
{
    uint32_t control;

    /* Pause... */
    control = in32(es->iobase + ES1371_REG_SER_CONTROL);
    control |= SER_P2_PAUSE;
    out32(es->iobase + ES1371_REG_SER_CONTROL,control);

    /* Stop by disabling DAC2 */
    control = in32(es->iobase + ES1371_REG_ICS_CONTROL);
    control &= ~ICS_DAC2_EN;
    out32(es->iobase + ES1371_REG_ICS_CONTROL, control);
}

/*
 * Writes a byte to one of the AC97 registers
 */

static void EsCodecWrite(es_t *es, uint8_t reg, uint16_t value)
{
    uint32_t s;

    while ((s = in32(es->iobase + ES1371_REG_CODEC_READ)) & CODEC_WIP)
        ;/*wprintf(L"EsCodecWrite(%x, %x): busy, s = %x\n", reg, value, s); */

    out32(es->iobase + ES1371_REG_CODEC_WRITE, 
        (((reg << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) | value));
}

/*
 * Reads a byte from one of the AC97 registers
 */

static uint16_t EsCodecRead(es_t *es, uint8_t reg)
{
    uint32_t s;

    while ((s = in32(es->iobase + ES1371_REG_CODEC_READ)) & CODEC_WIP)
        ;/*wprintf(L"EsCodecRead(%x): busy, s = %x\n", reg, s); */

    out32(es->iobase + ES1371_REG_CODEC_WRITE,
        ((reg << CODEC_PIADD_SHIFT) & CODEC_PIADD_MASK) | CODEC_PIRD);

    while (((s = in32(es->iobase + ES1371_REG_CODEC_READ)) & CODEC_RDY) == 0)
        ;/*wprintf(L"EsCodecRead(%x): not ready, s = %x\n", reg, s); */

    return s & CODEC_PIDAT_MASK;
}

static void EsSetupSound(es_t *es, const audio_control_t *controls, unsigned num_controls)
{
    unsigned i;
    uint16_t val, mask;

    for (i = 0; i < num_controls; i++)
    {
        val = 0;
        if (controls[i].width_left != 0)
        {
            mask = ~(((uint8_t) 0xffff) << controls[i].width_left) << controls[i].shift_left;
            val |= (controls[i].default_left << controls[i].shift_left) & mask;
        }

        if (controls[i].width_right != 0)
        {
            mask = ~(((uint8_t) 0xffff) << controls[i].width_right) << controls[i].shift_right;
            val |= (controls[i].default_right << controls[i].shift_right) & mask;
        }

        if (controls[i].has_mute && controls[i].default_mute)
            val |= 0x8000;

        EsCodecWrite(es, controls[i].index, val);
    }
}

static void EsSetupDefaults(es_t *es, audio_control_t *controls, unsigned num_controls)
{
    unsigned i;
    uint16_t val, mask;

    for (i = 0; i < num_controls; i++)
    {
        val = EsCodecRead(es, controls[i].index);

        if (controls[i].width_left != 0)
        {
            mask = ~(0xffff << controls[i].width_left) << controls[i].shift_left;
            controls[i].default_left = (val & mask) >> controls[i].shift_left;
        }
        else
            controls[i].default_left = 0;

        if (controls[i].width_right != 0)
        {
            mask = ~(0xffff << controls[i].width_right) << controls[i].shift_right;
            controls[i].default_right = (val & mask) >> controls[i].shift_right;
        }
        else
            controls[i].default_right = 0;

        if (controls[i].has_mute)
            controls[i].default_mute = (val & 0x8000) != 0;

        /*wprintf(L"es1371: %s: L=%u R=%u %s\n", 
            controls[i].name,
            controls[i].default_left,
            controls[i].default_right,
            controls[i].default_mute ? L"muted" : L"not muted");*/
    }
}

#define RESET_POWER_DOWN_REGISTER       0x16
/* bit to power down & reset */
#define B_RESET_POWER_DOWN_PD           0x02
#define B_RESET_POWER_DOWN_RST          0x01

static void EsResetCodec(es_t *es)
{
    /* #RST = 0, #PD & #RST will init to 1 */
    out32(es->iobase + ES1371_REG_CODEC_WRITE,
        ((RESET_POWER_DOWN_REGISTER << 8) | B_RESET_POWER_DOWN_PD));

    /* Some delay required */
    ArchMicroDelay(20);

    out32(es->iobase + ES1371_REG_CODEC_WRITE,
        ((RESET_POWER_DOWN_REGISTER << 8) | B_RESET_POWER_DOWN_PD | B_RESET_POWER_DOWN_RST));
    /* Some delay required */
    ArchMicroDelay(20);
}

static bool EsInitHardware(es_t *es)
{
    unsigned i, rate;

    DevRegisterIrq(es->irq, es->dev);
    /*wprintf(L"es1371: ics = %08x, ics_default = %08x\n",
        in32(es->iobase + ES1371_REG_ICS_CONTROL), ICS_CONTROL_DEFAULT);*/

    /* initialize the chips */
	out32(es->iobase + ES1371_REG_ICS_CONTROL, 0);
	out32(es->iobase + ES1371_REG_SER_CONTROL, 0);
	out32(es->iobase + ES1371_REG_LEGACY, 0);

	/* Warm reset the AC97 codec */
    out32(es->iobase + ES1371_REG_ICS_CONTROL, ICS_CONTROL_DEFAULT | ICS_SYNC_RES);
    ArchMicroDelay(2);
    out32(es->iobase + ES1371_REG_ICS_CONTROL, ICS_CONTROL_DEFAULT);

    /*
     * xxx - is it necessary to reset the codec through the registers as well 
     *  as getting the ES1371 to do it?
     */

    EsResetCodec(es);

    /*wprintf(L"es1371: icsc = %08x (%08x), icss = %08x (%08x)\n",
        in32(es->iobase + ES1371_REG_ICS_CONTROL), ICS_CONTROL_DEFAULT,
        in32(es->iobase + ES1371_REG_ICS_STATUS), ICS_STATUS_DEFAULT);
    wprintf(L"es1371: src = %08x (00000000)\n",
        in32(es->iobase + ES1371_REG_SRC));*/

    /* Initialise the sample rate converter */
    out32(es->iobase + ES1371_REG_SRC, SRC_DISABLE);

    /* Zero the SRC's memory */
    for (i = 0; i < 0x80; i++)
        EsSrcWrite(es, i, 0);

    /* Configure the SRC's registers at the end of its RAM */
    EsSrcWrite(es, SRC_REG_DAC1 + SRC_REG_PLAY_TRUNC_N,  16 << 4);
    EsSrcWrite(es, SRC_REG_DAC1 + SRC_REG_PLAY_INT_REGS, 16 << 10);
    EsSrcWrite(es, SRC_REG_DAC2 + SRC_REG_PLAY_TRUNC_N,  16 << 4);
    EsSrcWrite(es, SRC_REG_DAC2 + SRC_REG_PLAY_INT_REGS, 16 << 10);
    EsSrcWrite(es, SRC_REG_ADC  + SRC_REG_PLAY_TRUNC_N,  16 << 4);
    EsSrcWrite(es, SRC_REG_ADC  + SRC_REG_PLAY_INT_REGS, 16 << 10);

    EsSrcWrite(es, SRC_REG_VOL_ADC_LEFT,   1 << 8);
    EsSrcWrite(es, SRC_REG_VOL_ADC_RIGHT,  1 << 8);

    EsSrcWrite(es, SRC_REG_VOL_DAC1_LEFT,  1 << 12);
    EsSrcWrite(es, SRC_REG_VOL_DAC1_RIGHT, 1 << 12);
    EsSrcWrite(es, SRC_REG_VOL_DAC2_LEFT,  1 << 12);
    EsSrcWrite(es, SRC_REG_VOL_DAC2_RIGHT, 1 << 12);

    /* Initialise ADC, DAC1 and DAC2 PLAY/REC registers */
    rate = EsAdcRate(es, 22050, true);
    //wprintf(L"EsInitHardware: adc rate = %u\n", rate);
    rate = EsDacRate(es, 22050, 1);
    //wprintf(L"EsInitHardware: dac1 rate = %u\n", rate);
    rate = EsDacRate(es, 22050, 2);
    //wprintf(L"EsInitHardware: dac2 rate = %u\n", rate);

    /* Enable the SRC */
    out32(es->iobase + ES1371_REG_SRC, 0);

    EsSetupSound(es, es1371_defaults, _countof(es1371_defaults));
    EsSetupDefaults(es, es1371_defaults, _countof(es1371_defaults));
    return true;
}

bool EsOpen(sound_t *snd)
{
    return EsInitHardware((es_t*) snd);
}

void EsClose(sound_t *snd)
{
    free(snd);
}

bool EsIsr(sound_t *snd, uint8_t irq)
{
    es_t *es;
    /*unsigned st; */
    uint32_t interrupt_status;

    es = (es_t*) snd;
    /*st = SysUpTime(); */
    interrupt_status = in32(es->iobase + ES1371_REG_ICS_STATUS);
    /*wprintf(L"EsIsr(%u): status = %08x\n", irq, interrupt_status); */

    if (interrupt_status & ICS_INTR)
    {
        /* UART interrupt */
#if 0
        if (interrupt_status & ICS_UART)
        {
            uint8 status, control;
            
            status = dio_read_register_byte(UART_STATUS_REGISTER);

            /* If there is outstanding work to do and there is a port-driver for */
            /* the uart miniport... */
            if (!(status & B_UART_STATUS_RXRDY))
            {
                /* ...notify the MPU port driver to do work. */
            }

            control = dio_read_register_byte(UART_CONTROL_REGISTER);

            /* Ack the interrupt */
            if (status & B_UART_STATUS_RXINT)
            {
                control &= ~B_UART_CONTROL_RXINTEN;
                dio_write_register_byte(UART_CONTROL_REGISTER,control);
                control |= B_UART_CONTROL_RXINTEN;
                dio_write_register_byte(UART_CONTROL_REGISTER,control);
            }
            else
            {
                control &= ~B_UART_CONTROL_TXINTEN;
                dio_write_register_byte(UART_CONTROL_REGISTER,control);
                control |= B_UART_CONTROL_TXRDYINTEN;
                dio_write_register_byte(UART_CONTROL_REGISTER,control);
            }
        }
#endif

        /*  ADC/DAC2 Interrupt */
        if (interrupt_status & (ICS_DAC2 | ICS_ADC));
        {
            uint32_t control = in32(es->iobase + ES1371_REG_SER_CONTROL);

            /* Ack the interrupt */
            if (interrupt_status & ICS_DAC2)
            {
                if (es->state == playing)
                {
                    SpinAcquire(&es->sem_play);

                    es->write_count++;
                    wprintf(L"EsIsr(DAC2): write_count = %u/%u\n", 
                        es->write_count, es->play_count);

                    if (es->write_count >= es->play_count)
                    {
                        wprintf(L"EsIsr(DAC2): finished\n");
                        es->state = stopped;
                        EsStopPlayback(es);
                        SpinRelease(&es->sem_play);
                        SndFinishedBuffer(es->dev);
                    }
                    else
                    {
                        EsSetPlaybackBuffer(es, 
                            es->pages->pages[es->write_count] + es->pages->mod_first_page, 
                            PAGE_SIZE);
                        SpinRelease(&es->sem_play);
                    }
                }

                /* Ack DAC2 channel */
                control &= ~SER_P2_INT_EN;
                out32(es->iobase + ES1371_REG_SER_CONTROL, control);
                control |= SER_P2_INT_EN;
                out32(es->iobase + ES1371_REG_SER_CONTROL, control);
            }
            else
            {
                /* Ack ADC channel */
                control &= ~SER_R1_INT_EN;
                out32(es->iobase + ES1371_REG_SER_CONTROL,control);
                control |= SER_R1_INT_EN;
                out32(es->iobase + ES1371_REG_SER_CONTROL,control);
            }
        }

        /* DAC1 interrupt */
        if (interrupt_status & ICS_DAC1)
        {
            /* just ack it... */
            uint32_t control = in32(es->iobase + ES1371_REG_SER_CONTROL);

            control &= ~SER_P1_INT_EN;
            out32(es->iobase + ES1371_REG_SER_CONTROL,control);
            control |= SER_P1_INT_EN;
            out32(es->iobase + ES1371_REG_SER_CONTROL,control);
        }

        /* CCB interrupt */
        if (interrupt_status & ICS_MCCB)
        {
            /* just ack it... */
            uint32_t control = in32(es->iobase + ES1371_REG_ICS_CONTROL);

            control &= ~ICS_CCB_INTRM;
            out32(es->iobase + ES1371_REG_ICS_CONTROL,control);
            control |= ICS_CCB_INTRM;
            out32(es->iobase + ES1371_REG_ICS_CONTROL,control);
        }

        return true;
    }

    return false;
}

CASSERT(PAGE_SIZE <= 262144);

status_t EsStartBuffer(sound_t *snd, page_array_t *pages, size_t length, bool is_recording)
{
    es_t *es;

    es = (es_t*) snd;

    if (is_recording)
        return ENOTIMPL;

    if (es->state != stopped)
        return EACCESS;

    SpinAcquire(&es->sem_play);
    es->write_count = 0;
    es->play_count = PAGE_ALIGN_UP(length) / PAGE_SIZE;
    es->pages = MemCopyPageArray(pages);
    es->state = is_recording ? recording : playing;

    wprintf(L"EsStartBuffer: phys = %x\n", 
        es->pages->pages[0] + es->pages->mod_first_page);
    EsSetPlaybackBuffer(es, 
        es->pages->pages[0] + es->pages->mod_first_page, 
        PAGE_SIZE);
    EsSetSampleCount(es, PAGE_SIZE - 1);
    EsSetPlaybackFormat(es, false, false);
    EsStartPlayback(es);
    SpinRelease(&es->sem_play);

    return 0;
}

static const sound_vtbl_t es_vtbl =
{
    EsOpen,
    EsClose,
    EsIsr,
    EsStartBuffer,
};

/* xxx - this should be called EsInit or something */
sound_t *DllMainCRTStartup(device_t *dev, device_config_t *cfg)
{
    es_t *es;
    unsigned i;
    uint32_t command;

    if (cfg == NULL)
    {
        wprintf(L"es1371: no PnP configuration present\n");
        return NULL;
    }

    if (cfg->vendor_id != 0x1274 ||
        cfg->device_id != 0x1371)
    {
        wprintf(L"es1371: incorrect device/vendor IDs (%04x:%04x; expecting 1274:1371)\n",
            cfg->vendor_id, cfg->device_id);
        return NULL;
    }

    es = malloc(sizeof(es_t));
    if (es == NULL)
        return NULL;

    memset(es, 0, sizeof(es_t));
    es->iobase = es->irq = 0;
    es->state = stopped;
    SpinInit(&es->sem_play);

    for (i = 0; i < cfg->num_resources; i++)
    {
        if (cfg->resources[i].cls == resIo)
            es->iobase = cfg->resources[i].u.io.base & ~1;
        if (cfg->resources[i].cls == resIrq)
            es->irq = cfg->resources[i].u.irq;
    }

    if (es->iobase == 0 || es->irq == 0)
    {
        wprintf(L"es1371: no I/O port base or IRQ found\n");
        free(es);
        return NULL;
    }

    command = PciReadConfig(&cfg->location.pci, 0x04, 2);
    /* turn on bus master support */
    command |= 4;
    PciWriteConfig(&cfg->location.pci, 0x04, 1, command);

    //wprintf(L"es1371: iobase = %x, irq = %u\n", es->iobase, es->irq);
    es->sound.vtbl = &es_vtbl;
    es->dev = dev;
    return &es->sound;
}

