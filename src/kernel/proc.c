/* $Id: proc.c,v 1.23 2002/09/03 13:13:31 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/thread.h>
#include <kernel/fs.h>
#include <kernel/sched.h>

/*#define DEBUG*/
#include <kernel/debug.h>
#include <os/rtl.h>
#include <os/syscall.h>

#include <stdlib.h>
#include <wchar.h>
#include <os/defs.h>

//extern char kernel_stack_end[], scode[];
extern addr_t kernel_pagedir[];

process_t *proc_first, *proc_last;
unsigned proc_last_id;

process_info_t proc_idle_info =
{
    0,                          /* id */
    (addr_t) scode,             /* base */
    NULL, NULL,                 /* std_in, std_out */
    SYS_BOOT,                   /* cwd */
    L"",                        /* cmdline */
};

module_t mod_kernel =
{
    NULL,                       /* prev */
    NULL,                       /* next */
    (unsigned) -1,              /* refs */
    SYS_BOOT L"/kernel.exe",    /* name */
    (addr_t) scode,             /* base */
    0x1000000,                  /* length */
    NULL,                       /* entry */
    NULL,                       /* file */
    PAGE_SIZE,                  /* sizeof_headers */
    true,                       /* imported */
};

process_t proc_idle = 
{
    {
        0,                      /* hdr.locks */
        'proc',                 /* hdr.tag */
        0,                      /* flags */
        NULL,                   /* hdr.locked_by */
        0,                      /* hdr.signal */
        {
                NULL,           /* hdr.waiting.first */
                NULL,           /* hdr.waiting.last */
                NULL,           /* hdr.current */
        },
        __FILE__,               /* hdr.file */
        __LINE__,               /* hdr.line */
    },
    NULL,                       /* prev */
    NULL,                       /* next */
    /*(addr_t) kernel_stack_end,*/  /* stack_end */
    0x80000000,                 /*
                                 * stack_end -- this is only used for user 
                                 *  threads running in proc_idle
                                 */
    0,                          /* page_dir_phys */
    NULL,                       /* handles */
    0,                          /* handle_count */
    0,                          /* handle_allocated */
    &mod_kernel,                /* mod_first */
    &mod_kernel,                /* mod_last */
    NULL,                       /* area_first */
    NULL,                       /* area_last */
    /*0xe8000000,*/             /* vmm_end */
    { 0 },                      /* sem_vmm */
    { 0 },                      /* sem_lock */
    L"kernel",                  /* exe */
    &proc_idle_info,            /* info */
};

handle_t ProcSpawnProcess(const wchar_t *exe, const process_info_t *defaults)
{
    process_t *proc;
    thread_t *thr;
    wchar_t temp[MAX_PATH];

    FsFullPath(exe, temp);
    KeAtomicInc((unsigned*) &current()->process->hdr.copies);
    proc = ProcCreateProcess(temp);
    if (proc == NULL)
        return NULL;

    proc->creator = current()->process;
    proc->info = malloc(sizeof(*proc->info));
    if (defaults == NULL)
        defaults = proc->creator->info;
    memcpy(proc->info, defaults, sizeof(*defaults));

    HndInheritHandles(proc->creator, proc);

    thr = ThrCreateThread(proc, false, (void (*)(void)) 0xdeadbeef, false, NULL, 16);
    ScNeedSchedule(true);
    //wprintf(L"ProcSpawnProcess(%s): proc = %p\n", exe, proc);
    return HndDuplicate(current()->process, &proc->hdr);
}

static void ProcCleanupProcess(void *p)
{
    process_t *proc;

    proc = p;
    wprintf(L"ProcCleanupProcess: proc = %s, current()->process = %s...",
        proc->exe, current()->process->exe);

    /*SpinAcquire(&proc->sem_lock);

    assert(proc->hdr.copies == 0);
    free((wchar_t*) proc->exe);
    SpinRelease(&proc->sem_lock);
    free(proc);*/
    wprintf(L"done\n");
}

