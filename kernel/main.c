/* $Id: main.c,v 1.1 2002/12/21 09:49:25 pavlovskii Exp $ */

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

static void KeInstallAddon(const wchar_t *name)
{
    wchar_t path[256];
    module_t *mod;
    void (*startup)(void);

    swprintf(path, SYS_BOOT L"/%s.kll", name);
    mod = PeLoad(NULL, path, 0);
    if (mod == NULL)
        wprintf(L"KeInstallAddon(%s): failed to load %s\n", name, path);

    startup = (void*) mod->entry;
    startup();
}

static void __initcode KeInstallDevices(void)
{
    unsigned i, j, count;
    wchar_t value[10], line[256], *tok, *comma, key[50];
    const wchar_t *ptr, *driver, *device_path, *dest;
    device_config_t *cfg;
    enum { UNKNOWN, MOUNT, DEVICE, ADDON } action;

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
        action = UNKNOWN;
        while (*tok != '\0')
        {
            switch (j)
            {
            case 0: /* action */
                if (_wcsicmp(tok, L"device") == 0)
                    action = DEVICE;
                else if (_wcsicmp(tok, L"mount") == 0)
                    action = MOUNT;
                else if (_wcsicmp(tok, L"addon") == 0)
                    action = ADDON;
                else
                {
                    wprintf(L"system.pro/Devices: invalid action option: %s\n", tok);
                    tok = NULL;
                    continue;
                }

                break;

            case 1: /* driver name */
                driver = tok;
                if (action == DEVICE)
                    device_path = driver;
                break;

            case 2: /* device name or mount point path */
                if (action < ADDON)
                    device_path = tok;
                break;

            case 3: /* mount point device (optional) */
                if (action == MOUNT)
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
        else
        {
            switch (action)
            {
            case MOUNT:
                wprintf(L"Mounting %s on %s as %s\n", device_path, dest, driver);
                if (!FsCreateDir(device_path))
                    wprintf(L"warning: failed to create mount point %s\n", device_path);
                FsMount(device_path, driver, dest);
                break;

            case DEVICE:
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
                wprintf(L"-- %s (%s)\n", device_path, driver);
                swprintf(key, L"ISA/%s", driver);
                DevInstallDevice(driver, device_path, cfg, key);
                break;

            case ADDON:
                wprintf(L"-- %s (addon)\n", driver);
                KeInstallAddon(driver);
                break;

            case UNKNOWN:
                break;
            }
        }

        TextUpdateProgress(0, i, count - 1);
        swprintf(value, L"%u", i + 1);
    }

    wcscpy(proc_idle.info->cwd, L"/");
    ptr = ProGetString(L"", L"Shell", NULL);
    if (ptr == NULL)
    {
        wprintf(L"No Shell setting found in system.pro; starting debugger\n");
        DbgStartShell();
    }
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
    ThrCreateThread(&proc_idle, true, MemZeroPageThread, false, NULL, 24);
    //KeInstallDevices();
    TRACE0("Idle\n");

    for (;;)
        ArchProcessorIdle();
}
