/* $Id: main.c,v 1.6 2002/01/15 00:13:06 pavlovskii Exp $ */

/*!
 *	\defgroup	kernel	Kernel
 */

#include <os/syscall.h>

#include <kernel/kernel.h>
#include <kernel/sched.h>
#include <kernel/thread.h>
#include <kernel/driver.h>
#include <kernel/io.h>
#include <kernel/fs.h>
#include <kernel/arch.h>
#include <kernel/proc.h>

#include <kernel/init.h>
#include <stdio.h>

extern char scode[];
extern unsigned sc_uptime;
extern process_t proc_idle;

void SemInit(semaphore_t *sem)
{
	sem->locks = 0;
	sem->owner = NULL;
}

/*
typedef struct
{
	const wchar_t *name;
	unsigned wait;
} kti;

kti i[3] =
{
	{ L"Thread A", 5000 },
	{ L"Thread B", 2000 },
	{ L"Thread C", 10000 }
};

void KernelThread(void *param)
{
	kti *i = param;
	for (;;)
	{
		wprintf(L"%u Hello, %s!\n", sc_uptime, i->name);
		ThrSleep(current, i->wait);
		ArchProcessorIdle();
	}
}
*/

void KernelMain(void)
{
	process_t *proc;
	device_t *dev;
	
	MemInit();
	ArchInit();

	wprintf(L"ProcInit\n");
	ProcInit();
	wprintf(L"RdInit\n");
	RdInit();
	wprintf(L"FsInit\n");
	FsInit();

	/*wprintf(L"ThrCreateThread\n");
	ThrCreateThread(&proc_idle, true, KernelThread, true, i + 0, 16);
	ThrCreateThread(&proc_idle, true, KernelThread, true, i + 1, 16);
	ThrCreateThread(&proc_idle, true, KernelThread, true, i + 2, 16);*/

	/*proc = ProcCreateProcess(SYS_BOOT L"/console.exe");
	ThrCreateThread(proc, false, (void (*)(void*)) 0xdeadbeef, false, NULL, 16);*/

	dev = IoOpenDevice(L"fdc0");
	wprintf(L"KernelMain: Mounting fdc0(%p) on /hd using fat\n", dev);
	FsMount(L"/hd", L"fat", dev);

	DevInstallDevice(L"tty", L"tty0", NULL);
	DevInstallDevice(L"keyboard", L"keyboard", NULL);
	
	proc = ProcCreateProcess(SYS_BOOT L"/test.exe");
	ThrCreateThread(proc, false, (void (*)(void*)) 0xdeadbeef, false, NULL, 16);

	wprintf(L"Kernel ready\n");

	wprintf(L"ScEnableSwitch\n");
	ScEnableSwitch(true);

	wprintf(L"Idle\n");
	for (;;)
		ArchProcessorIdle();
}