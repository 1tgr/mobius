#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>
#include <kernel/profile.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

/*!
*  \ingroup	drivers
*  \defgroup	 pci PCI bus
*  @{
*/

typedef struct pci_cfg_t pci_cfg_t;
struct pci_cfg_t
{
    /* normal header stuff */
    uint16_t vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t revision_id;
    uint8_t interface;
    uint8_t sub_class;
    uint8_t base_class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;	

    /* device info */
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint8_t irq;

    /* base registers */
    uint32_t base[6];
    uint32_t size[6];

    uint16_t subsys_vendor;
    uint16_t subsys;
};

typedef struct confadd
{
    uint8_t reg:8;
    uint8_t func:3;
    uint8_t dev:5;
    uint8_t bus:8;
    uint8_t rsvd:7;
    uint8_t enable:1;
} confadd;

uint32_t pciRead(int bus, int dev, int func, int reg, int bytes)
{
    uint16_t base;

    union {
        confadd c;
        uint32_t n;
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

void pciWrite(int bus, int dev, int func, int reg, uint32_t v, int bytes)
{
    uint16_t base;

    union {
        confadd c;
        uint32_t n;
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
    case 1: out(base, (uint8_t) v); break;
    case 2: out16(base, (uint16_t) v); break;
    case 4: out32(base, v); break;
    }

}

uint32_t PciReadConfig(const pci_location_t *loc, unsigned reg, unsigned bytes)
{
    return pciRead(loc->bus, loc->dev, loc->func, reg, bytes);
}

void PciWriteConfig(const pci_location_t *loc, unsigned reg, uint32_t v, unsigned bytes)
{
    pciWrite(loc->bus, loc->dev, loc->func, reg, v, bytes);
}

bool pciProbe(int bus, int dev, int func, pci_cfg_t *cfg)
{
    uint32_t *uint16_t = (uint32_t *) cfg;
    uint32_t v;
    int i;
    for(i=0;i<4;i++){
        uint16_t[i] = pciRead(bus,dev,func,4*i,4);
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
                    /*wprintf(L"  * Base Register %d IO: %x (%x)\n",i,v&0xfff0,v2&0xffff);*/
                    cfg->base[i] = v & 0xffff;
                    cfg->size[i] = v2 & 0xffff;
                } else {
                    /*wprintf(L"  * Base Register %d MM: %x (%x)\n",i,v&0xfffffff0,v2);*/
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

        /*wprintf(L"  * Interrupt Line: %X\n",cfg->irq);*/
        break;
    case 1:
        /*wprintf(L"  * PCI <-> PCI Bridge\n");*/
        break;
    default:
        /*wprintf(L"  * Unknown Header Type\n");*/
    }
    return true;
}

bool DrvInit(driver_t *drv)
{
    uint16_t bus, dev, func, buslimit;
    device_t *pci;
    wchar_t name[20], key[50];
    const wchar_t *driver, *desc, *device;
    device_config_t* cfg;
    pci_cfg_t pcfg;
    int i, j;

    pci = malloc(sizeof(device_t));
    memset(pci, 0, sizeof(device_t));
    pci->driver = drv;
    DevAddDevice(pci, L"pci", NULL);

    buslimit = wcstol(ProGetString(drv->profile_key, L"BusLimit", L"2"), NULL, 0);
    if (buslimit == 0)
        buslimit = 1;
    for (bus = 0; bus < buslimit; bus++)
    {
        //wprintf(L"PCI scan: %d%%\r", (bus * 100) / 255);

        for (dev = 0; dev < 32; dev++)
        {
            for (func = 0; func < 8; func++)
            {
                if (pciProbe(bus, dev, func, &pcfg))
                {
                    cfg = malloc(sizeof(device_config_t));
                    cfg->parent = pci;
                    cfg->vendor_id = pcfg.vendor_id;
                    cfg->device_id = pcfg.device_id;
                    cfg->subsystem = pcfg.subsys_vendor << 16 | pcfg.subsys;
                    cfg->bus_type = DEV_BUS_PCI;
                    cfg->location.pci.bus = bus;
                    cfg->location.pci.dev = dev;
                    cfg->location.pci.func = func;
                    cfg->device_class = (pcfg.base_class << 8) | pcfg.sub_class;

                    swprintf(name, L"pci:%d:%d:%d", bus, dev, func);
                    swprintf(key, L"PCI/Vendor%04xDevice%04xSubsystem%08x", 
                        cfg->vendor_id, cfg->device_id, cfg->subsystem);
                    driver = ProGetString(key, L"Driver", NULL);
                    desc = ProGetString(key, L"Description", key);
                    device = ProGetString(key, L"Device", name);

                    if (pcfg.irq)
                        cfg->num_resources = 1;
                    else
                        cfg->num_resources = 0;

                    for (i = 0; i < _countof(pcfg.base); i++)
                        if (pcfg.base[i])
                            cfg->num_resources++;

                    cfg->resources = malloc(sizeof(device_resource_t) * cfg->num_resources);
                    for (i = 0, j = 0; i < _countof(pcfg.base); i++)
                    {
                        if (pcfg.base[i])
                        {
                            if ((pcfg.base[i] & 0xffff0000) == 0)
                            {
                                cfg->resources[j].cls = resIo;
                                cfg->resources[j].u.io.base = pcfg.base[i];
                                cfg->resources[j].u.io.length = pcfg.size[i];
                            }
                            else
                            {
                                cfg->resources[j].cls = resMemory;
                                cfg->resources[j].u.memory.base = pcfg.base[i];
                                cfg->resources[j].u.memory.length = pcfg.size[i];
                            }

                            j++;
                        }
                    }

                    if (pcfg.irq)
                    {
                        cfg->resources[j].cls = resIrq;
                        cfg->resources[j].u.irq = pcfg.irq;
                    }

                    wprintf(L"  == %s (%s) %s\n", device, driver, desc);
                    //if (driver == NULL)
                        //wprintf(L"PCI: %s => %s\n", name, desc);
                    //else
                        DevInstallDevice(driver, device, cfg, key);

                    if ((pcfg.header_type & 0x80) == 0 &&
                        func == 0)
                        /* single-function device */
                        break;
                }
            }
        }
    }

    wprintf(L"  ==\nPCI: finished\n");
    return true;
}

/*@}*/
