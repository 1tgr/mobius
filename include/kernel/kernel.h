#ifndef __KERNEL_H
#define __KERNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef DLLIMPORT
#define DLLIMPORT
#endif

#include <kernel/i386.h>
//#include <os/com.h>
#include <os/os.h>

/*!
 *	\defgroup	kernel	Kernel
 *	@{
 */

typedef struct sysinfo_t sysinfo_t;

//! Global system information block
struct sysinfo_t
{
	addr_t kernel_phys, ramdisk_phys;
	size_t kernel_size, ramdisk_size;
	addr_t memory_top;
	size_t kernel_data;
};

typedef struct irq_t irq_t;

//! Describes information about an IRQ handler
struct irq_t
{
	irq_t *prev, *next;
	struct device_t* dev;
};

extern sysinfo_t DLLIMPORT _sysinfo;
extern irq_t DLLIMPORT *irq_first[16], *irq_last[16];
volatile extern dword DLLIMPORT uptime;

struct thread_t;
void exception(struct thread_t* thr, context_t* ctx, dword code, dword address);
int _cputws(const wchar_t* str);

#define CHECK_L2	L"\t\04 "
void _cputws_check(const wchar_t* str);
int wprintf(const wchar_t* fmt, ...);
void syscall(context_t* ctx);
bool dbgInvoke(struct thread_t* thr, struct context_t* ctx, addr_t address);

enum PrintfMode { modeBlue, modeConsole };
void conSetMode(enum PrintfMode mode);

struct textwin_t;
extern struct textwin_t spew, checks, *wprintf_window;

//! Asserts that the specified condition is true.
/*!
 *	If the condition is false, the macro calls the assert_fail() function to
 *		alert the programmer.
 *	\param	x	The condition to test
 */
#define assert(x)	if (!(x)) assert_fail(__FILE__, __LINE__, L#x);
#define CONCAT_(a, b)	a##b
#define CONCAT(a, b)	CONCAT_(a, b)
#define CASSERT(exp)	static int CONCAT(v, __LINE__)[(exp) ? 1 : -1] = { 0 }
#define offsetof(t, f)	((addr_t) &((t*) NULL)->f)

void assert_fail(const char* file, int line, const wchar_t* exp);

#ifndef _MSC_VER
//#define IN_SECTION(s)	__attribute__((section (s)))
#define IN_SECTION(s)	
#define INIT_CODE		IN_SECTION(".init")
#define INIT_DATA		IN_SECTION(".init")
#endif

//@}

#ifdef __cplusplus
}
#endif

#endif