/* $Id: arch.c,v 1.20 2002/06/22 17:20:06 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/multiboot.h>

#include <stdio.h>

#include "wrappers.h"

void *sbrk_virtual(size_t diff);

extern char scode[], sbss[], _data_end__[];
extern thread_info_t idle_thread_info;
extern uint16_t con_attribs;
/*extern thread_t thr_idle;*/
extern uint8_t arch_df_stack[];

descriptor_t arch_gdt[11];
gdtr_t arch_gdtr;
descriptor_int_t arch_idt[_countof(_wrappers)];
idtr_t arch_idtr;
tss_t arch_tss;
struct tss0_t arch_df_tss;

#ifdef __SMP__
static uint32_t cpu_apic_id[MAX_CPU], 
    cpu_os_id[MAX_CPU], 
    cpu_apic_version[MAX_CPU];
static uint8_t *apic, *ioapic;
static addr_t apic_phys, ioapic_phys;
static const wchar_t *cpu_family[] = { L"", L"", L"", L"", L"Intel 486",
    L"Intel Pentium", L"Intel Pentium Pro", L"Intel Pentium II" };
static unsigned bsp;
static volatile bool i386_ap_ready;
#endif

void i386PitInit(unsigned hz);
void i386PicInit(uint8_t master_vector, uint8_t slave_vector);

