/*
 * util.c
 * 
 * Assorted IRQ/DMA utility functions for DJGPP 2.01
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

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/memory.h>
#include <wchar.h>
#include "util.h"

/* definition of DMA channels */
const static DmaChannel dmainfo[] = {
   { 0x87, 0x00, 0x01 },
   { 0x83, 0x02, 0x03 },
   { 0x81, 0x04, 0x05 },
   { 0x82, 0x06, 0x07 }
};

/* 
 * this allocates a 4KB buffer in the < 1M range, maps it and returns the 
 * linear address, also setting the physical in the integer pointed at
 */
long alloc_dma_buffer()
{
	addr_t phys;
	phys = MemAllocLow();
	assert(phys + PAGE_SIZE < 0x100000);
	assert(((phys + PAGE_SIZE) & 0xffff) >= ((phys + PAGE_SIZE) & 0xffff));
	wprintf(L"DMA transfer buffer is at 0x%x\n", phys);
	return phys;
}

/*
 * this sets up a DMA trasfer between a device and memory.  Pass the DMA
 * channel number (0..3), the physical address of the buffer and transfer
 * length.  If 'read' is TRUE, then transfer will be from memory to device,
 * else from the device to memory.
 */
void dma_xfer(int channel,long physaddr,int length,BOOL read)
{
   long page,offset;
   
   assert(channel < 4);
   
   /* calculate dma page and offset */
   page = physaddr >> 16;
   offset = physaddr & 0xffff;
   length -= 1;  /* with dma, if you want k bytes, you ask for k - 1 */

   disable();  /* disable irq's */
   
   /* set the mask bit for the channel */
   out(0x0a,channel | 4);
   
   /* clear flipflop */
   out(0x0c,0);
   
   /* set DMA mode (write+single+r/w) */
   out(0x0b,(read ? 0x48 : 0x44) + channel);
   
   /* set DMA page */
   out(dmainfo[channel].page,page);
   
   /* set DMA offset */
   out(dmainfo[channel].offset,offset & 0xff);  /* low byte */
   out(dmainfo[channel].offset,offset >> 8);    /* high byte */
   
   /* set DMA length */
   out(dmainfo[channel].length,length & 0xff);  /* low byte */
   out(dmainfo[channel].length,length >> 8);    /* high byte */
   
   /* clear DMA mask bit */
   out(0x0a,channel);
   
   enable();  /* enable irq's */
}
