/* $Id: fdc.h,v 1.3 2002/01/03 01:24:01 pavlovskii Exp $ */

/*
 * fdc.h
 * 
 * header for floppy controller handler
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
 *                   10 Eastbrooke
 *                   Highstead Road
 *                   Rondebosch 7700
 *                   South Africa
 */

#ifndef FDC_H_
#define FDC_H_

#include "mytypes.h"

/*!
 *  \ingroup	drivers
 *  \defgroup	fdc Floppy drive controller
 *  @{
 */

/* datatypes */

/* drive geometry */
typedef struct DrvGeom {
   uint8_t heads;
   uint8_t tracks;
   uint8_t spt;     /* sectors per track */
} DrvGeom;

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

/* IO ports */
#define FDC_DOR  (0x3f2)   /* Digital Output Register */
#define FDC_MSR  (0x3f4)   /* Main Status Register (input) */
#define FDC_DRS  (0x3f4)   /* Data Rate Select Register (output) */
#define FDC_DATA (0x3f5)   /* Data Register */
#define FDC_DIR  (0x3f7)   /* Digital Input Register (input) */
#define FDC_CCR  (0x3f7)   /* Configuration Control Register (output) */

/* command bytes (these are 765 commands + options such as MFM, etc) */
#define CMD_SPECIFY (0x03)  /* specify drive timings */
#define CMD_WRITE   (0xc5)  /* write data (+ MT,MFM) */
#define CMD_READ    (0xe6)  /* read data (+ MT,MFM,SK) */
#define CMD_RECAL   (0x07)  /* recalibrate */
#define CMD_SENSEI  (0x08)  /* sense interrupt status */
#define CMD_FORMAT  (0x4d)  /* format track (+ MFM) */
#define CMD_SEEK    (0x0f)  /* seek track */
#define CMD_VERSION (0x10)  /* FDC version */

#define FDC_DOR_ENABLE	0x4
#define FDC_DOR_IRQIO	0x8
#define FDC_DOR_MOTOR0	0x10
#define FDC_DOR_MOTOR1	0x20
#define FDC_DOR_MOTOR2	0x40
#define FDC_DOR_MOTOR3	0x80

#define FDC_CCR_500K	0
#define FDC_CCR_250K	2

typedef struct Fdc Fdc;

/* function prototypes */

void fdcInit(Fdc* fdc);
void fdcCleanup(Fdc* fdc);

void fdcReset(Fdc* fdc);
bool fdcDiskChange(Fdc* fdc);
void fdcMotorOn(Fdc* fdc);
void fdcMotorOff(Fdc* fdc);
void fdcRecalibrate(Fdc* fdc);
bool fdcSeek(Fdc* fdc, int track);
bool fdcLogDisk(Fdc* fdc, DrvGeom *g);
bool fdcReadBlock(Fdc* fdc, int block,uint8_t *blockbuff);
bool fdcWriteBlock(Fdc* fdc, int block,uint8_t *blockbuff);
bool fdcFormatTrack(Fdc* fdc, uint8_t track,DrvGeom *g);

/*!@}*/

#endif /* FDC_H_ */
