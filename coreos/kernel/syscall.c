/* $Id: syscall.c,v 1.1.1.1 2002/12/21 09:49:29 pavlovskii Exp $ */
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/arch.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/counters.h>

/* #define DEBUG */
#include <kernel/debug.h>

#include <kernel/syscall.h>

#include <errno.h>
#include <wchar.h>
#include <stdio.h>
//#include <malloc.h>

int WrapHello(int a, int b)
{
    wprintf(L"Hello, world! %d %d\n", a, b);
    return 42;
}


int WrapDbgWrite(const wchar_t *str, size_t count)
{
    return DbgWrite(str, count);
}


void WrapThrKillThread(int code)
{
    ThrExitThread(code);
}


unsigned WrapSysUpTime(void)
{
    return sc_uptime;
}


bool WrapThrWaitHandle(handle_t hnd)
{
    handle_hdr_t *hdr;

    hdr = HndRef(NULL, hnd, 0);
    if (hdr == NULL)
        return false;

    if (ThrWaitHandle(current(), hdr))
    {
        ScNeedSchedule(true);
        return true;
    }
    else
        return false;
}

int WrapThrWaitMultiple(const handle_t *handles, unsigned num_handles, bool wait_all)
{
    handle_hdr_t **hdrs;
    int ret;
    unsigned i;

    if (num_handles >= _countof(current()->wait_handles))
    {
        errno = EMFILE;
    }

    hdrs = malloc(sizeof(handle_hdr_t*) * num_handles);
    for (i = 0; i < num_handles; i++)
    {
        hdrs[i] = HndRef(NULL, handles[i], 0);
        if (hdrs[i] == NULL)
        {
            free(hdrs);
            return -1;
        }
    }

    if (!ThrWaitMultiple(current(), hdrs, num_handles, wait_all, &ret))
    {
        free(hdrs);
        return -1;
    }
    else
    {
        free(hdrs);

        while (ret < 0)
        {
            wprintf(L"SysThrWaitMultiple: %u\n", sc_uptime);
            //if (num_handles > 0)
                //__asm__("int3");
            KeYield();
        }

        return ret;
    }
}

void WrapThrSleep(unsigned ms)
{
    ThrSleep(current(), ms);
}

bool WrapThrContinue(const context_t *ctx)
{
    return ThrContinue(current(), ctx);
}

bool WrapSysGetInfo(sysinfo_t *info)
{
    info->page_size = PAGE_SIZE;
    //info->pages_total = pool_all.num_pages + pool_low.num_pages;
    //info->pages_free = pool_all.free_pages + pool_low.free_pages;
    info->pages_total = PAGE_ALIGN_UP(kernel_startup.memory_size - kernel_startup.kernel_data) / PAGE_SIZE;
    info->pages_free = mem_zero.num_pages;
    info->pages_physical = PAGE_ALIGN_UP(kernel_startup.memory_size) / PAGE_SIZE;
    info->pages_kernel = PAGE_ALIGN_UP(kernel_startup.kernel_data) / PAGE_SIZE;
    return true;
}

bool WrapSysGetTimes(systimes_t *times)
{
    times->quantum = SCHED_QUANTUM;
    times->uptime = sc_uptime;
    times->current_cputime = current()->cputime;
    return true;
}

#ifdef i386
handle_t WrapThrCreateV86Thread(FARPTR entry, FARPTR stack_top, 
                                unsigned priority, void (*handler)(void), 
                                const wchar_t *name)
{
    thread_t *thr;
    thr = i386CreateV86Thread(entry, stack_top, priority, handler, name);
    if (thr != NULL)
        return HndDuplicate(&thr->hdr, current()->process);
    else
        return NULL;
}

bool WrapThrGetV86Context(context_v86_t* ctx)
{
    if (!MemVerifyBuffer(ctx, sizeof(*ctx)))
    {
    errno = EBUFFER;
    return false;
    }

    if (current()->v86_in_handler)
    {
    *ctx = current()->v86_context;
    if (current()->v86_if)
        ctx->eflags |= EFLAG_IF;
    else
        ctx->eflags &= ~EFLAG_IF;
    return true;
    }
    else
    {
    errno = EINVALID;
    return false;
    }
}

