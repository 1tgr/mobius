/* $Id: syscall.c,v 1.18 2002/08/14 16:24:00 pavlovskii Exp $ */
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/arch.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>

/* #define DEBUG */
#include <kernel/debug.h>

#include <os/syscall.h>

#include <errno.h>
#include <wchar.h>
#include <stdio.h>
//#include <malloc.h>

int Hello(int a, int b)
{
    wprintf(L"Hello, world! %d %d\n", a, b);
    return 42;
}

extern uint16_t con_attribs;

int DbgWrite(const wchar_t *str, size_t count)
{
    uint16_t old_attr;
    int ret;
    old_attr = con_attribs;
    con_attribs = ~con_attribs & 0xff00;
    ret = _cputws(str, count);
    con_attribs = old_attr;
    return ret;
}

void ThrExitThread(int code)
{
    wprintf(L"Thread %u exited with code %d\n", current()->id, code);
    ThrDeleteThread(current());
    ScNeedSchedule(true);
}

unsigned SysUpTime(void)
{
    return sc_uptime;
}

bool SysThrWaitHandle(handle_t hnd)
{
    if (ThrWaitHandle(current(), hnd, 0))
    {
	ScNeedSchedule(true);
	return true;
    }
    else
	return false;
}

void SysThrSleep(unsigned ms)
{
    ThrSleep(current(), ms);
}

bool SysGetInfo(sysinfo_t *info)
{
    info->page_size = PAGE_SIZE;
    info->pages_total = pool_all.num_pages + pool_low.num_pages;
    info->pages_free = pool_all.free_pages + pool_low.free_pages;
    info->pages_physical = PAGE_ALIGN_UP(kernel_startup.memory_size) / PAGE_SIZE;
    info->pages_kernel = PAGE_ALIGN_UP(kernel_startup.kernel_data) / PAGE_SIZE;
    return true;
}

bool SysGetTimes(systimes_t *times)
{
    times->quantum = SCHED_QUANTUM;
    times->uptime = sc_uptime;
    times->current_cputime = current()->cputime;
    return true;
}

#ifdef i386
handle_t ThrCreateV86Thread(FARPTR entry, FARPTR stack_top, unsigned priority, void (*handler)(void))
{
    thread_t *thr;
    thr = i386CreateV86Thread(entry, stack_top, priority, handler);
    if (thr != NULL)
	return HndDuplicate(current()->process, &thr->hdr);
    else
	return NULL;
}

bool ThrGetV86Context(context_v86_t* ctx)
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

bool ThrSetV86Context(const context_v86_t* ctx)
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

bool ThrContinueV86(void)
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
handle_t ThrCreateV86Thread(uint32_t entry, uint32_t stack_top, unsigned priority, void (*handler)(void))
{
    errno = ENOTIMPL;
    return NULL;
}
#endif

handle_t SysThrCreateThread(void (*entry)(void), void *param, unsigned priority)
{
    thread_t *thr;
    thr = ThrCreateThread(current()->process, false, entry, true, param, priority);
    if (thr == NULL)
        return NULL;
    else
        return HndDuplicate(current()->process, &thr->hdr);
}

bool SysVmmFree(void *base)
{
    vm_area_t *area;
    area = VmmArea(current()->process, base);
    if (area == NULL)
    {
	errno = ENOTFOUND;
	return false;
    }
    else
    {
	VmmFree(area);
	return true;
    }
}

/*
 * These are the user-mode equivalents of the handle.c event functions.
 * To save one layer of indirection, they patch straight through to the 
 *    HndXXX equivalents.
 */
handle_t SysEvtAlloc(void)
{
    return HndAlloc(NULL, 0, 'evnt');
}

void SysEvtSignal(handle_t evt)
{
    HndSignal(NULL, evt, 0, true);
}

bool SysEvtIsSignalled(handle_t evt)
{
    return HndIsSignalled(NULL, evt, 0);
}

bool SysHndClose(handle_t hnd)
{
    return HndClose(NULL, hnd, 0);
}

bool SysShutdown(unsigned type)
{
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

void KeLeakBegin(void)
{
    __malloc_leak(1);
    wprintf(L"/");
}

void KeLeakEnd(void)
{
    __malloc_leak_dump();
    wprintf(L"\\");
    __malloc_leak(0);
}

void SysYield(void)
{
    ScNeedSchedule(true);
}

void KeSetSingleStep(bool set)
{
    context_t *ctx;
    ctx = ThrGetUserContext(current());
    if (set)
        ctx->eflags |= EFLAG_TF;
    else
        ctx->eflags &= ~EFLAG_TF;
}
