/* $Id: smp.c,v 1.1 2002/12/21 09:49:35 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/memory.h>
#include <stdio.h>

void *sbrk_virtual(size_t diff);

extern gdtr_t arch_gdtr;
extern idtr_t arch_idtr;

static uint32_t cpu_apic_id[MAX_CPU], 
    cpu_os_id[MAX_CPU], 
    cpu_apic_version[MAX_CPU];
static uint8_t *apic, *ioapic;
static addr_t apic_phys, ioapic_phys;
static const wchar_t *cpu_family[] = { L"", L"", L"", L"", L"Intel 486",
    L"Intel Pentium", L"Intel Pentium Pro", L"Intel Pentium II" };
static unsigned bsp;
static volatile bool i386_ap_ready;

#pragma pack(push, 2)

#define APIC_ID            ((unsigned int *) (apic + 0x020))    /* Local APIC ID Register */
#define APIC_VERSION       ((unsigned int *) (apic + 0x030))    /* Local APIC Version Register */
#define APIC_TPRI          ((unsigned int *) (apic + 0x080))    /* Task Priority Register */
#define APIC_APR           ((unsigned int *) (apic + 0x090))    /* Arbitration Priority Register */
#define APIC_PPR           ((unsigned int *) (apic + 0x0a0))    /* Processor Priority Register */
#define APIC_EOI           ((unsigned int *) (apic + 0x0b0))    /* EOI Register */
#define APIC_SIVR          ((unsigned int *) (apic + 0x0f0))    /* Spuriour Interrupt Vector Register */
#define APIC_ESR           ((unsigned int *) (apic + 0x280))    /* Error Status Register */
#define APIC_ICR1          ((unsigned int *) (apic + 0x300))    /* Interrupt Command Reg. 0-31 */
#define APIC_ICR2          ((unsigned int *) (apic + 0x310))    /* Interrupt Command Reg. 32-63 */
#define APIC_LVTT          ((unsigned int *) (apic + 0x320))    /* Local Vector Table (Timer) */
#define APIC_LPC           ((unsigned int *) (apic + 0x0b0))    /* Performance Counter LVT */
#define APIC_LINT0         ((unsigned int *) (apic + 0x350))    /* Local Vector Table (LINT0) */
#define APIC_LINT1         ((unsigned int *) (apic + 0x350))    /* Local Vector Table (LINT1) */
#define APIC_LVT3          ((unsigned int *) (apic + 0x370))    /* Local Vector Table (Error) */
#define APIC_ICRT          ((unsigned int *) (apic + 0x380))    /* Initial Count Register for Timer */
#define APIC_CCRT          ((unsigned int *) (apic + 0x390))    /* Current Count Register for Timer */
#define APIC_TDCR          ((unsigned int *) (apic + 0x3e0))    /* Timer Divide Configuration Register */

#define APIC_TDCR_2        0x00
#define APIC_TDCR_4        0x01
#define APIC_TDCR_8        0x02
#define APIC_TDCR_16       0x03
#define APIC_TDCR_32       0x08
#define APIC_TDCR_64       0x09
#define APIC_TDCR_128      0x0a
#define APIC_TDCR_1        0x0b

#define APIC_LVTT_VECTOR   0x000000ff
#define APIC_LVTT_DS       0x00001000
#define APIC_LVTT_M        0x00010000
#define APIC_LVTT_TM       0x00020000

#define APIC_LVT_DM        0x00000700
#define APIC_LVT_IIPP      0x00002000
#define APIC_LVT_TM        0x00008000
#define APIC_LVT_M         0x00010000
#define APIC_LVT_OS        0x00020000

#define APIC_TPR_PRIO      0x000000ff
#define APIC_TPR_INT       0x000000f0
#define APIC_TPR_SUB       0x0000000f

#define APIC_SVR_SWEN      0x00000100
#define APIC_SVR_FOCUS     0x00000200

#define APIC_DEST_STARTUP  0x00600

#define LOPRIO_LEVEL       0x00000010

#define APIC_DEST_FIELD          (0)
#define APIC_DEST_SELF           (1 << 18)
#define APIC_DEST_ALL            (2 << 18)
#define APIC_DEST_ALL_BUT_SELF   (3 << 18)

#define IOAPIC_ID          0x0
#define IOAPIC_VERSION     0x1
#define IOAPIC_ARB         0x2
#define IOAPIC_REDIR_TABLE 0x10

#define IPI_CACHE_FLUSH    0x40
#define IPI_INV_TLB        0x41
#define IPI_INV_PTE        0x42
#define IPI_INV_RESCHED    0x43
#define IPI_STOP           0x44