bool WrapThrSetV86Context(const context_v86_t* ctx)
{
    if (!MemVerifyBuffer(ctx, sizeof(*ctx)))
    {
    errno = EBUFFER;
    return false;
    }

    if (current()->v86_in_handler)
    {
    current()->v86_context = *ctx;
    return true;
    }
    else
    {
    errno = EINVALID;
    return false;
    }
}

bool WrapThrContinueV86(void)
{
    context_t *ctx;
    context_v86_t *v86;
    uint32_t kernel_esp;

    if (current()->v86_in_handler)
    {
    ctx = ThrGetUserContext(current());
    kernel_esp = ctx->kernel_esp;
    kernel_esp -= sizeof(context_v86_t) - sizeof(context_t);
    /*current->kernel_esp = */ctx->kernel_esp = kernel_esp;

    /*v86 = (context_v86_t*) ThrGetUserContext(current);*/
    v86 = (context_v86_t*) (kernel_esp - 4);
    *v86 = current()->v86_context;

    current()->v86_if = (v86->eflags & EFLAG_IF) == EFLAG_IF;

    TRACE2("ThrContinueV86: continuing: new esp = %x, old esp = %x\n",
        kernel_esp, v86->kernel_esp);
    v86->eflags |= EFLAG_IF | EFLAG_VM;
    /*v86->kernel_esp = kernel_esp;*/
    /*ArchDbgDumpContext((context_t*) v86);*/
    current()->v86_in_handler = false;
    __asm__("mov %0,%%eax\n"
        "jmp _isr_switch_ret" : : "g" (kernel_esp));
    return true;
    }
    else
    {
    errno = EINVALID;
    return false;
    }
}

#else
handle_t WrapThrCreateV86Thread(uint32_t entry, uint32_t stack_top, 
                                unsigned priority, void (*handler)(void), 
                                const wchar_t *name)
{
    errno = ENOTIMPL;
    return NULL;
}
#endif

handle_t WrapThrCreateThread(int (*entry)(void*), void *param, unsigned priority, const wchar_t *name)
{
    thread_t *thr;
    thr = ThrCreateThread(current()->process, false, (void (*)(void)) entry, 
        true, param, priority, name);
    if (thr == NULL)
        return NULL;
    else
        return HndDuplicate(&thr->hdr, current()->process);
}


handle_t WrapEvtCreate(bool is_signalled)
{
    event_t *evt;

    evt = EvtCreate(is_signalled);
    if (evt == NULL)
        return 0;

    return HndDuplicate(&evt->hdr, NULL);
}


bool WrapEvtSignal(handle_t event, bool is_signalled)
{
    event_t *evt;

    evt = (event_t*) HndRef(NULL, event, 'evnt');
    if (evt == NULL)
        return false;

    EvtSignal(evt, is_signalled);
    return true;
}


bool WrapEvtIsSignalled(handle_t event)
{
    event_t *evt;

    evt = (event_t*) HndRef(NULL, event, 'evnt');
    if (evt == NULL)
        return false;

    return evt->is_signalled;
}


bool WrapHndClose(handle_t hnd)
{
    handle_hdr_t *hdr;

    hdr = HndRef(NULL, hnd, 0);
    if (hdr == NULL)
        return false;
    else
    {
        HndClose(hdr);
        current()->process->handles[hnd] = NULL;
        return true;
    }
}


/*handle_t WrapMuxCreate(void)
{
    mutex_t *mux;
    mux = MuxCreate();
    return HndDuplicate(&mux->hdr, NULL);
}


bool WrapMuxAcquire(handle_t mux)
{
    mutex_t *hdr;

    hdr = (mutex_t*) HndRef(NULL, mux, 'mutX');
    if (hdr == NULL)
        return false;

    return MuxAcquire(hdr);
}


bool WrapMuxRelease(handle_t mux)
{
    mutex_t *hdr;

    hdr = (mutex_t*) HndRef(NULL, mux, 'mutX');
    if (hdr == NULL)
        return false;

    MuxRelease(hdr);
    return true;
}*/


handle_t WrapSemCreate(int count)
{
    semaphore_t *sem;
    sem = SemCreate(count);
    return HndDuplicate(&sem->hdr, NULL);
}


bool WrapSemUp(handle_t sem)
{
    semaphore_t *hdr;

    hdr = (semaphore_t*) HndRef(NULL, sem, 'sema');
    if (hdr == NULL)
        return false;

    return SemUp(hdr);
}


