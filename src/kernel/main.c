/* $Id: main.c,v 1.18 2002/05/05 13:43:24 pavlovskii Exp $ */

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

/*static bool KernelEnumDevices(void *param, const wchar_t *name, const wchar_t *value)
{
    if (value != NULL)
	DevInstallDevice(value, name, NULL);
    return true;
}

static bool KernelEnumMounts(void *param, const wchar_t *name, const wchar_t *value)
{
    wchar_t *dest, *type;

    if (value != NULL)
    {
        dest = wcschr(value, ',');
        if (dest == NULL)
        {
            wprintf(L"KernelEnumMounts: mounting %s as %s\n",
                name, value);
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
        }

        FsMount(name, type, dest);

        if (dest != NULL)
            free(type);
    }

    return true;
}*/

void KeInstallDevices(void)
{
    unsigned i, j;
    wchar_t value[10], line[256], *tok, *comma;
    const wchar_t *ptr, *driver, *device_path, *dest;
    bool is_mount;

    wcscpy(value, L"0");
    for (i = 0; 
        (ptr = ProGetString(L"Devices", value, NULL)) != NULL; 
        i++)
    {
        memset(line, 0, sizeof(line));
        wcsncpy(line, ptr, _countof(line) - 1);

        j = 0;
        driver = device_path = dest = NULL;
        tok = line;

        while ((comma = wcschr(tok, ',')) != NULL)
        {
            *comma = '\0';
            tok = comma + 1;
        }

        tok = line;
        while (*tok != '\0')
        {
            switch (j)
            {
            case 0: /* action */
                if (_wcsicmp(tok, L"device") == 0)
                    is_mount = false;
                else if (_wcsicmp(tok, L"mount") == 0)
                    is_mount = true;
                else
                {
                    wprintf(L"system.pro/Devices: invalid device/mount option: %s\n", tok);
                    tok = NULL;
                    continue;
                }

                break;

            case 1: /* driver name */
                driver = tok;
                if (!is_mount)
                    device_path = driver;
                break;

            case 2: /* device name or mount point path */
                device_path = tok;
                break;

            case 3: /* mount point device (optional) */
                if (is_mount)
                    dest = tok;
                else
                    wprintf(L"system.pro/Devices: mount point device name (%s) is only valid for mount points\n",
                        tok);
                break;
            }

            j++;
            tok += wcslen(tok) + 1;
        }

        if (driver == NULL)
            wprintf(L"system.pro/Devices: need to specify at least driver name\n");
        else if (is_mount)
        {
            wprintf(L"Mounting %s on %s as %s\n", device_path, dest, driver);
            FsMount(device_path, driver, dest);
        }
        else
        {
            wprintf(L"Installing device %s using driver %s\n", device_path, driver);
            DevInstallDevice(driver, device_path, NULL);
        }

        swprintf(value, L"%u", i + 1);
    }
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

    ProLoadProfile(L"/System/Boot/system.pro", L"/");
    i386InitSerialDebug();

    KeInstallDevices();

    wcscpy(proc_idle.info->cwd, L"/hd/boot");
    ProcSpawnProcess(ProGetString(L"", L"Shell", SYS_BOOT L"/shell.exe"), 
        proc_idle.info);
    ScEnableSwitch(true);

    wprintf(L"Idle\n");

    for (;;)
	ArchProcessorIdle();
}
