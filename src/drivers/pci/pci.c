#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

/*!
 *  \ingroup	drivers
 *  \defgroup	pci PCI bus
 *  @{
 */

typedef struct confadd
{
	byte reg:8;
	byte func:3;
	byte dev:5;
	byte bus:8;
	byte rsvd:7;
	byte enable:1;
} confadd;

const struct
{
	byte base_class;
	byte sub_class;
	byte interface;
	const wchar_t* name;
} classes[] =
{
	{ 0x00, 0x00, 0x00, L"Undefined" },
	{ 0x00, 0x01, 0x00, L"VGA" },

	{ 0x01, 0x00, 0x00, L"SCSI" },
	{ 0x01, 0x01, 0x00, L"IDE" },
	{ 0x01, 0x02, 0x00, L"Floppy" },
	{ 0x01, 0x03, 0x00, L"IPI" },
	{ 0x01, 0x04, 0x00, L"RAID" },
	{ 0x01, 0x80, 0x00, L"Other" },

	{ 0x02, 0x00, 0x00, L"Ethernet" },
	{ 0x02, 0x01, 0x00, L"Token Ring" },
	{ 0x02, 0x02, 0x00, L"FDDI" },
	{ 0x02, 0x03, 0x00, L"ATM" },
	{ 0x02, 0x04, 0x00, L"ISDN" },
	{ 0x02, 0x80, 0x00, L"Other" },

	{ 0x03, 0x00, 0x00, L"VGA" },
	{ 0x03, 0x00, 0x01, L"VGA+8514" },
	{ 0x03, 0x01, 0x00, L"XGA" },
	{ 0x03, 0x02, 0x00, L"3D" },
	{ 0x03, 0x80, 0x00, L"Other" },

	{ 0x04, 0x00, 0x00, L"Video" },
	{ 0x04, 0x01, 0x00, L"Audio" },
	{ 0x04, 0x02, 0x00, L"Telephony" },
	{ 0x04, 0x80, 0x00, L"Other" },

	{ 0x05, 0x00, 0x00, L"RAM" },
	{ 0x05, 0x01, 0x00, L"Flash" },
	{ 0x05, 0x80, 0x00, L"Other" },

	{ 0x06, 0x00, 0x00, L"PCI to HOST" },
	{ 0x06, 0x01, 0x00, L"PCI to ISA" },
	{ 0x06, 0x02, 0x00, L"PCI to EISA" },
	{ 0x06, 0x03, 0x00, L"PCI to MCA" },
	{ 0x06, 0x04, 0x00, L"PCI to PCI" },
	{ 0x06, 0x04, 0x01, L"PCI to PCI (Subtractive Decode)" },
	{ 0x06, 0x05, 0x00, L"PCI to PCMCIA" },
	{ 0x06, 0x06, 0x00, L"PCI to NuBUS" },
	{ 0x06, 0x07, 0x00, L"PCI to Cardbus" },
	{ 0x06, 0x08, 0x00, L"PCI to RACEway" },
	{ 0x06, 0x09, 0x00, L"PCI to PCI" },
	{ 0x06, 0x0A, 0x00, L"PCI to InfiBand" },
	{ 0x06, 0x80, 0x00, L"PCI to Other" },

	{ 0x07, 0x00, 0x00, L"Serial" },
	{ 0x07, 0x00, 0x01, L"Serial - 16450" },
	{ 0x07, 0x00, 0x02, L"Serial - 16550" },
	{ 0x07, 0x00, 0x03, L"Serial - 16650" },
	{ 0x07, 0x00, 0x04, L"Serial - 16750" },
	{ 0x07, 0x00, 0x05, L"Serial - 16850" },
	{ 0x07, 0x00, 0x06, L"Serial - 16950" },
	{ 0x07, 0x01, 0x00, L"Parallel" },
	{ 0x07, 0x01, 0x01, L"Parallel - BiDir" },
	{ 0x07, 0x01, 0x02, L"Parallel - ECP" },
	{ 0x07, 0x01, 0x03, L"Parallel - IEEE1284" },
	{ 0x07, 0x01, 0xFE, L"Parallel - IEEE1284 Target" },
	{ 0x07, 0x02, 0x00, L"Multiport Serial" },
	{ 0x07, 0x03, 0x00, L"Hayes Compatible Modem" },
	{ 0x07, 0x03, 0x01, L"Hayes Compatible Modem, 16450" },
	{ 0x07, 0x03, 0x02, L"Hayes Compatible Modem, 16550" },
	{ 0x07, 0x03, 0x03, L"Hayes Compatible Modem, 16650" },
	{ 0x07, 0x03, 0x04, L"Hayes Compatible Modem, 16750" },
	{ 0x07, 0x80, 0x00, L"Other" },

	{ 0x08, 0x00, 0x00, L"PIC" },
	{ 0x08, 0x00, 0x01, L"ISA PIC" },
	{ 0x08, 0x00, 0x02, L"EISA PIC" },
	{ 0x08, 0x00, 0x10, L"I/O APIC" },
	{ 0x08, 0x00, 0x20, L"I/O(x) APIC" },
	{ 0x08, 0x01, 0x00, L"DMA" },
	{ 0x08, 0x01, 0x01, L"ISA DMA" },
	{ 0x08, 0x01, 0x02, L"EISA DMA" },
	{ 0x08, 0x02, 0x00, L"Timer" },
	{ 0x08, 0x02, 0x01, L"ISA Timer" },
	{ 0x08, 0x02, 0x02, L"EISA Timer" },
	{ 0x08, 0x03, 0x00, L"RTC" },
	{ 0x08, 0x03, 0x00, L"ISA RTC" },
	{ 0x08, 0x03, 0x00, L"Hot-Plug" },
	{ 0x08, 0x80, 0x00, L"Other" },

	{ 0x09, 0x00, 0x00, L"Keyboard" },
	{ 0x09, 0x01, 0x00, L"Pen" },
	{ 0x09, 0x02, 0x00, L"Mouse" },
	{ 0x09, 0x03, 0x00, L"Scanner" },
	{ 0x09, 0x04, 0x00, L"Game Port" },
	{ 0x09, 0x80, 0x00, L"Other" },

	{ 0x0a, 0x00, 0x00, L"Generic" },
	{ 0x0a, 0x80, 0x00, L"Other" },

	{ 0x0b, 0x00, 0x00, L"386" },
	{ 0x0b, 0x01, 0x00, L"486" },
	{ 0x0b, 0x02, 0x00, L"Pentium" },
	{ 0x0b, 0x03, 0x00, L"PentiumPro" },
	{ 0x0b, 0x10, 0x00, L"DEC Alpha" },
	{ 0x0b, 0x20, 0x00, L"PowerPC" },
	{ 0x0b, 0x30, 0x00, L"MIPS" },
	{ 0x0b, 0x40, 0x00, L"Coprocessor" },
	{ 0x0b, 0x80, 0x00, L"Other" },

	{ 0x0c, 0x00, 0x00, L"FireWire" },
	{ 0x0c, 0x00, 0x10, L"OHCI FireWire" },
	{ 0x0c, 0x01, 0x00, L"Access.bus" },
	{ 0x0c, 0x02, 0x00, L"SSA" },
	{ 0x0c, 0x03, 0x00, L"USB (UHCI)" },
	{ 0x0c, 0x03, 0x10, L"USB (OHCI)" },
	{ 0x0c, 0x03, 0x80, L"USB" },
	{ 0x0c, 0x03, 0xFE, L"USB Device" },
	{ 0x0c, 0x04, 0x00, L"Fiber" },
	{ 0x0c, 0x05, 0x00, L"SMBus Controller" },
	{ 0x0c, 0x06, 0x00, L"InfiniBand" },
	{ 0x0c, 0x80, 0x00, L"Other" },

	{ 0x0d, 0x00, 0x00, L"iRDA" },
	{ 0x0d, 0x01, 0x00, L"Consumer IR" },
	{ 0x0d, 0x10, 0x00, L"RF" },
	{ 0x0d, 0x80, 0x00, L"Other" },

	{ 0x0e, 0x00, 0x00, L"I2O" },
	{ 0x0e, 0x80, 0x00, L"Other" },

	{ 0x0f, 0x01, 0x00, L"TV" },
	{ 0x0f, 0x02, 0x00, L"Audio" },
	{ 0x0f, 0x03, 0x00, L"Voice" },
	{ 0x0f, 0x04, 0x00, L"Data" },
	{ 0x0f, 0x80, 0x00, L"Other" },

	{ 0x10, 0x00, 0x00, L"Network" },
	{ 0x10, 0x10, 0x00, L"Entertainment" },
	{ 0x10, 0x80, 0x00, L"Other" },

	{ 0x11, 0x00, 0x00, L"DPIO Modules" },
	{ 0x11, 0x01, 0x00, L"Performance Counters" },
	{ 0x11, 0x10, 0x00, L"Comm Sync, Time+Frequency Measurement" },
	{ 0x11, 0x80, 0x00, L"Other" },
};

