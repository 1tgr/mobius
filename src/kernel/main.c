/* $Id: main.c,v 1.23 2002/08/17 19:13:32 pavlovskii Exp $ */

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
#include <string.h>

extern process_t proc_idle;

void SpinInit(spinlock_t *sem)
{
    sem->locks = 0;
    sem->owner = NULL;
}

void TextUpdateProgress(int min, int progress, int max);
extern unsigned sc_uptime;

static void KernelCpuMeter(void)
{
    unsigned up_last, up_cur, up_own;

    up_last = sc_uptime;
    for (;;)
    {
        up_cur = sc_uptime;
        up_own = cpu()->thr_idle.cputime;
        cpu()->thr_idle.cputime = 0;
        TextUpdateProgress(0, up_cur - up_last - up_own, up_cur - up_last);
        up_last = up_cur;

        ThrSleep(current(), 100);
        KeYield();
    }
}

static void __initcode KeInstallDevices(void)
{
    unsigned i, j, count;
    wchar_t value[10], line[256], *tok, *comma;
    const wchar_t *ptr, *driver, *device_path, *dest;
    bool is_mount;
    device_config_t *cfg;

    wcscpy(value, L"0");
    for (count = 0; (ptr = ProGetString(L"Devices", value, NULL)) != NULL; count++)
        swprintf(value, L"%u", count + 1);

    TextUpdateProgress(0, 0, count - 1);

    wcscpy(value, L"0");
    for (i = 0; i < count; i++)
    {
        memset(line, 0, sizeof(line));

        ptr = ProGetString(L"Devices", value, NULL);
        if (ptr == NULL)
            break;

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
            if (!FsCreateDir(device_path))
                wprintf(L"warning: failed to create mount point %s\n", device_path);
            FsMount(device_path, driver, dest);
        }
        else
        {
            cfg = malloc(sizeof(device_config_t));
            cfg->parent = NULL;
            cfg->num_resources = 0;
            cfg->resources = NULL;
            cfg->vendor_id = 0xffff;
            cfg->device_id = 0xffff;
            cfg->subsystem = 0;
            cfg->bus_type = DEV_BUS_UNKNOWN;
            cfg->device_class = 0;
            cfg->reserved = 0;
            wprintf(L"Installing device %s using driver %s\n", device_path, driver);
            DevInstallDevice(driver, device_path, cfg);
        }

        TextUpdateProgress(0, i, count - 1);
        swprintf(value, L"%u", i + 1);
    }

    wcscpy(proc_idle.info->cwd, L"/");
    ptr = ProGetString(L"", L"Shell", NULL);
    if (ptr == NULL)
        DbgStartShell();
    else
        ProcSpawnProcess(ptr, proc_idle.info);

#ifndef WIN32
    ThrCreateThread(&proc_idle, true, KernelCpuMeter, false, NULL, 20);
#endif
    TextUpdateProgress(0, 0, 0);
    ThrExitThread(0);
    KeYield();
}

extern vnode_t fs_root;

void KernelMain(void)
{
    MemInit();
    RtlInit();
    ArchInit();

    TRACE0("ProcInit\n");
    ProcInit();
    TRACE0("RdInit\n");
    RdInit();
    TRACE0("FsInit\n");
    FsInit();

    proc_idle.root = fs_root;
    /* xxx - need to have something here or FsChangeDir will get confused */
    wcscpy(proc_idle.info->cwd, L"/");
    FsChangeDir(L"/");

    if (!ProLoadProfile(SYS_BOOT L"/system.pro", L"/"))
        wprintf(L"KernelMain: unable to load " SYS_BOOT L"/system.pro\n");

    i386InitSerialDebug();

    ScEnableSwitch(true);
    ThrCreateThread(&proc_idle, true, KeInstallDevices, false, NULL, 16);
    //KeInstallDevices();
    TRACE0("Idle\n");

    for (;;)
        ArchProcessorIdle();
}
