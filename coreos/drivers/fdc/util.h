/*
 * util.h
 * 
 * header for IRQ/DMA utility functions for DJGPP 2.01
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

#ifndef UTIL_H_
#define UTIL_H_

#include "mytypes.h"

/* used to store hardware definition of DMA channels */
typedef struct DmaChannel {
   uint8_t page;     /* page register */
   uint8_t offset;   /* offset register */
   uint8_t length;   /* length register */
} DmaChannel;

/* function prototypes */
long alloc_dma_buffer();
void dma_xfer(int channel,long physaddr,int length,BOOL read);

/* inline funcs */
extern __inline void wfill(uint16_t *start,UINT32 size,uint16_t value)
{
	__asm__ __volatile__ ("cld\n"
		 "\trep\n"
		 "\tstosw"
		 : /* no outputs */
		 : "D"(start),"c"(size),"a"(value)
		 : "%edi","%ecx");
}

#endif /* UTIL_H_ */