dword pciRead(int bus, int dev, int func, int reg, int bytes)
{
	word base;
	
	union {
		confadd c;
		dword n;
	} u;
	
	u.n = 0;
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

void pciWrite(int bus, int dev, int func, int reg, dword v, int bytes)
{
	word base;
	
	union {
		confadd c;
		dword n;
	} u;
	
	u.n = 0;
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

bool pciProbe(int bus, int dev, int func, pci_cfg_t *cfg)
{
	dword *word = (dword *) cfg;
	dword v;	
	int i;
	for(i=0;i<4;i++){
		word[i] = pciRead(bus,dev,func,4*i,4);
	}
	if(cfg->vendor_id == 0xffff) return false;

	cfg->bus = bus;
	cfg->dev = dev;
	cfg->func = func;
	cfg->subsys_vendor = pciRead(bus, dev, func, 0x2c, 2);
	cfg->subsys = pciRead(bus, dev, func, 0x2e, 2);

#if 0
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
			v = pciRead(bus,dev,func,i*4 + 0x10, 4);
			if(v) {
				int v2;
				pciWrite(bus,dev,func,i*4 + 0x10, 0xffffffff, 4);
				v2 = pciRead(bus,dev,func,i*4+0x10, 4) & 0xfffffff0;
				pciWrite(bus,dev,func,i*4 + 0x10, v, 4);
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
		v = pciRead(bus,dev,func,0x3c,1);
		cfg->irq = (v == 0xff ? 0 : v);
			
		//wprintf(L"  * Interrupt Line: %X\n",cfg->irq);
		break;
	case 1:
		//wprintf(L"  * PCI <-> PCI Bridge\n");
		break;
	default:
		//wprintf(L"  * Unknown Header Type\n");
	}
	return true;	
}

bool STDCALL INIT_CODE drvInit(driver_t *drv)
{
	word bus, dev, func;
	device_t *pci;
	wchar_t name[20];
	device_config_t* cfg;
	pci_cfg_t pcfg;
	int i, j;

	pci = hndAlloc(sizeof(device_t), NULL);
	pci->driver = drv;
	pci->req_first = pci->req_last = NULL;
	pci->request = NULL;
	devRegister(L"pci", pci, NULL);

	for (bus = 0; bus < 2/*255*/; bus++)
	{
		wprintf(L"PCI scan: %d%%\r", (bus * 100) / 255);

		for (dev = 0; dev < 32; dev++)
		{
			for (func = 0; func < 8; func++)
			{
				if (pciProbe(bus, dev, func, &pcfg))
				{
					swprintf(name, L"pci:%d:%d:%d", bus, dev, func);
					//wcscpy(name, L"pcidev");
					
					cfg = hndAlloc(sizeof(device_config_t), NULL);
					cfg->parent = pci;
					cfg->vendor_id = pcfg.vendor_id;
					cfg->device_id = pcfg.device_id;
					cfg->subsystem = pcfg.subsys_vendor << 16 | pcfg.subsys;
					cfg->pci_bus = bus;
					cfg->pci_dev = dev;
					cfg->pci_func = func;

					if (pcfg.irq)
						cfg->num_resources = 1;
					else
						cfg->num_resources = 0;

					for (i = 0; i < countof(pcfg.base); i++)
						if (pcfg.base[i])
							cfg->num_resources++;

					cfg->resources = hndAlloc(sizeof(device_resource_t) * 
						cfg->num_resources, NULL);
					for (i = 0, j = 0; i < countof(pcfg.base); i++)
					{
						if (pcfg.base[i])
						{
							if ((pcfg.base[i] & 0xffff0000) == 0)
							{
								cfg->resources[j].cls = dresIo;
								cfg->resources[j].u.io.base = pcfg.base[i];
								cfg->resources[j].u.io.length = pcfg.size[i];
							}
							else
							{
								cfg->resources[j].cls = dresMemory;
								cfg->resources[j].u.memory.base = pcfg.base[i];
								cfg->resources[j].u.memory.length = pcfg.size[i];
							}

							j++;
						}
					}

					if (pcfg.irq)
					{
						cfg->resources[j].cls = dresIrq;
						cfg->resources[j].u.irq = pcfg.irq;
					}
					
					for (i = 0; i < countof(classes); i++)
						if (pcfg.base_class == classes[i].base_class &&
							pcfg.sub_class == classes[i].sub_class &&
							pcfg.interface == classes[i].interface)
						{
							_cputws(classes[i].name);
							putwchar(' ');
							break;
						}

					devRegister(name, NULL, cfg);
				}
			}
		}
	}

	wprintf(L"PCI: finished\n");
	return true;
}

//@}