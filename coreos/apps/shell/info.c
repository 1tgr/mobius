/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "common.h"

static void ShSplitTime(unsigned in, unsigned *h, unsigned *m, unsigned *s, unsigned *ms)
{
    *h = in / 3600000;
    in -= *h * 3600000;
    *m = in / 60000;
    in -= *m * 60000;
    *s = in / 1000;
    *ms = in - *s * 1000;
}


void ShCmdInfo(const wchar_t *command, wchar_t *params)
{
    wchar_t **names, **values;
    sysinfo_t info;
    systimes_t times;
    
    ShParseParams(params, &names, &values, &params);

    if (ShHasParam(names, L"system"))
    {
        if (SysGetInfo(&info))
        {
            size_t reserved_pages;
            reserved_pages = info.pages_physical - info.pages_total;
            printf("         System page size: %uKB\n", 
                info.page_size / 1024);
            printf("    Total physical memory: %uMB\n", 
                (info.pages_physical * info.page_size) / 0x100000);
            printf("Available physical memory: %uMB (%u%%)\n", 
                (info.pages_total * info.page_size) / 0x100000,
                (info.pages_total * 100) / info.pages_physical);
            printf(" Reserved physical memory: %uMB (%u%%)\n", 
                (reserved_pages * info.page_size) / 0x100000,
                100 - (info.pages_total * 100) / info.pages_physical);
            printf("     Free physical memory: %uMB (%u%% of total, %u%% of available)\n", 
                (info.pages_free * info.page_size) / 0x100000,
                (info.pages_free * 100) / info.pages_physical,
                (info.pages_free * 100) / info.pages_total);
            printf("   Kernel reserved memory: %uKB (%u%% of total, %u%% of reserved)\n",
                (info.pages_kernel * info.page_size) / 1024,
                (info.pages_kernel * 100) / info.pages_physical,
                (info.pages_kernel * 100) / reserved_pages);
        }
        else
            _pwerror(L"SysGetInfo");
    }

    if (ShHasParam(names, L"times"))
    {
        if (SysGetTimes(&times))
        {
            unsigned ms, s, m, h;
            printf("   System quantum: %u ms\n",
                times.quantum);
            ShSplitTime(times.uptime, &h, &m, &s, &ms);
            printf("    System uptime: %02u:%02u:%02u.%03u\n",
                h, m, s, ms);
            ShSplitTime(times.current_cputime, &h, &m, &s, &ms);
            printf("CPU time (thread): %02u:%02u:%02u.%03u (average %u%% CPU usage)\n",
                h, m, s, ms, (times.current_cputime * 100) / times.uptime);
        }
        else
            _pwerror(L"SysGetTimes");
    }

    free(names);
    free(values);
}
