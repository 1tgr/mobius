/* $Id: gdb.c,v 1.2 2003/06/05 21:56:51 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/profile.h>
#include <kernel/debug.h>

#include <wchar.h>

/* Got this from http://www.acm.uiuc.edu/sigops/roll_your_own/2.c.html */

void set_debug_traps(void);
void handle_exception(int exceptionVector);
extern descriptor_int_t arch_idt[];

void (*exceptionHook)(int);

bool dbg_hasgdb;

static spinlock_t spin_serial;

bool DbgInitSerial(void)
{
    uint16_t divisor;
    unsigned speed;

    SpinInit(&spin_serial);

    if (kernel_startup.debug_port != 0)
    {
        SpinAcquire(&spin_serial);

        out(kernel_startup.debug_port + 3, 
            in(kernel_startup.debug_port + 3) | 0x80);

        speed = kernel_startup.debug_speed;
        if (speed == 0)
            speed = 9600;
        divisor = 1843200 / (speed * 16);
        out(kernel_startup.debug_port, divisor % 256);
        out(kernel_startup.debug_port + 1, divisor / 256);

        out(kernel_startup.debug_port + 3, 3);  /* no parity, 8 bits per character */
        SpinRelease(&spin_serial);

        wprintf(L"i386InitSerialDebug: kernel debugger running on port %x\n", 
            kernel_startup.debug_port);
    }

    return true;
}


wchar_t DbgGetCharSerial(void)
{
    if (kernel_startup.debug_port == 0)
        return 0;
    else
    {
        uint8_t ret;

        SpinAcquire(&spin_serial);
        while (!(in(kernel_startup.debug_port + 5) & 0x01))
            ;

        ret = in(kernel_startup.debug_port);
        SpinRelease(&spin_serial);

        return ret;
    }
}


static int putDebugChar(int ch)
{
    if (kernel_startup.debug_port != 0)
    {
        SpinAcquire(&spin_serial);

        if (ch == '\n')
        {
            while ((in(kernel_startup.debug_port + 5) & 0x20) == 0)
                in(0x80);

            out(kernel_startup.debug_port, (char) '\r');
        }

        while ((in(kernel_startup.debug_port + 5) & 0x20) == 0)
            in(0x80);

        out(kernel_startup.debug_port, (char) ch);
        SpinRelease(&spin_serial);
    }

    return 1;
}


int DbgWriteStringSerial(const wchar_t *str, size_t count)
{
    for (; count > 0; str++, count--)
        putDebugChar(*str);

    return 0;
}


void i386TrapToDebugger(const context_t *ctx)
{
}


void exceptionHandler(int exc, void *addr)
{
    /*
     * Page faults are an everyday occurrence -- we don't want to trap into
     *  the debugger every time a page is faulted in.
     * Bochs seems to issue INT1 on every HLT as well.
     */
    if (exc != 14 &&    /* page fault (normal) */
        exc != 13 &&    /* GPF (normal in V86 mode) */
        exc != 8 &&     /* double fault (so abnormal we just crash) */
        exc != 1)       /* debug (seems to get issued on every HLT) */
        i386SetDescriptorInt(arch_idt + exc, 
        KERNEL_FLAT_CODE, 
        (uint32_t) addr, 
        ACS_INT | ACS_DPL_3, 
        ATTR_GRANULARITY | ATTR_BIG);
}

void flush_i_cache(void)
{
   __asm__ __volatile__ ("jmp 1f\n1:");
}
