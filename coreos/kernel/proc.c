/*
 * $History: proc.c $
 * 
 * *****************  Version 4  *****************
 * User: Tim          Date: 14/03/04   Time: 0:31
 * Updated in $/coreos/kernel
 * Removed redundant process handle prev/next links
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 7/03/04    Time: 13:06
 * Updated in $/coreos/kernel
 * Tidied up
 * Removed redundant ScNeedSchedule call from ProcCreateProcess
 * Duplicate main thread handle into new processes
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 13:59
 * Updated in $/coreos/kernel
 * Initial handle table created by handle.c
 * Tidied up
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/thread.h>
#include <kernel/fs.h>
#include <kernel/sched.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <os/defs.h>

void *morecore_user(size_t nbytes);

extern addr_t kernel_pagedir[];

process_t *proc_first, *proc_last;
unsigned proc_last_id;


static void ProcCleanupProcess(void *p);
static bool ProcIsSignalled(void *p);

static handle_class_t process_class = 
    HANDLE_CLASS('proc', ProcCleanupProcess, NULL, NULL, ProcIsSignalled);

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
        { 0 },                  /* hdr.lock */
        &process_class,
        {
                NULL,           /* hdr.waiting.first */
        },
        __FILE__,               /* hdr.file */
        __LINE__,               /* hdr.line */
    },
    NULL,                       /* prev */
    NULL,                       /* next */
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
    NULL,                       /* vmm_top */
    { 0 },                      /* sem_vmm */
    { 0 },                      /* sem_lock */
    L"kernel",                  /* exe */
    &proc_idle_info,            /* info */
};

/*
 * We can't include <os/rtl.h> because its function definitions confligt with 
 *  the kernel's.
 */
bool FsFullPath(const wchar_t* src, wchar_t* dst);

process_t *ProcSpawnProcess(const wchar_t *exe, const process_info_t *defaults)
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

    thr = ThrCreateThread(proc, false, (void (*)(void)) 0xdeadbeef, false, NULL, 16, L"main");
    HndDuplicate(&thr->hdr, proc);
    return proc;
}


static void ProcCleanupProcess(void *p)
{
    process_t *proc;
    proc = p;
    free((wchar_t*) proc->exe);
    free(proc);
}


static bool ProcIsSignalled(void *p)
{
    process_t *proc;
    proc = p;
    return proc->has_exited;
}


process_t *ProcCreateProcess(const wchar_t *exe)
{
    process_t *proc;
    vm_node_t *node;

    proc = malloc(sizeof(process_t));
    if (proc == NULL)
        return NULL;

    node = malloc(sizeof(vm_node_t));
    if (node == NULL)
        return NULL;

    memset(proc, 0, sizeof(*proc));
    proc->page_dir_phys = MemAllocPageDir();
    HndInit(&proc->hdr, &process_class);

    /* Copy number 1 is the process's handle to itself */
    proc->hdr.copies = 1;
    HndDuplicate(NULL, proc);
    HndDuplicate(&proc->hdr, proc);

    proc->stack_end = 0x80000000;
    proc->exe = _wcsdup(exe);
    proc->info = NULL;
    KeAtomicInc(&proc_last_id);
    proc->id = proc_last_id;
    proc->exit_code = 0;

    node->left = node->right = NULL;
    node->base = PAGE_SIZE;
    node->flags = 0;
    node->u.empty_pages = 0x100000 - 1025;
    proc->vmm_top = node;

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
    module_t *mod;
    thread_t *thr;
    addr_t stack;

    TRACE3("Creating process from %s: page dir = %x = %x\n", 
        proc->exe, proc->page_dir_phys, cr3);

	proc->user_heap = malloc_create_heap(&__av_default, morecore_user);
    if (proc->user_heap == NULL)
        return false;

    info = amalloc(proc->user_heap, sizeof(*info));

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
                VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
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

        if (info->std_in == 0)
            ptr = NULL;
        else
            ptr = HndRef(proc->creator, info->std_in, 0);

        if (ptr == NULL)
            info->std_in = 0;
        else
            info->std_in = HndDuplicate(HndCopy(ptr), proc);

        if (info->std_out == 0)
            ptr = NULL;
        else
            ptr = HndRef(proc->creator, info->std_out, 0);

        if (ptr == NULL)
            info->std_out = 0;
        else
            info->std_out = HndDuplicate(HndCopy(ptr), proc);

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
    info->module_first = info->module_last = NULL;

#if 0
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
            kmod = PeLoad(proc, mod->name, mod->base);
			if (kmod == NULL)
			{
				wprintf(L"ProcFirstTimeInit: failed to load kernel module %s\n", mod->name);
				assert(kmod != NULL);
			}

            kmod->imported = true;
        }
    }