process_t *ProcCreateProcess(const wchar_t *exe)
{
    process_t *proc;
    vm_area_t *area;

    proc = malloc(sizeof(process_t));
    if (proc == NULL)
        return NULL;

    area = malloc(sizeof(vm_area_t));
    if (area == NULL)
        return NULL;

    memset(proc, 0, sizeof(*proc));
    proc->page_dir_phys = MemAllocPageDir();
    proc->hdr.tag = 'proc';
    proc->hdr.file = __FILE__;
    proc->hdr.line = __LINE__;
    proc->hdr.free_callback = ProcCleanupProcess;

    /* Copy number 1 is the process's handle to itself */
    proc->hdr.copies = 1;
    proc->handle_count = 2;
    proc->handle_allocated = 16;
    proc->handles = malloc(proc->handle_allocated * sizeof(void*));
    memset(proc->handles, 0, proc->handle_allocated * sizeof(void*));
    /*proc->handles[0] = NULL;*/
    proc->handles[1] = proc;
    /*proc->vmm_end = PAGE_SIZE;*/
    proc->stack_end = 0x80000000;
    proc->exe = _wcsdup(exe);
    proc->info = NULL;
    KeAtomicInc(&proc_last_id);
    proc->id = proc_last_id;
    proc->exit_code = 0;

    /* Create a VM area spanning the whole address space */
    memset(area, 0, sizeof(vm_area_t));
    area->owner = proc;
    area->start = PAGE_SIZE;
    area->num_pages = 0x100000 - 1025; /* 4GB - 4MB - 4096 -- trust me on this */
    area->type = VM_AREA_EMPTY;
    proc->area_first = proc->area_last = area;

    TRACE2("ProcCreateProcess(%s): page dir = %x\n", 
            exe, proc->page_dir_phys);
    LIST_ADD(proc, proc);
    return proc;
}

bool ProcFirstTimeInit(process_t *proc)
{
    context_t *ctx;
    process_info_t *info;
    wchar_t *ch;
    module_t *mod, *kmod;
    thread_t *thr;
    //uint32_t cr3;
    addr_t stack;

    /*SpinAcquire(&proc->sem_lock);*/
    //__asm__("mov %%cr3,%0" : "=r" (cr3));
    TRACE3("Creating process from %s: page dir = %x = %x\n", 
        proc->exe, proc->page_dir_phys, cr3);

    info = VmmAlloc(1, NULL, 
        3 | MEM_READ | MEM_WRITE | MEM_ZERO | MEM_COMMIT);
    if (info == NULL)
    {
        //SpinRelease(&proc->sem_lock);
        return false;
    }

    for (thr = thr_first; thr; thr = thr->all_next)
    {
        if (thr->process == proc)
        {
            if (thr->info == NULL)
            {
                ThrAllocateThreadInfo(thr);
                assert(thr->info != NULL);
            }

            thr->info->process = info;

            stack = (addr_t) VmmAlloc(0x100000 / PAGE_SIZE, 
                thr->user_stack_top - 0x100000, 
                3 | MEM_READ | MEM_WRITE);
            wprintf(L"ProcFirstTimeInit: user stack at %x\n", stack);
            stack += 0x100000;
            assert(stack == thr->user_stack_top);

            ctx = ThrGetUserContext(thr);
            ctx->esp = stack;
        }
    }

    if (proc->info != NULL)
    {
        handle_hdr_t *ptr;
        memcpy(info, proc->info, sizeof(*info));

        ptr = HndGetPtr(proc->creator, info->std_in, 0);
        if (ptr)
            info->std_in = HndDuplicate(proc, ptr);
        else
            info->std_in = NULL;

        ptr = HndGetPtr(proc->creator, info->std_out, 0);
        if (ptr)
            info->std_out = HndDuplicate(proc, ptr);
        else
            info->std_out = NULL;

        free(proc->info);
    }
    else
    {
        ch = wcsrchr(proc->exe, '/');

        if (ch == NULL)
            wcscpy(info->cwd, L"/");
        else
            wcsncpy(info->cwd, proc->exe, ch - proc->exe);
    }

    if (info->std_in == NULL)
        info->std_in = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
    if (info->std_out == NULL)
        info->std_out = FsOpen(SYS_DEVICES L"/tty1", FILE_WRITE);

    if (proc->creator == NULL)
        proc->root = proc->cur_dir = proc_idle.root;
    else
    {
        KeAtomicDec((unsigned*) &proc->creator->hdr.copies);
        proc->root = proc->creator->root;
        proc->cur_dir = proc->creator->cur_dir;
    }

    proc->info = info;
    info->id = proc->id;

    /* 
     * Kernel modules need to be duplicated first, in case this is the first 
     *  process (e.g. the shell) and running it involves faulting in some 
     *  kernel-mode code (e.g. the FSD)
     */
    for (mod = proc_idle.mod_first; mod != NULL; mod = mod->next)
    {
        /* The kernel must already be fully mapped */
        if (mod != &mod_kernel)
        {
            /*wprintf(L"ProcFirstTimeInit: loading kernel module %s\n",
                mod->name);*/
            kmod = PeLoad(proc, mod->name, mod->base);
            kmod->imported = true;
        }
    }

    mod = PeLoad(proc, proc->exe, NULL);
    if (mod == NULL)
    {
        wprintf(L"ProcCreateProcess: failed to load %s\n", proc->exe);
        //SpinRelease(&proc->sem_lock);
        ProcExitProcess(0);
        return true;
    }

    //FsChangeDir(proc->info->cwd);
    info->base = mod->base;
    /*ctx = ThrGetContext(current);*/
    ctx = ThrGetUserContext(current());
    ctx->eip = mod->entry;

    TRACE2("Successful; continuing at %lx, ctx = %p\n", ctx->eip, ctx);
    /*SpinRelease(&proc->sem_lock);*/
    return true;
}