void ArchStartup(multiboot_header_t *header, multiboot_info_t *info)
{
    descriptor_t *gdt = MANGLE_PTR(descriptor_t*, arch_gdt);
    gdtr_t *gdtr = MANGLE_PTR(gdtr_t*, &arch_gdtr);
    kernel_startup_t *startup = MANGLE_PTR(kernel_startup_t*, &kernel_startup);

    startup->memory_size = 0x100000 + info->mem_upper * 1024;
    startup->kernel_phys = KERNEL_PHYS;
    startup->kernel_size = sbss - scode;
    startup->multiboot_info = DEMANGLE_PTR(multiboot_info_t*, info);
    info->mods_addr = DEMANGLE_PTR(uint32_t, info->mods_addr);
    info->mmap_addr = DEMANGLE_PTR(uint32_t, info->mmap_addr);

    i386SetDescriptor(gdt + 0, 0, 0, 0, 0);
    /* Based kernel code = 0x8 */
    i386SetDescriptor(gdt + 1, startup->kernel_phys - (addr_t) scode, 
        0xfffff, ACS_CODE | ACS_DPL_0, ATTR_GRANULARITY | ATTR_DEFAULT);
    /* Based kernel data/stack = 0x10 */
    i386SetDescriptor(gdt + 2, startup->kernel_phys - (addr_t) scode, 
        0xfffff, ACS_DATA | ACS_DPL_0, ATTR_GRANULARITY | ATTR_BIG);
    /* Flat kernel code = 0x18 */
    i386SetDescriptor(gdt + 3, 0, 
        0xfffff, ACS_CODE | ACS_DPL_0, ATTR_DEFAULT | ATTR_GRANULARITY);
    /* Flat kernel data = 0x20 */
    i386SetDescriptor(gdt + 4, 0, 
        0xfffff, ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
    /* Flat user code = 0x28 */
    i386SetDescriptor(gdt + 5, 0, 
        0xfffff, ACS_CODE | ACS_DPL_3, ATTR_GRANULARITY | ATTR_DEFAULT);
    /* Flat user data = 0x30 */
    i386SetDescriptor(gdt + 6, 0, 
        0xfffff, ACS_DATA | ACS_DPL_3, ATTR_GRANULARITY | ATTR_BIG);
    /* TSS = 0x38 */
    i386SetDescriptor(gdt + 7, 0, 0, 0, 0);
    /* Thread info struct = 0x40 */
    i386SetDescriptor(gdt + 8, (addr_t) &ThrGetCpu(0)->thr_idle_info,/*(addr_t) &idle_thread_info, */
        sizeof(thread_info_t) - 1, ACS_DATA | ACS_DPL_3, ATTR_BIG);
    /* Double fault TSS = 0x48 */
    i386SetDescriptor(gdt + 9, 
        (addr_t) &arch_df_tss, 
        sizeof(arch_df_tss) - 1, 
        ACS_TSS, 
        ATTR_BIG);
    
    gdtr->base = (addr_t) gdt/* + startup->kernel_phys*/;
    gdtr->limit = sizeof(arch_gdt) - 1;
    __asm__("lgdt (%0)" : : "r" (gdtr));

    __asm__("mov %0,%%ds\n"
        "mov %0,%%ss\n" : : "r" (KERNEL_BASED_DATA));

    __asm__("mov %0,%%es\n"
        "mov %0,%%gs\n"
        "mov %0,%%fs" : : "r" (KERNEL_FLAT_DATA));
    __asm__("add %0, %%esp" : : "g" (scode - KERNEL_PHYS) : "esp");
    
    __asm__("ljmp %0,$_KernelMain"
        :
        : "i" (KERNEL_BASED_CODE));
}

static void i386CpuId(uint32_t level, cpuid_info *cpuid)
{
    __asm__("cpuid\n"
        : "=a" (cpuid->regs.eax), "=b" (cpuid->regs.ebx),
            "=d" (cpuid->regs.edx), "=c" (cpuid->regs.ecx)
        : "a" (level));
}

#ifdef __SMP__

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

mp_flt_ptr *i386MpProbe(void *base, void *limit)
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
        wprintf(L"i386MpProbe: fails on config checksum\n");
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
            wprintf(L"i386MpProbe: fails on MPC checksum\n");
            return NULL;
        }
    }
    else
    {
        wprintf(L"i386MpProbe: no MPC structure\n");
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
    extern char i386MpBspInit[], i386MpBspInitData[], i386MpBspInitEnd[];

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
    code_size = i386MpBspInitData - i386MpBspInit;
    if (i == 0)
    {
        uint32_t cr3;
        memcpy(code, i386MpBspInit, i386MpBspInitEnd - i386MpBspInit);
        __asm__("mov %%cr3, %0" : "=r" (cr3));
        ((uint32_t*) (code + code_size))[0] = cr3;
    }
    else
        memcpy(code, i386MpBspInit, code_size);

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

#else

unsigned ArchThisCpu(void)
{
    return 0;
}

#endif /* __SMP__ */

/*!
 *    \brief    Initializes the i386 features of the kernel
 *
 *    This function is called just after \p MemInit (so paging is enabled) and
 *    performs the following tasks:
 *    - Rebases GDTR now that paging is enabled
 *    - Sets up the interrupt handlers
 *    - Installs the double fault task gate
 *    - Sets up the user-to-kernel transition TSS
 *    - Sets up the double fault TSS
 *    - Reprograms the PIT to run at 100Hz
 *    - Reprograms the PIC to put IRQs where we want them
 *    - Enables interrupts
 *    - Clears the screen
 *    - Performs a CPUID
 *    - Initializes the \p gdb remote debugging stubs
 *
 *    \return    \p true
 */
bool ArchInit(void)
{
    unsigned i, cfg;
    cpuid_info cpuid1, cpuid2;
#ifdef __SMP__
    mp_flt_ptr *config;
#endif

    arch_gdtr.base = (addr_t) arch_gdt;
    arch_gdtr.limit = sizeof(arch_gdt) - 1;
    __asm__("lgdt %0" : : "m" (arch_gdtr));

    for (i = 0; i < _countof(_wrappers); i++)
        i386SetDescriptorInt(arch_idt + i, 
            KERNEL_FLAT_CODE, 
            (uint32_t) _wrappers[i], 
            ACS_INT | ACS_DPL_3, 
            ATTR_GRANULARITY | ATTR_BIG);
    
    /* Double fault task gate */
    i386SetDescriptorInt(arch_idt + 8, 
        KERNEL_DF_TSS,
        0,
        ACS_TASK | ACS_DPL_0, 
        0);

    /*
     * The value of tss.esp makes no difference until the first task switch is made:
     * - initially, CPU is in ring 0
     * - first timer IRQ occurs -- processor executes timer interrupt
     * - no PL switch so tss.esp0 is ignored
     * - current thread's esp is ignored
     * - when a task switch ocurs, tss.esp0 is updated
     */
    i386SetDescriptor(arch_gdt + 7, (addr_t) &arch_tss, sizeof(arch_tss), 
        ACS_TSS, ATTR_BIG);
    arch_tss.io_map_addr = offsetof(tss_t, io_map);
    arch_tss.ss0 = KERNEL_FLAT_DATA;
    arch_tss.esp0 = ThrGetCpu(0)->thr_idle.kernel_esp;

    arch_df_tss.io_map_addr = sizeof(arch_df_tss);
    arch_df_tss.ss = KERNEL_FLAT_DATA;
    arch_df_tss.esp = (uint32_t) arch_df_stack + PAGE_SIZE;
    arch_df_tss.cs = KERNEL_FLAT_CODE;
    arch_df_tss.eip = (uint32_t) exception_8;
    arch_df_tss.ds = 
        arch_df_tss.es =
        arch_df_tss.fs = 
        arch_df_tss.gs = KERNEL_FLAT_DATA;

    __asm__("mov %%cr3, %0"
        : "=r" (arch_df_tss.cr3));

    __asm__("ltr %0"
        :
        : "r" ((uint16_t) KERNEL_TSS));

    arch_idtr.base = (addr_t) arch_idt;
    arch_idtr.limit = sizeof(arch_idt) - 1;
    __asm__("lidt %0"
        :
        : "m" (arch_idtr));

#ifdef __SMP__
    config = i386MpProbe(PHYSICAL(0x9fc00), PHYSICAL(0xa0000));
    if (config == NULL)
        config = i386MpProbe(PHYSICAL(0xf0000), PHYSICAL(0x100000));
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
#else
    kernel_startup.num_cpus = 1;
#endif

    ThrInit();
    i386PitInit(100);
    i386PicInit(0x20, 0x28);
    enable();

    wprintf(L"ArchInit: kernel is %uKB (%uKB code)\n",
        kernel_startup.kernel_data / 1024, (_data_end__ - scode) / 1024);

    i386CpuId(0, &cpuid1);
    if (cpuid1.eax_0.max_eax > 0)
        i386CpuId(1, &cpuid2);
    else
        memset(&cpuid2, 0, sizeof(cpuid2));

    wprintf(L"Detected %u CPU%s: BSP CPU is %.12S, %u:%u:%u\n",
        kernel_startup.num_cpus,
        kernel_startup.num_cpus == 1 ? L"" : L"s",
        cpuid1.eax_0.vendorid, 
        cpuid2.eax_1.stepping, cpuid2.eax_1.model, cpuid2.eax_1.family);

#ifdef __SMP__
    if (kernel_startup.num_cpus > 1)
    {
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
#endif

    return true;
}

void ArchSetupContext(thread_t *thr, addr_t entry, bool isKernel, 
                      bool useParam, addr_t param, addr_t stack)
{
    context_t *ctx;
    
    thr->kernel_esp = 
        (addr_t) thr->kernel_stack + PAGE_SIZE * 2 - sizeof(context_t);
    thr->user_stack_top = stack;

    ctx = ThrGetContext(thr);
    ctx->kernel_esp = thr->kernel_esp;
    /*ctx->ctx_prev = NULL;*/
    memset(&ctx->regs, 0, sizeof(ctx->regs));
    ctx->regs.eax = 0xaaaaaaaa;
    ctx->regs.ebx = 0xbbbbbbbb;
    ctx->regs.ecx = 0xcccccccc;
    ctx->regs.edx = 0xdddddddd;

    if (isKernel)
    {
        ctx->cs = KERNEL_FLAT_CODE;
        ctx->ds = ctx->es = ctx->gs = ctx->ss = KERNEL_FLAT_DATA;
        ctx->fs = USER_THREAD_INFO;
        ctx->esp = thr->kernel_esp + sizeof(context_t);
    }
    else
    {
        ctx->cs = USER_FLAT_CODE | 3;
        ctx->ds = ctx->es = ctx->gs = ctx->ss = USER_FLAT_DATA | 3;
        ctx->fs = USER_THREAD_INFO | 3;
        ctx->esp = thr->user_stack_top;
    }

#if 0
    /* xxx - this doesn't work */
    if (useParam)
    {
        /* parameter */
        /*ctx->esp -= 4;*/
        *(addr_t*) ctx->esp = param;
        
        /* return address */
        ctx->esp -= 4;
        *(addr_t*) ctx->esp = 0;
    }
#endif

    ctx->eflags = EFLAG_IF | 2;
    ctx->eip = entry;
    ctx->error = (uint32_t) -1;
    ctx->intr = 1;
}

void ArchProcessorIdle(void)
{
    __asm__("hlt");
}

void ArchMaskIrq(uint16_t enable, uint16_t disable)
{
    /*uint16_t mask;
    mask = (uint16_t) ~in(PORT_8259M + 1) 
        | (uint16_t) ~in(PORT_8259S + 1) << 8;
    mask = (mask | enable) & ~disable;
    out(PORT_8259M + 1, ~enable & 0xff);
    out(PORT_8259S + 1, (~enable & 0xff) >> 8);*/
}

void MtxAcquire(semaphore_t *sem)
{
    if (sem->owner != NULL &&
        sem->owner != current())
    {
        while (sem->locks)
            ArchProcessorIdle();
    }

    sem->owner = current();
    sem->locks++;
}

void MtxRelease(semaphore_t *sem)
{
    assert(sem->locks > 0);
    assert(sem->owner == current());

    sem->locks--;
    if (sem->locks == 0)
        sem->owner = NULL;
}

void ArchDbgBreak(void)
{
    __asm__("int3");
}

void ArchDbgDumpContext(const context_t* ctx)
{
    context_v86_t *v86;
    v86 = (context_v86_t*) ctx;
    while (ctx != NULL)
    {
        wprintf(L"---- Context at %p: ----\n", ctx);
        wprintf(L"  EAX %08x\t  EBX %08x\t  ECX %08x\t  EDX %08x\n",
            ctx->regs.eax, ctx->regs.ebx, ctx->regs.ecx, ctx->regs.edx);
        wprintf(L"  ESI %08x\t  EDI %08x\t  EBP %08x\n",
            ctx->regs.esi, ctx->regs.edi, ctx->regs.ebp);
        wprintf(L"   CS %08x\t   DS %08x\t   ES %08x\t   FS %08x\n",
            ctx->cs, ctx->ds, ctx->es, ctx->fs);
        wprintf(L"   GS %08x\t   SS %08x\n",
            ctx->gs, ctx->ss);
        wprintf(L"  EIP %08x\t eflg %08x\t ESP0 %08x\t ESP3 %08x\n", 
            ctx->eip, ctx->eflags, ctx->kernel_esp, ctx->esp);

        if (ctx->eflags & EFLAG_VM)
            wprintf(L"V86DS %08x\tV86ES %08x\tV86FS %08x\tV86GS %08x\n",
                v86->v86_ds, v86->v86_es, v86->v86_fs, v86->v86_gs);
        /*wprintf(L"Previous context: %p\n", ctx->ctx_prev);*/

        /*ArchProcessorIdle();*/
        ctx = ctx->ctx_prev;
    }

    if (ctx != NULL)
        wprintf(L"---- Next context at %p ----\n", ctx);
}

/*!
 *    \brief    Switches to a new execution context and address space
 *
 *    Performs the following tasks:
 *    - Switches address spaces (if necessary)
 *    - Updates the \p ESP0 field in the TSS
 *    - Updates the \p kernel_esp field in the thread's context structure
 *    - Switches the \p FS base address to the new thread's information structure
 *    - Runs any APCs queued on the new thread
 *
 *    If any APCs were run then the thread is paused and a reschedule will need 
 *    to take place (in which case \p ArchAttachToThread will return \p false ).
 *
 *    \param    thr    New thread to switch to
 *    \param    isNewAddressSpace    If \p true , then \p ArchAttachToThread will
 *        switch address spaces as well (thereby invoking a TLB flush)
 *    \return    \p true if the switch went ahead correctly, or \p false if 
 *        reschedule should occur.
 */
bool ArchAttachToThread(thread_t *thr, bool isNewAddressSpace)
{
    context_t *new;
    thread_apc_t *a, *next;
    cpu_t *c;
    
    c = cpu();
    c->current_thread = thr;
    if (isNewAddressSpace)
        __asm__("mov %0, %%cr3"
            :
            : "r" (thr->process->page_dir_phys));
    
    new = ThrGetContext(thr);

    if (new->eflags & EFLAG_VM)
        arch_tss.esp0 = (uint32_t) ((context_v86_t*) new + 1);
    else
        arch_tss.esp0 = (uint32_t) (new + 1);

    new->kernel_esp = thr->kernel_esp;

    i386SetDescriptor(arch_gdt + 8, 
        (uint32_t) thr->info, 
        sizeof(thread_info_t) - 1, 
        ACS_DATA | ACS_DPL_3, 
        0);

    if (thr->apc_first)
    {
        for (a = thr->apc_first; a; a = next)
        {
            next = a->next;
            LIST_REMOVE(thr->apc, a);
            a->fn(a->param);
            free(a);
            ThrPause(c->current_thread);
        }

        return false;
    }

    return true;
}

void ArchReboot(void)
{
    uint8_t temp;
    disable();

    /* flush the keyboard controller */
    do
    {
        temp = in(0x64);
        if (temp & 1)
            in(0x60);
    } while(temp & 2);

    /* pulse the CPU reset line */
    out(0x64, 0xFE);

    ArchProcessorIdle();
}

extern uint8_t i386PowerOff[], i386PowerOffEnd[];

static void ArchPowerOffThread(void)
{
    uint16_t *code, *stack, *stack_top;
    FARPTR fpcode, fpstack_top;
    handle_t thr;
    
    code = VmmAlloc(1, NULL, 3 | MEM_READ | MEM_WRITE);
    stack = VmmAlloc(1, NULL, 3 | MEM_READ | MEM_WRITE);
    stack_top = stack + 0x800;
    memcpy(code, i386PowerOff, i386PowerOffEnd - i386PowerOff);
    fpcode = MK_FP((uint16_t) ((addr_t) code >> 4), 0);
    fpstack_top = MK_FP((uint16_t) ((addr_t) stack >> 4), 0x1000);
    wprintf(L"ArchPowerOffThread: code = %p (%04x:%04x), stack = %p (%04x:%04x)\n",
        code, FP_SEG(fpcode), FP_OFF(fpcode),
        stack_top, FP_SEG(fpstack_top), FP_OFF(fpstack_top));
    thr = ThrCreateV86Thread(fpcode,
        fpstack_top,
        16,
        NULL);
    ThrWaitHandle(current(), thr, 0);
    KeYield();
    ThrExitThread(0);
    KeYield();
}

void ArchPowerOff(void)
{
    thread_t *thr;
    handle_t hnd;
    thr = ThrCreateThread(&proc_idle, true, ArchPowerOffThread, false, NULL, 16);
    hnd = HndDuplicate(current()->process, &thr->hdr);
    ThrWaitHandle(current(), hnd, 0);
    KeYield();
    /* xxx -- usual HndClose problem with exited threads */
    /*HndClose(current()->process, hnd, 0);*/
}

static unsigned short ctc(void)
{
    union
    {
        unsigned char byte[2];
        signed short word;
    } ret_val;

    semaphore_t sem = { 0 };

    SemAcquire(&sem);
    disable();
    out(0x43, 0x00);               /* latch channel 0 value */
    ret_val.byte[0] = in(0x40);    /* read LSB */
    ret_val.byte[1] = in(0x40);    /* read MSB */
    SemRelease(&sem);
    return -ret_val.word;          /* convert down count to up count */
}

void ArchMicroDelay(unsigned microseconds)
{
    unsigned short curr, prev;
    union
    {
        unsigned short word[2];
        unsigned long dword;
    } end;

    prev = ctc();
    end.dword = prev + ((unsigned long)microseconds * 500) / 419;
    for (;;)
    {
        curr = ctc();
        if (end.word[1] == 0)
        {
            if (curr >= end.word[0])
                break;
        }
        if (curr < prev)                /* timer overflowed */
        {
            if (end.word[1] == 0)
                break;
            (end.word[1])--;
        }
        prev = curr;
    }
}
