/* Copyright 1999, Brian J. Swetland. All rights reserved.
** Distributed under the terms of the OpenBLT License
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <kernel/driver.h>
#include "pci.h"

typedef struct confadd
{
	byte reg:8;
	byte func:3;
	byte dev:5;
	byte bus:8;
	byte rsvd:7;
	byte enable:1;
} confadd;

dword pci_read(int bus, int dev, int func, int reg, int bytes)
{
	word base;
	
	union {
		confadd c;
		dword n;
	} u;
	
	u.c.enable = 1;
	u.c.rsvd = 0;
	u.c.bus = bus;
	u.c.dev = dev;
	u.c.func = func;
	u.c.reg = reg & 0xFC;
	
	out32(0xCF8, u.n);
	
	base = 0xCFC + (reg & 0x03);
		
	switch(bytes){
	case 1: return in(base);
	case 2: return in16(base);
	case 4: return in32(base);
	default: return 0;
	}
}

void pci_write(int bus, int dev, int func, int reg, dword v, int bytes)
{
	word base;
	
	union {
		confadd c;
		dword n;
	} u;
	
	u.c.enable = 1;
	u.c.rsvd = 0;
	u.c.bus = bus;
	u.c.dev = dev;
	u.c.func = func;
	u.c.reg = reg & 0xFC;
	
	base = 0xCFC + (reg & 0x03);
	out32(0xCF8, u.n);
	switch(bytes){
	case 1: out(base, (byte) v); break;
	case 2: out16(base, (word) v); break;
	case 4: out32(base, v); break;
	}
	
}

int pci_probe(int bus, int dev, int func, pci_cfg_t *cfg)
{
	dword *word = (dword *) cfg;
	dword v;	
	int i;
	for(i=0;i<4;i++){
		word[i] = pci_read(bus,dev,func,4*i,4);
	}
	if(cfg->vendor_id == 0xffff) return 1;

	cfg->bus = bus;
	cfg->dev = dev;
	cfg->func = func;
#if 1
	wprintf(L"Device Info: /bus/pci/%d/%d/%d\n",bus,dev,func);
	wprintf(L"  * Vendor: %X   Device: %X  Class/SubClass/Interface %X/%X/%X\n",
		   cfg->vendor_id,cfg->device_id,cfg->base_class,cfg->sub_class,cfg->interface);
	wprintf(L"  * Status: %X  Command: %X  BIST/Type/Lat/CLS: %X/%X/%X/%X\n",
		   cfg->status, cfg->command, cfg->bist, cfg->header_type, 
		   cfg->latency_timer, cfg->cache_line_size);
#endif
			
	switch(cfg->header_type & 0x7F){
	case 0: /* normal device */
		for(i=0;i<6;i++){
			v = pci_read(bus,dev,func,i*4 + 0x10, 4);
			if(v) {
				int v2;
				pci_write(bus,dev,func,i*4 + 0x10, 0xffffffff, 4);
				v2 = pci_read(bus,dev,func,i*4+0x10, 4) & 0xfffffff0;
				pci_write(bus,dev,func,i*4 + 0x10, v, 4);
				v2 = 1 + ~v2;
				if(v & 1) {
//					printf("  * Base Register %d IO: %x (%x)\n",i,v&0xfff0,v2&0xffff);
					cfg->base[i] = v & 0xffff;
					cfg->size[i] = v2 & 0xffff;
				} else {
//					printf("  * Base Register %d MM: %x (%x)\n",i,v&0xfffffff0,v2);
					cfg->base[i] = v;
					cfg->size[i] = v2;
				}
			} else {
				cfg->base[i] = 0;
				cfg->size[i] = 0;
			}
			
		}
		v = pci_read(bus,dev,func,0x3c,1);
		cfg->irq = (v == 0xff ? 0 : v);
			
		wprintf(L"  * Interrupt Line: %X\n",cfg->irq);
		break;
	case 1:
		wprintf(L"  * PCI <-> PCI Bridge\n");
		break;
	default:
		wprintf(L"  * Unknown Header Type\n");
	}
	return 0;	
}

bool pci_find(word vendor_id, word device_id, pci_cfg_t* cfg)
{
	word bus, dev;

	for (bus = 0; bus < 255; bus++)
	{
		for(dev = 0;dev < 32; dev++)
		{
			if (pci_probe(bus, dev, 0, cfg))
				continue;

			if (cfg->vendor_id == vendor_id && cfg->device_id == device_id)
				return true;
		}
	}

	return false;
}