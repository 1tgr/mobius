/* $Id: main.c,v 1.17 2002/04/20 12:30:03 pavlovskii Exp $ */

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
#include <wchar.h>

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

static bool KernelEnumMounts(void *param, const wchar_t *name, const wchar_t *value)
{
    wchar_t *dest, *type;
    device_t *dev;

    if (value != NULL)
    {
        dest = wcschr(value, ',');
        if (dest == NULL)
        {
            wprintf(L"KernelEnumMounts: mounting %s as %s\n",
                name, value);
            dev = NULL;
            type = (wchar_t*) value;
        }
        else
        {
            type = malloc(sizeof(wchar_t) * (dest - value));
            wcsncpy(type, value, dest - value);
            type[dest - value] = '\0';
            dest++;

            wprintf(L"KernelEnumMounts: mounting %s on %s as %s\n",
                name, dest, type);

            dev = IoOpenDevice(dest);
            if (dev == NULL)
            {
                wprintf(L"%s: not found\n", dest);
                free(type);
                return true;
            }
        }

        FsMount(name, type, dev);

        if (dest != NULL)
            free(type);
    }

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
    ProEnumValues(L"Mounts", NULL, KernelEnumMounts);

    /*FsMount(L"/hd", L"fat", IoOpenDevice(L"ide0a"));
    FsMount(L"/cd", L"iso9660", IoOpenDevice(L"ide1"));*/
    /*FsMount(L"/fd", L"fat", IoOpenDevice(L"fdc0"));*/

    ProcSpawnProcess(ProGetString(L"", L"Shell", SYS_BOOT L"/shell.exe"), 
        proc_idle.info);
    ScEnableSwitch(true);

    wprintf(L"Idle\n");

    for (;;)
	ArchProcessorIdle();
}
