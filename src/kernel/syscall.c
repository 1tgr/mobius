/* $Id: syscall.c,v 1.7 2002/02/25 18:42:09 pavlovskii Exp $ */
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/arch.h>
#include <kernel/memory.h>

#include <os/syscall.h>

#include <wchar.h>

int Hello(int a, int b)
{
	wprintf(L"Hello, world! %d %d\n", a, b);
	return 42;
}

extern uint16_t con_attribs;
int _cputws(const wchar_t *str, size_t count);

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
	wprintf(L"Thread %u exited with code %d\n", current->id, code);
	ThrDeleteThread(current);
	ScNeedSchedule(true);
}

unsigned SysUpTime(void)
{
	return sc_uptime;
}

bool SysThrWaitHandle(handle_t hnd)
{
	if (ThrWaitHandle(current, hnd, 0))
	{
		ScNeedSchedule(true);
		return true;
	}
	else
		return false;
}

void SysThrSleep(unsigned ms)
{
	ThrSleep(current, ms);
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
	times->current_cputime = current->cputime;
	return true;
}

/*
 * These are the user-mode equivalents of the handle.c event functions.
 * To save one layer of indirection, they patch straight through to the 
 *	HndXXX equivalents.
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