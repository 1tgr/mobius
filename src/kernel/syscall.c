/* $Id: syscall.c,v 1.2 2001/11/05 22:41:06 pavlovskii Exp $ */

#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>

#include <os/syscall.h>

#include <wchar.h>

#undef ThrWaitHandle
#undef ThrSleep

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
	wprintf(L"Thread %u exited with code %d\n", current->id, code);
	ThrDeleteThread(current);
	ScNeedSchedule(true);
}

unsigned SysUpTime(void)
{
	return sc_uptime;
}

void ProcExitProcess(int code)
{
	wprintf(L"Process %u exited with code %d\n", current->process->id, code);
	ProcDeleteProcess(current->process);
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