bool WrapSemDown(handle_t sem)
{
    semaphore_t *hdr;

    hdr = (semaphore_t*) HndRef(NULL, sem, 'sema');
    if (hdr == NULL)
        return false;

    return SemDown(hdr);
}


bool WrapHndExport(handle_t hnd, const wchar_t* name)
{
    handle_hdr_t *hdr;

    hdr = HndRef(NULL, hnd, 0);
    if (hdr == NULL)
        return false;
    else
        return HndExport(hdr, name);
}


handle_t WrapHndOpen(const wchar_t *name)
{
    handle_hdr_t *ret;

    ret = HndOpen(name);
    if (ret == NULL)
        return 0;
    else
        return HndDuplicate(ret, NULL);
}


bool WrapSysShutdown(unsigned type)
{
    wprintf(L"SysShutdown: dismounting file system...");
    FsDismount(L"/");
    wprintf(L"done\n");

    switch (type)
    {
    case SHUTDOWN_REBOOT:
        ArchReboot();
        return true;

    case SHUTDOWN_POWEROFF:
        ArchPowerOff();
        return true;

    default:
        errno = ENOTIMPL;
        return false;
    }
}

void __malloc_leak(int tag);
void __malloc_leak_dump();

void WrapKeLeakBegin(void)
{
    __malloc_leak(1);
    wprintf(L"/");
}

void WrapKeLeakEnd(void)
{
    __malloc_leak_dump();
    wprintf(L"\\");
    __malloc_leak(0);
}

void WrapSysYield(void)
{
    //ScNeedSchedule(true);
}

void WrapKeSetSingleStep(bool set)
{
    context_t *ctx;
    ctx = ThrGetUserContext(current());
    if (set)
        ctx->eflags |= EFLAG_TF;
    else
        ctx->eflags &= ~EFLAG_TF;
}


bool WrapHndSetInheritable(handle_t hnd, bool inheritable)
{
    /*handle_hdr_t *ptr;

    ptr = HndRef(NULL, hnd, 0);
    if (ptr == NULL)
        return false;

    HndAcquire(ptr);

    if (inheritable)
        ptr->flags |= HND_FLAG_INHERITABLE;
    else
        ptr->flags &= ~HND_FLAG_INHERITABLE;

    HndRelease(ptr);
    return true;*/

    return HndSetInheritable(NULL, hnd, inheritable);
}


void WrapProcKillProcess(int exitcode)
{
    ProcExitProcess(exitcode);
}


handle_t WrapProcSpawnProcess(const wchar_t *exe, const struct process_info_t *info)
{
    process_t *ret;

    ret = ProcSpawnProcess(exe, info);
    if (ret == NULL)
        return 0;
    else
        return HndDuplicate(&ret->hdr, NULL);
}


int WrapProcGetExitCode(handle_t ph)
{
    process_t *proc;

    proc = (process_t*) HndRef(NULL, ph, 'proc');
    if (proc == NULL)
        return -1;
    else
        return proc->exit_code;
}


handle_t WrapFsCreate(const wchar_t *name, uint32_t flags)
{
    file_handle_t *ret;
    
    ret = FsCreate(NULL, name, flags);
    if (ret == NULL)
        return 0;
    else
        return HndDuplicate(&ret->hdr, NULL);
}


handle_t WrapFsOpen(const wchar_t *name, uint32_t flags)
{
    file_handle_t *ret;
    
    ret = FsOpen(NULL, name, flags);
    if (ret == NULL)
        return 0;
    else
        return HndDuplicate(&ret->hdr, NULL);
}


status_t WrapFsReadAsync(handle_t file, void *buffer, uint64_t offset, 
                         size_t bytes, fileop_t *op)
{
    file_handle_t *fh;

    fh = (file_handle_t*) HndRef(NULL, file, 'file');
    if (fh == NULL)
        return errno;
    else
        return FsReadAsync(fh, buffer, offset, bytes, op);
}


status_t WrapFsWriteAsync(handle_t file, const void *buffer, uint64_t offset, 
                          size_t bytes, fileop_t *op)
{
    file_handle_t *fh;

    fh = (file_handle_t*) HndRef(NULL, file, 'file');
    if (fh == NULL)
        return errno;
    else
        return FsWriteAsync(fh, buffer, offset, bytes, op);
}


