/* $Id: syscall.c,v 1.5 2002/02/22 15:31:27 pavlovskii Exp $ */
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/arch.h>

#include <os/syscall.h>
#include <os/rtl.h>

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

handle_t ProcSpawnProcess(const wchar_t *exe)
{
	process_t *proc;
	thread_t *thr;
	context_t *ctx;
	wchar_t temp[MAX_PATH];
	
	FsFullPath(exe, temp);
	proc = ProcCreateProcess(temp);
	if (proc == NULL)
		return NULL;

	thr = ThrCreateThread(proc, false, (void (*)(void*)) 0xdeadbeef, false, NULL, 16);
	ctx = ThrGetContext(thr);
	/*ctx->eflags |= EFLAG_TF;*/
	ScNeedSchedule(true);
	return HndDuplicate(current->process, &proc->hdr);
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