#include <string.h>
#include <stdlib.h>
#include <kernel/kernel.h>
#include <kernel/sys.h>
#include <kernel/driver.h>
#include <kernel/hash.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <errno.h>

hashtable_t* sys_mtab;
extern thread_t thr_idle;
extern process_t proc_idle;

void msleep(dword ms)
{
	dword end = sysUpTime() + ms;
	enable();
	while (sysUpTime() < end)
		;
}

void* sysOpen(const wchar_t* filespec)
{
	hashelem_t* elem = hashFind(sys_mtab, filespec);

	if (elem)
		return elem->data;
	else
		return NULL;
}

void sysMount(const wchar_t* path, void* ptr)
{
	hashelem_t elem, *f;

	f = hashFind(sys_mtab, path);

	// hashInsert() doesn't copy elem.str if already found
	if (f)
		elem.str = path;
	else
		elem.str = wcsdup(path);
		
	elem.data = ptr;
	hashInsert(sys_mtab, &elem);
}

dword i386_docall(void* addr, void* params, size_t sizeof_params);

dword sysInvoke(void* ptr, dword method, dword* params, size_t sizeof_params)
{
	dword *vtbl;//, ret;
	dword (*func) (void*, dword, dword, dword);
	//static dword temp[64];

	//wprintf(L"sysInvoke(%p, %d, %p, %d)\n", ptr, method, params, sizeof_params);
	
	if (ptr == NULL)
	{
		//_cputws(L"sysInvoke: ptr == NULL\n");
		errno = EINVALID;
		return 0;
	}

	vtbl = *((dword**) ptr);
	if (vtbl == NULL)
	{
		errno = EINVALID;
		return 0;
	}

	//wprintf(L"func = %p\n", vtbl[method]);
	//asm("cli ; hlt");
	func = (dword (*) (void*, dword, dword, dword)) vtbl[method];
	return func(ptr, params[0], params[1], params[2]);

	//temp[0] = (dword) ptr;
	//memcpy(temp + 1, params, sizeof_params);
	//ret = i386_docall((void*) vtbl[method], temp, sizeof_params + sizeof(dword));
	//memcpy(params, temp + 1, sizeof_params);
	//return ret;
}

dword sysUpTime()
{
	return uptime;
}

void sysYield()
{
	asm("mov $2,%eax ; int $0x30");
}

void INIT_CODE sysInit()
{
	sys_mtab = hashCreate(31);
}

void devCleanup();

bool keShutdown(dword flags)
{
	word idtr[3] = { 0, 0, 0 };
	
	while (thr_last != &thr_idle)
	{
		wprintf(L"Shutting down process %d...", thr_last->process->id);
		procTerminate(thr_last->process);
		_cputws(L"done\n");
	}

	//devCleanup();
	//procDelete(&proc_idle);

	switch (flags & 1)
	{
	case SHUTDOWN_REBOOT:
		asm("lidt	(%0); int	$0" : : "p" (idtr));
		/* reboot failed? */
		return false;
	case SHUTDOWN_POWEROFF:
		halt(flags);
		return false;
	}

	return true;
}
