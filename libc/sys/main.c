/* $Id: main.c,v 1.2 2003/06/05 22:02:13 pavlovskii Exp $ */

#include <stdio.h>
#include <string.h>

#include <os/rtl.h>
#include <os/defs.h>
#include <os/syscall.h>

void __setup_file_rec_list(void);

static bool DbgHandleException(context_t *ctx)
{
    char *path, *file;
    unsigned line;

    _wdprintf(L"libc: exception %ld at %08lx\n",
        ctx->intr, ctx->eip);

    if (DbgLookupLineNumber(ctx->eip, &path, &file, &line))
        _wdprintf(L"%S%S(%hu)\n", path, file, line);

    return ctx->intr == 3;
}

void __libc_exception_handler(void)
{
    static bool nested;
    bool handled;
    context_t *ctx;

    if (nested)
        ProcExitProcess(0);

    nested = true;
    ctx = &ThrGetThreadInfo()->exception_info;
    handled = DbgHandleException(ctx);
    nested = false;

    if (handled)
        __asm__("mov %%ebp,%%esp\n"
            "pop %%ebp\n"
            "jmp *%0" : : "m" (ctx->eip));
    else
        exit(0);
}

void __main(void)
{
    ThrGetThreadInfo()->exception_handler = __libc_exception_handler;

    __get_stdin();
    __get_stdout();
    __get_stderr();
    __setup_file_rec_list();
}
