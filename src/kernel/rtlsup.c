/* $Id: rtlsup.c,v 1.15 2002/05/19 13:04:59 pavlovskii Exp $ */

#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/sched.h>

#include <os/rtl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>

#include <libc/local.h>
#include <libc/file.h>
#include <libc/stdiohk.h>

addr_t kernel_sbrk = 0xe0000000;
unsigned con_x, con_y;
uint16_t con_attribs = 0x0700;

extern uint16_t dbg_combase;
int putDebugChar(int ch);

/*wchar_t __uc_map_file[] = L"/System/Boot/unicode.dat";*/

#define VGA_AC_INDEX        0x3C0
#define VGA_AC_WRITE        0x3C0
#define VGA_AC_READ         0x3C1
#define VGA_MISC_WRITE      0x3C2
#define VGA_SEQ_INDEX       0x3C4
#define VGA_SEQ_DATA        0x3C5
#define VGA_PALETTE_MASK    0x3C6
#define VGA_DAC_READ_INDEX  0x3C7
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA        0x3C9
#define VGA_MISC_READ       0x3CC
#define VGA_GC_INDEX        0x3CE
#define VGA_GC_DATA         0x3CF
#define VGA_CRTC_INDEX      0x3D4
#define VGA_CRTC_DATA       0x3D5
#define VGA_INSTAT_READ     0x3DA

#if 0
static void TextUpdateCursor(void)
{
    unsigned short Off;

    Off = (con_y * 80 + con_x) * 2;
    /* extra shift because even/odd text mode uses word clocking */
    out(VGA_CRTC_INDEX, 14);
    out(VGA_CRTC_DATA, Off >> 9);
    out(VGA_CRTC_INDEX, 15);
    out(VGA_CRTC_DATA, Off >> 1);
}
#endif

void TextSwitchToKernel(void)
{
    out(VGA_CRTC_INDEX, 12);
    out(VGA_CRTC_DATA, 0);
    out(VGA_CRTC_INDEX, 13);
    out(VGA_CRTC_DATA, 0);
    /*TextUpdateCursor();*/
}

void TextUpdateProgress(int min, int progress, int max)
{
    uint16_t *mem;
    unsigned chars, i;

    if (min == max)
        chars = 0;
    else
        chars = ((progress - min) * 80) / (max - min);

    mem = (uint16_t*) PHYSICAL(0xb8000) + 80 * 24;
    for (i = 0; i < chars; i++)
        mem[i] = 0x71fe;
    for (; i < 80; i++)
        mem[i] = 0x7120;
}

void __dj_assert(const char *test, const char *file, int line)
{
    TextSwitchToKernel();
    wprintf(L"Assertion failed: %S, line %d\n"
        L"%S\n",
        file, line, test);
    ScEnableSwitch(false);
    __asm__("int3");
    enable();
    for (;;);
    /*__asm__("cli;hlt");*/
}

#define DEFINE_PUTS(name, ct) \
int name(const ct *str, size_t count) \
{ \
    uint16_t *mem = (uint16_t*) PHYSICAL(0xb8000); \
\
    for (; *str && count > 0; count--, str++) \
    { \
        if (dbg_combase != 0) \
        { \
            if (*str == '\n') \
            putDebugChar('\r'); \
            else \
            putDebugChar(*str); \
        } \
\
        switch (*str) \
        { \
        case '\n': \
            con_y++; \
        case '\r': \
            con_x = 0; \
            break; \
 \
        case '\b': \
            if (con_x > 0) \
                con_x--; \
            break; \
 \
        case '\t': \
            con_x = (con_x + 4) & ~3; \
            break; \
 \
        default: \
            mem[con_x + con_y * 80] =  \
                (uint16_t) (uint8_t) *str | con_attribs; \
            con_x++; \
        } \
 \
        if (con_x >= 80) \
        { \
            con_x = 0; \
            con_y++; \
        } \
 \
        while (con_y >= 24) \
        { \
            unsigned i; \
 \
            memmove(mem, mem + 80, 80 * 23 * 2); \
            for (i = 0; i < 80; i++) \
                mem[80 * 23 + i] = ' ' | con_attribs; \
 \
            con_y--; \
        } \
    } \
 \
    /*TextUpdateCursor();*/ \
    return 0; \
}

DEFINE_PUTS(_cputws, wchar_t);
DEFINE_PUTS(_cputs, char);

wchar_t *ProcGetCwd()
{
    return current->process->info->cwd;
}

void *__morecore(size_t diff)
{
    addr_t new_sbrk, start, phys;

    diff = PAGE_ALIGN_UP(diff);
    assert(diff < 1048576);
    if (kernel_sbrk + diff >= 0xf0000000)
        return NULL;

    start = kernel_sbrk;
    new_sbrk = kernel_sbrk + diff;

    wprintf(L"sbrk: getting %d bytes at %lx", diff, start);
    for (; kernel_sbrk < new_sbrk; kernel_sbrk += PAGE_SIZE)
    {
        phys = MemAlloc();
        if (phys == NULL)
            return (char*) -1;

        /*wprintf(L"%lx=>%lx ", kernel_sbrk, phys);*/
        _cputws(L".", 1);
        if (!MemMap(kernel_sbrk, phys, kernel_sbrk + PAGE_SIZE, 
            PRIV_WR | PRIV_KERN | PRIV_PRES))
            return NULL;
    }

    wprintf(L"done\n");
    return (void*) start;
}

void *sbrk_virtual(size_t diff)
{
    addr_t start;

    diff = PAGE_ALIGN_UP(diff);
    if (kernel_sbrk + diff >= 0xf0000000)
        return NULL;

    start = kernel_sbrk;
    kernel_sbrk += diff;
    return (void*) start;
}

int *_geterrno()
{
    return &current->info->status;
}

/*FILE *__get_stdin(void)
{
    return NULL;
}

FILE *__get_stdout(void)
{
    return NULL;
}*/

/*
 * Using the default libc stdin.c, stdout.c and stderr.c causes all kinds of 
 *  grief.
 */
FILE __dj_stdin = {
  0, 0, 0, 0,
  _IOREAD | _IOLBF,
  0
};

FILE __dj_stdout = {
  0, 0, 0, 0,
  _IOWRT | _IOFBF,
  0
};

FILE __dj_stderr = {
  0, 0, 0, 0,
  _IOWRT | _IONBF,
  0
};

FILE *__get_stderr(void)
{
    return &__dj_stderr;
}

__file_rec *__file_rec_list = NULL;

void __setup_file_rec_list(void)
{
}

ssize_t _write(int fd, const void *buf, size_t nbyte)
{
    if (fd == 0)
        _cputs(buf, nbyte);

    return nbyte;
}

const __wchar_info_t *__lookup_unicode(wchar_t cp)
{
    return NULL;
}