void ProcExitProcess(int code)
{
    thread_t *thr, *tnext;
    //vm_area_t *area, *anext;
    process_t *proc;
    //unsigned i;
    //void **handles;

    proc = current()->process;

    SpinAcquire(&proc->sem_lock);
    wprintf(L"The process %s has exited with code %d\n", proc->exe, code);
    proc->exit_code = code;

    for (thr = thr_first; thr; thr = tnext)
    {
        tnext = thr->all_next;
        if (thr->process == proc)
            ThrDeleteThread(thr);
    }

    /*for (area = proc->area_first; area != NULL; area = anext)
    {
        anext = area->next;
        if (area->type != VM_AREA_EMPTY)
        {
            assert(area->owner == proc);
            //wprintf(L"ProcExitProcess: freeing area at %x...", area->start);
            VmmFree(area);
            free(area);
            //wprintf(L"done\n");
        }
    }

    proc->area_first = proc->area_last = NULL;

    for (i = 0; i < proc->handle_count; i++)
        if (proc->handles[i] != NULL &&
            proc->handles[i] != proc)
        {
            handle_hdr_t *hdr;
            hdr = proc->handles[i];
            //wprintf(L"ProcExitProcess: (notionally) closing handle %ld(%S, %d)\n", 
                //i, hdr->file, hdr->line);
            HndClose(proc, i, 0);
        }*/

    /*for (area = proc->area_first; area != NULL; area = anext)
    {
        assert(area->type == VM_AREA_EMPTY);
        anext = area->next;
        free(area);
    }*/

    HndSignalPtr(&proc->hdr, true);
    /*handles = proc->handles;
    wprintf(L"ProcExitProcess: closing handle...");
    HndClose(proc, HANDLE_PROCESS, 'proc');
    wprintf(L"done\n");
    free(handles);*/
    SpinRelease(&proc->sem_lock);

    /*wprintf(L"ProcExitProcess(%p): finished, copies = %u, sem_lock = %u\n", 
        proc, proc->hdr.copies, proc->sem_lock.locks);*/
}

int ProcGetExitCode(handle_t hnd)
{
    handle_hdr_t *hdr;
    process_t *proc;
    int ret;

    hdr = HndLock(NULL, hnd, 'proc');
    if (hdr == NULL)
        return 0;

    proc = (process_t*) (hdr - 1);
    wprintf(L"ProcGetExitCode(%p): exe = %p, sem_lock = %u/%p\n", 
        proc, proc->exe, proc->sem_lock.locks, proc->sem_lock.owner);

    SpinAcquire(&proc->sem_lock);
    ret = proc->exit_code;
    SpinRelease(&proc->sem_lock);

    HndUnlock(NULL, hnd, 'proc');
    return ret;
}

