/* $Id: main.c,v 1.14 2002/03/07 15:52:03 pavlovskii Exp $ */

/*!
 *    \defgroup    kernel    Kernel
 */

#include <os/syscall.h>

#include <kernel/kernel.h>
#include <kernel/sched.h>
#include <kernel/thread.h>
#include <kernel/driver.h>
#include <kernel/io.h>
#include <kernel/fs.h>
#include <kernel/arch.h>
#include <kernel/proc.h>
#include <kernel/debug.h>

#include <kernel/init.h>
#include <stdio.h>

extern process_t proc_idle;

void SemInit(semaphore_t *sem)
{
    sem->locks = 0;
    sem->owner = NULL;
}

void KernelMain(void)
{
    MemInit();
    ArchInit();
    
    TRACE0("ProcInit\n");
    ProcInit();
    TRACE0("RdInit\n");
    RdInit();
    TRACE0("FsInit\n");
    FsInit();

    DevInstallDevice(L"keyboard", L"keyboard", NULL);
    DevInstallDevice(L"tty", L"tty0", NULL);
    DevInstallDevice(L"tty", L"tty1", NULL);
    DevInstallDevice(L"tty", L"tty2", NULL);
    DevInstallDevice(L"tty", L"tty3", NULL);
    DevInstallDevice(L"tty", L"tty4", NULL);
    DevInstallDevice(L"tty", L"tty5", NULL);
    DevInstallDevice(L"tty", L"tty6", NULL);
    DevInstallDevice(L"cmos", L"cmos", NULL);
    DevInstallDevice(L"video", L"video", NULL);
    
    FsMount(L"/hd", L"fat", IoOpenDevice(L"ide0a"));
    FsMount(L"/fd", L"fat", IoOpenDevice(L"fdc0"));
    
    ProcSpawnProcess(SYS_BOOT L"/shell.exe", proc_idle.info);
    ScEnableSwitch(true);
    
    wprintf(L"Idle\n");
    DevInstallDevice(L"pci", L"pci", NULL);
    
    for (;;)
	ArchProcessorIdle();
}