#endif

    mod = PeLoad(proc, proc->exe, NULL);
    if (mod == NULL)
    {
        wprintf(L"ProcCreateProcess: failed to load %s\n", proc->exe);
        ProcExitProcess(0);
        return true;
    }

    info->base = mod->base;

    ctx = ThrGetUserContext(current());
    ctx->eip = mod->entry;

    mod = PeLoad(proc, L"libsys.dll", 0);
    if (mod != NULL)
        ctx->eip = PeGetExport(mod, "ProcStartup", (uint16_t) -1);

    wprintf(L"Created process %s\n", proc->exe);
    TRACE2("Successful; continuing at %lx, ctx = %p\n", ctx->eip, ctx);
    return true;
}


static void ProcFreeVmTree(vm_node_t *parent)
{
    if (parent != NULL)
    {
        ProcFreeVmTree(parent->left);
        if (parent->base < 0x80000000)
        {
            ProcFreeVmTree(parent->right);

            if (!VMM_NODE_IS_EMPTY(parent))
                VmmFreeNode(parent);
        }
    }
}

void ProcExitProcess(int code)
{
    thread_t *thr, *tnext;
    process_t *proc;
    module_t *mod, *mnext;

    proc = current()->process;

    SpinAcquire(&proc->sem_lock);
    proc->exit_code = code;

	ProcFreeVmTree(proc->vmm_top);

	for (mod = proc->mod_first; mod != NULL; mod = mnext)
    {
        mnext = mod->next;
        if (mod->base < 0x80000000)
			PeUnload(proc, mod);
    }

    /*for (i = 0; i < proc->handle_count; i++)
        if (proc->handles[i] != NULL &&
            proc->handles[i] != proc)
        {
            handle_hdr_t *hdr;
            hdr = proc->handles[i];
            wprintf(L"ProcExitProcess: closing handle %ld(%S, %d) = %p\n", 
                i, hdr->file, hdr->line, hdr);
            //HndClose(proc->handles[i]);
        }*/

    for (thr = thr_first; thr; thr = tnext)
    {
        tnext = thr->all_next;
        if (thr->process == proc)
            ThrDeleteThread(thr);
    }

    proc->has_exited = true;
    HndCompleteWait(&proc->hdr, true);
    HndClose(&proc->hdr);
    SpinRelease(&proc->sem_lock);

    wprintf(L"The process %s has exited with code %d\n", proc->exe, code);
}


int ProcGetExitCode(process_t *proc)
{
    int ret;

    wprintf(L"ProcGetExitCode(%p): exe = %p, sem_lock = %u/%p\n", 
        proc, proc->exe, proc->sem_lock.locks, proc->sem_lock.owner);

    SpinAcquire(&proc->sem_lock);
    ret = proc->exit_code;
    SpinRelease(&proc->sem_lock);

    return ret;
}


bool ProcPageFault(process_t *proc, addr_t addr, bool is_writing)
{
    if (proc->mod_first == NULL &&
        addr == 0xdeadbeef)
        return ProcFirstTimeInit(proc);

    if (addr < 0x80000000 && 
        addr >= proc->stack_end)
    {
        addr = PAGE_ALIGN(addr);
        if (MemGetPageState((void*) addr) == 0)
        {
            return MemMapRange((void*) addr, MemAlloc(), (uint8_t*) addr + PAGE_SIZE, 
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

    return VmmPageFault(proc, addr, is_writing);
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
    vm_node_t *node;

    node = malloc(sizeof(*node));
    node->base = 0;
    node->flags = 0;
    node->u.empty_pages = 0xC0000000 / PAGE_SIZE - 1024;
    node->left = node->right = NULL;

    HndDuplicate(NULL, &proc_idle);
    HndDuplicate(&proc_idle.hdr, &proc_idle);
    proc_idle.page_dir_phys = (addr_t) kernel_pagedir
        - (addr_t) scode 
        + kernel_startup.kernel_phys;
    proc_idle.vmm_top = node;

    proc_first = proc_last = &proc_idle;
    return true;
}