typedef struct
{
    unsigned int signature;       /* "PCMP" */
    unsigned short table_len;     /* length of this structure */
    unsigned char mp_rev;         /* spec supported, 1 for 1.1 or 4 for 1.4 */
    unsigned char checksum;       /* checksum, all bytes add up to zero */
    char oem[8];                  /* oem identification, not null-terminated */
    char product[12];             /* product name, not null-terminated */
    void *oem_table_ptr;          /* addr of oem-defined table, zero if none */
    unsigned short oem_len;       /* length of oem table */
    unsigned short num_entries;   /* number of entries in base table */
    unsigned int apic;            /* address of apic */
    unsigned short ext_len;       /* length of extended section */
    unsigned char ext_checksum;   /* checksum of extended table entries */
    unsigned char reserved;       /* reserved */
} mp_config_table;

typedef struct
{
    unsigned int signature;       /* "_MP_" */
    mp_config_table *mpc;         /* address of mp configuration table */
    unsigned char mpc_len;        /* length of this structure in 16-byte units */
    unsigned char mp_rev;         /* spec supported, 1 for 1.1 or 4 for 1.4 */
    unsigned char checksum;       /* checksum, all bytes add up to zero */
    unsigned char mp_feature_1;   /* mp system configuration type if no mpc */
    unsigned char mp_feature_2;   /* imcrp */
    unsigned char mp_feature_3, mp_feature_4, mp_feature_5; /* reserved */
} mp_flt_ptr;

typedef struct
{
    unsigned char type;
    unsigned char apic_id;
    unsigned char apic_version;
    unsigned char cpu_flags;
    unsigned int signature;       /* stepping, model, family, each four bits */
    unsigned int feature_flags;
    unsigned int res1, res2;
} mp_ext_pe;

typedef struct
{
    unsigned char type;
    unsigned char ioapic_id;
    unsigned char ioapic_version;
    unsigned char ioapic_flags;
    uint32_t addr;
} mp_ext_ioapic;

#pragma pack(pop)

#define MP_FLT_SIGNATURE   (('_' << 24) | ('P' << 16) | ('M' << 8) | ('_'))
#define MP_CTH_SIGNATURE   (('P' << 24) | ('C' << 16) | ('M' << 8) | ('P'))

#define MP_DEF_LOCAL_APIC  0xfee00000 /* default local apic address */
#define MP_DEF_IO_APIC     0xfec00000 /* default i/o apic address */

#define APIC_DM_INIT       (5 << 8)
#define APIC_DM_STARTUP    (6 << 8)
#define APIC_LEVEL_TRIG    (1 << 14)
#define APIC_ASSERT        (1 << 13)

#define APIC_ENABLE        0x100
#define APIC_FOCUS         (~(1 << 9))
#define APIC_SIV           (0xff)

#define MP_EXT_PE          0
#define MP_EXT_BUS         1
#define MP_EXT_IO_APIC     2
#define MP_EXT_IO_INT      3
#define MP_EXT_LOCAL_INT   4

#define MP_EXT_PE_LEN          20
#define MP_EXT_BUS_LEN         8
#define MP_EXT_IO_APIC_LEN     8
#define MP_EXT_IO_INT_LEN      8
#define MP_EXT_LOCAL_INT_LEN   8

#define SET_APIC_DEST(x)       (x << 24)

mp_flt_ptr *i386MpFindConfig(void *base, void *limit)
{
    unsigned *ptr, i;
    mp_flt_ptr *config;
    uint8_t *b, total;
    mp_config_table *mpc;

    config = NULL;
    for (ptr = base; ptr < (unsigned*) limit; ptr++)
        if (*ptr == MP_FLT_SIGNATURE)
        {
            config = (mp_flt_ptr *) ptr;
            break;
        }

    if (config == NULL)
        return NULL;

    /* compute floating pointer structure checksum */
    b = (uint8_t*) config;
    for (i = total = 0; i < config->mpc_len * 16; i++)
        total += b[i];

    if (total != 0)
    {
        wprintf(L"i386MpFindConfig: fails on config checksum\n");
        return NULL;
    }

    /* compute mp configuration table checksum if we have one*/
    mpc = PHYSICAL(config->mpc);
    if (mpc != NULL)
    {
        b = (uint8_t*) mpc;
        for (i = total = 0; i < mpc->table_len; i++)
            total += b[i];

        if (total != 0)
        {
            wprintf(L"i386MpFindConfig: fails on MPC checksum\n");
            return NULL;
        }
    }
    else
    {
        wprintf(L"i386MpFindConfig: no MPC structure\n");
        return NULL;
    }

    return config;
}

