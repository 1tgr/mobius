/*
 * $History: main.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:33
 * Updated in $/coreos/kernel
 * Tidied up
 * Added history block
 */

/*!
 *    \defgroup    kernel    Kernel
 */

#include <kernel/kernel.h>
#include <kernel/init.h>
#include <kernel/proc.h>
#include <kernel/vmm.h>
#include <kernel/sched.h>
#include <kernel/profile.h>
#include <kernel/thread.h>

#include <stdio.h>
#include <wchar.h>
#include <string.h>

kernel_startup_t kernel_startup;

void SpinInit(spinlock_t *sem)
{
    sem->locks = 0;
    sem->owner = NULL;
}


static void KeStartupThread(void)
{
    process_info_t info;

    DevInit();
    FsInit();

    proc_idle.root = fs_root;
    wcscpy(proc_idle.info->cwd, L"/");
    FsChangeDir(L"/");

    if (!ProLoadProfile(SYS_BOOT L"/system.pro", L"/"))
        wprintf(L"KernelMain: unable to load " SYS_BOOT L"/system.pro\n");

    ThrCreateThread(&proc_idle, true, MemZeroPageThread, false, NULL, 24, L"MemZeroPageThread");

    info = *proc_idle.info;
    wcscpy(info.cmdline, kernel_startup.cmdline);
    ProcSpawnProcess(SYS_BOOT L"/monitor.exe", &info);

    ThrExitThread(0);
    KeYield();
}


static void *KeMapPhysicalRange(addr_t low, addr_t high, bool as_normal)
{
    return VmmMap((PAGE_ALIGN_UP(high) - PAGE_ALIGN(low)) / PAGE_SIZE,
        low,
        (void*) low,
        NULL,
        as_normal ? VM_AREA_NORMAL : VM_AREA_MAP,
        (as_normal ? VM_MEM_LITERAL : 0) | VM_MEM_READ | VM_MEM_WRITE | VM_MEM_USER);
}


void KernelMain(void)
{
    void *text;

    ArchInit();
    MemInit();
    ProcInit();

    MemMapRange(0, 0, (void*) 0x00100000, 0);
           KeMapPhysicalRange(0x00000000, 0x00001000, false);
           KeMapPhysicalRange(0x00001000, 0x000A0000, false);
           KeMapPhysicalRange(0x000A0000, 0x000B8000, false);
    text = KeMapPhysicalRange(0x000B8000, 0x000C0000, false);
           KeMapPhysicalRange(0x000C0000, 0x00100000, false);
    VmmShare(text, L"_fb_text");

    enable();
    ScEnableSwitch(true);
    ThrCreateThread(&proc_idle, true, KeStartupThread, false, NULL, 16, L"KeStartupThread");

    for (;;)
        ArchProcessorIdle();
}
