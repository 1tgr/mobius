/* $Id: main.c,v 1.11 2002/02/25 18:42:09 pavlovskii Exp $ */

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
#include <kernel/debug.h>

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

void KernelMain(void)
{
	device_t *dev;
	
	MemInit();
	ArchInit();
	
	TRACE0("ProcInit\n");
	ProcInit();
	TRACE0("RdInit\n");
	RdInit();
	TRACE0("FsInit\n");
	FsInit();

	dev = IoOpenDevice(L"ide0a");
	wprintf(L"FsInit: Mounting ide0a(%p) on /hd using fat\n", dev);
	FsMount(L"/hd", L"fat", dev);

	DevInstallDevice(L"keyboard", L"keyboard", NULL);
	DevInstallDevice(L"tty", L"tty0", NULL);
	DevInstallDevice(L"tty", L"tty1", NULL);
	DevInstallDevice(L"tty", L"tty2", NULL);
	DevInstallDevice(L"tty", L"tty3", NULL);
	DevInstallDevice(L"tty", L"tty4", NULL);
	DevInstallDevice(L"tty", L"tty5", NULL);
	DevInstallDevice(L"tty", L"tty6", NULL);
	DevInstallDevice(L"cmos", L"cmos", NULL);
	
	dev = IoOpenDevice(L"fdc0");
	wprintf(L"KernelMain: Mounting fdc0(%p) on /fd using fat\n", dev);
	FsMount(L"/fd", L"fat", dev);

	/*proc = ProcCreateProcess(SYS_BOOT L"/shell.exe");
	ThrCreateThread(proc, false, (void (*)(void*)) 0xdeadbeef, false, NULL, 16);*/
	ProcSpawnProcess(SYS_BOOT L"/shell.exe", proc_idle.info);
	/*proc_idle.info->std_in = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
	proc_idle.info->std_out = FsOpen(SYS_DEVICES L"/tty1", FILE_WRITE);
	ProcSpawnProcess(SYS_BOOT L"/shell.exe");*/

	ScEnableSwitch(true);

	wprintf(L"Idle\n");
	for (;;)
		ArchProcessorIdle();
}