void i386MpParseConfig(mp_flt_ptr *config)
{
    uint8_t *ptr;
    int i;
    mp_ext_pe *pe;
    mp_ext_ioapic *io;
    mp_config_table *mpc;

    /*
     * we are not running in standard configuration, so we have to look through
     * all of the mp configuration table crap to figure out how many processors
     * we have, where our apics are, etc.
     */
    kernel_startup.num_cpus = 0;

    mpc = PHYSICAL(config->mpc);
    /* print out our new found configuration. */
    ptr = (uint8_t*) &(mpc->oem[0]);
    wprintf(L"smp: oem id: %c%c%c%c%c%c%c%c product id: %c%c%c%c%c%c%c%c%c%c%c%c\n", 
        ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
        ptr[5], ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12],
        ptr[13], ptr[14], ptr[15], ptr[16], ptr[17], ptr[18], ptr[19],
        ptr[20]);
    wprintf(L"smp: base table has %d entries, extended section %d bytes\n",
        mpc->num_entries, mpc->ext_len);
    apic_phys = mpc->apic;

    ptr = (char *) (mpc + 1);
    for (i = 0; i < mpc->num_entries; i++)
        switch (*ptr)
        {
        case MP_EXT_PE:
            pe = (mp_ext_pe *) ptr;
            cpu_apic_id[kernel_startup.num_cpus] = pe->apic_id;
            cpu_os_id[pe->apic_id] = kernel_startup.num_cpus;
            cpu_apic_version[kernel_startup.num_cpus] = pe->apic_version;
            if (pe->cpu_flags & 0x2)
                bsp = kernel_startup.num_cpus;
            wprintf(L"smp: cpu#%d: %s, apic id %d, version %d%s\n",
                kernel_startup.num_cpus++, 
                cpu_family[(pe->signature & 0xf00) >> 8],
                pe->apic_id, 
                pe->apic_version, 
                (pe->cpu_flags & 0x2) ? L", BSP" : L"");
            ptr += 20;
            break;
        case MP_EXT_BUS:
            ptr += 8;
            break;
        case MP_EXT_IO_APIC:
            io = (mp_ext_ioapic *) ptr;
            ioapic_phys = io->addr;
            wprintf(L"smp: found io apic with apic id %d, version %d\n",
                io->ioapic_id, io->ioapic_version);
            ptr += 8;
            break;
        case MP_EXT_IO_INT:
            ptr += 8;
            break;
        case MP_EXT_LOCAL_INT:
            ptr += 8;
            break;
        }

    wprintf(L"smp: apic @ 0x%x, i/o apic @ 0x%x, total %d processors detected\n",
        apic_phys, ioapic_phys, kernel_startup.num_cpus);
}

static volatile unsigned int apic_read (unsigned int *addr)
{
    return *addr;
}

static void apic_write (unsigned int *addr, unsigned int data)
{
    *addr = data;
}

void i386MpStartAp(unsigned i)
{
    extern char i386MpApInit[], i386MpApInitData[], i386MpApInitEnd[];

    unsigned int j, config, num_startups;
    uint16_t *ptr;
    uint8_t *code;
    uint32_t code_size;

    /* set shutdown code and warm reset vector */
    out(0x70, 0x0f);
    out(0x71, 0x0a);
    ptr = PHYSICAL(0x467);
    *ptr = (0x1000 + 0x1000 * i) >> 16;
    ptr = PHYSICAL(0x469);
    *ptr = 0;

    code = PHYSICAL(0x1000 + 0x1000 * i);
    code_size = i386MpApInitData - i386MpApInit;
    if (i == 0)
    {
        uint32_t cr3;
        memcpy(code, i386MpApInit, i386MpApInitEnd - i386MpApInit);
        __asm__("mov %%cr3, %0" : "=r" (cr3));
        ((uint32_t*) (code + code_size))[0] = cr3;
    }
    else
        memcpy(code, i386MpApInit, code_size);

    ((uint32_t*) PHYSICAL(0x1000 + code_size))[1] = i;
    MemMap(0x1000 + 0x1000 * i, 
        0x1000 + 0x1000 * i, 
        0x1000 + 0x1000 * i + PAGE_SIZE, 
        PRIV_KERN | PRIV_WR | PRIV_RD | PRIV_PRES);

    i386_ap_ready = false;

    /* clear apic errors */
    if (cpu_apic_version[i] & 0xf0)
    {
        apic_write(APIC_ESR, 0);
        apic_read(APIC_ESR);
    }

    /* send (aka assert) INIT IPI */
    config = (apic_read(APIC_ICR2) & 0xf0ffffff) 
        | (cpu_apic_id[i] << 24);
    apic_write (APIC_ICR2, config); /* set target pe */
    config = (apic_read(APIC_ICR1) & 0xfff0f800) 
        | APIC_DM_INIT | APIC_LEVEL_TRIG | APIC_ASSERT;
    apic_write (APIC_ICR1, config);

    /* deassert INIT */
    config = (apic_read(APIC_ICR2) & 0xffffff) 
        | (cpu_apic_id[i] << 24);
    apic_write (APIC_ICR2, config);
    config = (apic_read(APIC_ICR1) & ~0xcdfff) 
        | APIC_LEVEL_TRIG | APIC_DM_INIT;

    /* wait 10ms */
    ArchMicroDelay(10000);

    /* is this a local apic or an 82489dx ? */
    num_startups = (cpu_apic_version[i] & 0xf0) ? 2 : 0;
    for (j = 0; j < num_startups; j++)
    {
        /* it's a local apic, so send STARTUP IPIs */
        apic_write(APIC_ESR, 0);

        /* set target pe */
        config = (apic_read(APIC_ICR2) & 0xf0ffffff) | (cpu_apic_id[i] << 24);
        apic_write (APIC_ICR2, config);

        /* send the IPI */
        config = (apic_read(APIC_ICR1) & 0xfff0f800) | APIC_DM_STARTUP |
            ((0x1000 + 0x1000 * i) >> 12);
        apic_write (APIC_ICR1, config);

        /* wait */
        ArchMicroDelay(200);
        while (apic_read (APIC_ICR1) & 0x1000)
            ;
    }

    while (!i386_ap_ready)
        ;
}