bool ProcPageFault(process_t *proc, addr_t addr, bool is_writing)
{
    vm_area_t *area;
    /*module_t *mod;*/
#ifdef DEBUG
    static unsigned nest;
    unsigned i;

    for (i = 0; i < nest; i++)
        _cputws(L"  ", 2);

    wprintf(L"(%u) ProcPageFault(%s, %lx)\n", nest++, proc->exe, addr);
#endif

    if (proc->mod_first == NULL &&
        addr == 0xdeadbeef)
    {
#ifdef DEBUG
        nest--;
        for (i = 0; i < nest; i++)
            _cputws(L"  ", 2);
        wprintf(L"(%u) First-time init\n", nest);
#endif
        return ProcFirstTimeInit(proc);
    }

    if (addr < 0x80000000 && 
        addr >= proc->stack_end)
    {
        addr = PAGE_ALIGN(addr);
        if (MemGetPageState((void*) addr) == 0)
        {
#ifdef DEBUG
            nest--;
            for (i = 0; i < nest; i++)
                _cputws(L"  ", 2);
            wprintf(L"(%u) Stack\n", nest);
#endif
            return MemMap(addr, MemAlloc(), addr + PAGE_SIZE, 
                PRIV_USER | PRIV_RD | PRIV_WR | PRIV_PRES);
        }
        else
        {
            static bool nested;
            wprintf(L"ProcPageFault: invalid stack page %x\n", addr);
            if (nested)
                halt(0);
            else
            {
                nested = true;
                __asm__("int3");
            }
            return false;
        }
    }

    FOREACH (area, proc->area)
    {
        if (area->type != VM_AREA_EMPTY && 
            addr >= area->start &&
            addr < area->start + PAGE_SIZE * area->num_pages)
        {
#ifdef DEBUG
            nest--;
            for (i = 0; i < nest; i++)
                _cputws(L"  ", 2);
            wprintf(L"(%u) VMM area: user (%x)\n", nest, area->start);
#endif
            //return VmmCommit(area, NULL, -1);
            return VmmPageFault(area, addr, is_writing);
        }
    }

    if (proc != &proc_idle)
        FOREACH (area, proc_idle.area)
        {
            if (area->type != VM_AREA_EMPTY && 
                addr >= area->start &&
                addr < area->start + PAGE_SIZE * area->num_pages)
            {
#ifdef DEBUG
                nest--;
                for (i = 0; i < nest; i++)
                    _cputws(L"  ", 2);
                wprintf(L"(%u) VMM area: kernel (%x)\n", nest, area->start);
#endif
                //return VmmCommit(area, NULL, -1);
                return VmmPageFault(area, addr, is_writing);
            }
        }

#ifdef DEBUG
    nest--;
    for (i = 0; i < nest; i++)
        _cputws(L"  ", 2);
    wprintf(L"(%u) Not handled\n", nest);
#endif
    return false;
}

/*!
*   \brief        Initializes the idle process
*
*   This function sets up the idle process's initial handle table and
*   physical page directory pointer.
*
*   \return        \p true
*/
bool ProcInit(void)
{
    vm_area_t *area;

    /*area = malloc(sizeof(vm_area_t));
    memset(area, 0, sizeof(vm_area_t));
    area->owner = &proc_idle;
    area->start = 0x80000000;
    area->num_pages = 0x40000000 / PAGE_SIZE - 1; // 1GB
    area->type = VM_AREA_EMPTY;*/

    area = malloc(sizeof(vm_area_t));
    memset(area, 0, sizeof(vm_area_t));
    area->owner = &proc_idle;
    area->start = PAGE_SIZE;
    area->num_pages = 0xC0000000 / PAGE_SIZE - 1025; /* 4GB - 4MB - 4096 */
    area->type = VM_AREA_EMPTY;

    proc_idle.handle_count = 2;
    proc_idle.handle_allocated = 16;
    proc_idle.handles = malloc(proc_idle.handle_allocated * sizeof(void*));
    proc_idle.handles[0] = NULL;
    proc_idle.handles[1] = &proc_idle;
    proc_idle.page_dir_phys = (addr_t) kernel_pagedir/*proc_idle.page_dir */
        - (addr_t) scode 
        + kernel_startup.kernel_phys;
    proc_idle.area_first = proc_idle.area_last = area;

    proc_first = proc_last = &proc_idle;
    return true;
}
