/* $Id$ */

#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/pe.h>
#include <stddef.h>
#include <printf.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef int (*ENTRY)(void);
typedef bool (*DLLMAIN)(module_info_t*, unsigned);


static wchar_t printf_buffer[80], *ch;

static bool wdprintfhelp(void* pContext, const wchar_t* str, size_t len)
{
    if (ch + len > printf_buffer + _countof(printf_buffer))
    {
		DbgWrite(printf_buffer, ch - printf_buffer);
		ch = printf_buffer;
    }

    memcpy(ch, str, sizeof(wchar_t) * len);
    ch += len;
    return true;
}


int _vwdprintf(const wchar_t* fmt, va_list ptr)
{
    int ret;
    ch = printf_buffer;
    ret = dowprintf(wdprintfhelp, NULL, fmt, ptr);
    if (ch > printf_buffer)
		DbgWrite(printf_buffer, ch - printf_buffer);
    return ret;
}


int _wdprintf(const wchar_t* fmt, ...)
{
    va_list ptr;
    int ret;

    va_start(ptr, fmt);
    ret = _vwdprintf(fmt, ptr);
    va_end(ptr);
    return ret;
}


static const IMAGE_PE_HEADERS *ProcGetPeHeader(addr_t base)
{
	const IMAGE_DOS_HEADER* dos_head;
	dos_head = (const IMAGE_DOS_HEADER*) base;
    return (const IMAGE_PE_HEADERS*) ((char *) dos_head + dos_head->e_lfanew);
}


static bool ProcCallDllMains(unsigned reason, bool forwards)
{
	DLLMAIN dllmain;
	process_info_t *proc;
	module_info_t *mod;
    const IMAGE_PE_HEADERS* header;

	proc = ProcGetProcessInfo();

	if (forwards)
		mod = proc->module_last;
	else
		mod = proc->module_first;

	while (mod != NULL)
	{
		if (mod->base != proc->base)
		{
			header = ProcGetPeHeader(mod->base);
			dllmain = (DLLMAIN) (mod->base + header->OptionalHeader.AddressOfEntryPoint);

			if (!dllmain(mod, reason))
				return false;
		}

		if (forwards)
			mod = mod->prev;
		else
			mod = mod->next;
	}

	return true;
}


void ThrExitThread(int exitcode)
{
	ProcCallDllMains(DLLMAIN_THREAD_EXIT, false);
	ThrKillThread(exitcode);
	for (;;)
		;
}


void ProcExitProcess(int exitcode)
{
	ProcCallDllMains(DLLMAIN_PROCESS_EXIT, false);
	ProcKillProcess(exitcode);
	for (;;)
		;
}


void ThrStartup(void)
{
	thread_info_t *thr;

	if (!ProcCallDllMains(DLLMAIN_THREAD_STARTUP, true))
		ThrExitThread(-1);

	thr = ThrGetThreadInfo();
	ThrExitThread(thr->entry(thr->param));
}


void ProcStartup(void)
{
	ENTRY entry;
	process_info_t *proc;
	const IMAGE_PE_HEADERS* header;

	proc = ProcGetProcessInfo();
	proc->thread_entry = ThrStartup;

	if (!ProcCallDllMains(DLLMAIN_PROCESS_STARTUP, true))
		ProcExitProcess(-1);

	header = ProcGetPeHeader(proc->base);
	entry = (ENTRY) (proc->base + header->OptionalHeader.AddressOfEntryPoint);
	ProcExitProcess(entry());
}