handle_t WrapFsOpenDir(const wchar_t *name)
{
    file_dir_t *dh;

    dh = FsOpenDir(name);
    if (dh == NULL)
        return 0;
    else
        return HndDuplicate(&dh->hdr, NULL);
}


bool WrapFsQueryFile(const wchar_t *name, uint32_t code, void *buffer, size_t size)
{
    return FsQueryFile(name, code, buffer, size);
}


bool WrapFsRequest(handle_t file, uint32_t code, void *buffer, size_t bytes, struct fileop_t* op)
{
    file_handle_t *fh;

    fh = (file_handle_t*) HndRef(NULL, file, 'file');
    if (fh == NULL)
        return false;
    else
        return FsRequest(fh, code, buffer, bytes, op);
}


bool WrapFsIoCtl(handle_t file, uint32_t code, void *buffer, size_t bytes, struct fileop_t* op)
{
    file_handle_t *fh;

    fh = (file_handle_t*) HndRef(NULL, file, 'file');
    if (fh == NULL)
        return false;
    else
        return FsIoCtl(fh, code, buffer, bytes, op);
}


bool WrapFsReadDir(handle_t dir, struct dirent_t *buffer, size_t bytes)
{
    file_dir_t *dh;

    dh = (file_dir_t*) HndRef(NULL, dir, 'fdir');
    if (dh == NULL)
        return false;
    else
        return FsReadDir(dh, buffer, bytes);
}


bool WrapFsQueryHandle(handle_t file, uint32_t code, void *buffer, size_t bytes)
{
    file_handle_t *fh;

    fh = (file_handle_t*) HndRef(NULL, file, 'file');
    if (fh == NULL)
        return false;
    else
        return FsQueryHandle(fh, code, buffer, bytes);
}


bool WrapFsCreateDir(const wchar_t* dir)
{
    return FsCreateDir(dir);
}


bool WrapFsChangeDir(const wchar_t* dir)
{
    return FsChangeDir(dir);
}


bool WrapFsMount(const wchar_t *path, const wchar_t *filesys, const wchar_t *dest)
{
    return FsMount(path, filesys, dest);
}


bool WrapFsDismount(const wchar_t *path)
{
    return FsDismount(path);
}


bool WrapFsCreatePipe(handle_t *hends)
{
    file_handle_t *ends[2];

    if (!FsCreatePipe(ends))
        return false;
    else
    {
        hends[0] = HndDuplicate(&ends[0]->hdr, NULL);
        hends[1] = HndDuplicate(&ends[1]->hdr, NULL);
        return true;
    }
}


void *WrapVmmAlloc(size_t pages, addr_t base, uint32_t flags)
{
    return VmmAlloc(pages, base, flags);
}


bool WrapVmmFree(void *base)
{
    return VmmFree(base);
}


void *WrapVmmMapSharedArea(handle_t desch, addr_t base, uint32_t flags)
{
    vm_desc_t *desc;

    desc = (vm_desc_t*) HndRef(NULL, desch, 'vmmd');
    if (desc == NULL)
        return NULL;
    else
        return VmmMapSharedArea(desc, base, flags);
}


void *WrapVmmMapFile(handle_t file, addr_t base, size_t pages, uint32_t flags)
{
    file_handle_t *fh;

    fh = (file_handle_t*) HndRef(NULL, file, 'file');
    if (fh == NULL)
        return NULL;
    else
        return VmmMapFile(fh, base, pages, flags);
}


void *WrapVmmReserveArea(size_t pages, addr_t base)
{
    return VmmReserveArea(pages, base);
}


handle_t WrapVmmShare(void *base, const wchar_t *name)
{
    vm_desc_t *ret;

    ret = VmmShare(base, name);
    if (ret == NULL)
        return 0;
    else
        return HndDuplicate(&ret->hdr, NULL);
}


handle_t WrapCounterOpen(const wchar_t *name)
{
    counter_handle_t *counter;

    counter = CounterOpen(name);
    if (counter == NULL)
        return 0;
    else
        return HndDuplicate(&counter->hdr, NULL);
}


int WrapCounterSample(handle_t hnd)
{
    counter_handle_t *counter;

    counter = (counter_handle_t*) HndRef(NULL, hnd, 'cntr');
    if (counter == NULL)
        return -1;
    else
        return CounterSample(counter);
}


bool WrapDevInstallIsaDevice(const wchar_t *driver, const wchar_t *device)
{
    return DevInstallIsaDevice(driver, device);
}
