/* $Id: main.c,v 1.16 2002/03/14 01:27:06 pavlovskii Exp $ */

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
#include <kernel/profile.h>

#include <kernel/init.h>
#include <stdio.h>

extern process_t proc_idle;

void SemInit(semaphore_t *sem)
{
    sem->locks = 0;
    sem->owner = NULL;
}

static bool KernelEnumDevices(void *param, const wchar_t *name, const wchar_t *value)
{
    /*wprintf(L"KernelEnumDevices: loading device %s from driver %s\n",
	name, value);*/
    if (value != NULL)
	DevInstallDevice(value, name, NULL);
    return true;
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

    ProLoadProfile(L"system.pro", L"/");
    i386InitSerialDebug();
    ProEnumValues(L"Devices", NULL, KernelEnumDevices);

    /*FsMount(L"/hd", L"fat", IoOpenDevice(L"ide0a"));*/
    FsMount(L"/fd", L"fat", IoOpenDevice(L"fdc0"));

    ProcSpawnProcess(ProGetString(L"", L"Shell", SYS_BOOT L"/shell.exe"), 
        proc_idle.info);
    ScEnableSwitch(true);

    wprintf(L"Idle\n");

    for (;;)
	ArchProcessorIdle();
}