void i386MpApEntry(uint32_t id)
{
    unsigned cfg;

    __asm__("lgdt (%0)" : : "m" (arch_gdtr));

    __asm__("mov %0,%%ds\n"
        "mov %0,%%ss\n"
        "mov %0,%%es\n"
        "mov %0,%%gs\n"
        "mov %0,%%fs" : : "r" (KERNEL_FLAT_DATA));

    __asm__("lidt (%0)" : : "m" (arch_idtr));

    /* set up the local apic */
    cfg = (apic_read ((unsigned int *) APIC_SIVR) & APIC_FOCUS) | APIC_ENABLE |
        0xff; /* set spurious interrupt vector to 0xff */
    apic_write (APIC_SIVR, cfg);
    cfg = apic_read (APIC_TPRI) & 0xffffff00; /* accept all interrupts */
    apic_write (APIC_TPRI, cfg);

    apic_read (APIC_SIVR);
    apic_write (APIC_EOI, 0);

    cfg = apic_read (APIC_LVT3) & 0xffffff00; /* XXX - set vector */
    apic_write (APIC_LVT3, cfg);

    //apic_write (APIC_ICRT, 66 * 10000); /* 10 ms */
    //cfg = 0x20030;
    //apic_write (APIC_LVTT, cfg);

    enable();

    wprintf(L"Application processor %u on-line\n", id);
    i386_ap_ready = true;

    while (true)
        ArchProcessorIdle();
}

unsigned ArchThisCpu(void)
{
    if (kernel_startup.num_cpus == 1)
        return 0;
    else
        return cpu_os_id[(apic_read(APIC_ID) & 0xffffffff) >> 24];
}

void i386MpProbe(void)
{
    mp_flt_ptr *config;

    config = i386MpFindConfig(PHYSICAL(0x9fc00), PHYSICAL(0xa0000));
    if (config == NULL)
        config = i386MpFindConfig(PHYSICAL(0xf0000), PHYSICAL(0x100000));
    if (config == NULL)
        kernel_startup.num_cpus = 1;
    else
        i386MpParseConfig(config);

    if (kernel_startup.num_cpus > 1)
    {
        apic = sbrk_virtual(PAGE_SIZE);
        ioapic = sbrk_virtual(PAGE_SIZE);
        MemMap((addr_t) apic, apic_phys, (addr_t) apic + PAGE_SIZE, 
            PRIV_PRES | PRIV_KERN | PRIV_RD | PRIV_WR);
        MemMap((addr_t) ioapic, ioapic_phys, (addr_t) ioapic + PAGE_SIZE, 
            PRIV_PRES | PRIV_KERN | PRIV_RD | PRIV_WR);
    }
}

void i386MpInit(void)
{
    uint32_t cfg;
    unsigned i;

    cfg = (apic_read((unsigned int *) APIC_SIVR) & APIC_FOCUS) 
        | APIC_ENABLE | 0xff; /* set spurious interrupt vector to 0xff */
    apic_write(APIC_SIVR, cfg);
    cfg = apic_read (APIC_TPRI) & 0xffffff00; /* accept all interrupts */
    apic_write(APIC_TPRI, cfg);

    apic_read(APIC_SIVR);
    apic_write(APIC_EOI, 0);

    cfg = (apic_read(APIC_LVT3) & 0xffffff00) | 0xfe; /* XXX - set vector */
    apic_write(APIC_LVT3, cfg);

    for (i = 0; i < kernel_startup.num_cpus; i++)
        if (i != bsp)
            i386MpStartAp(i);
}
