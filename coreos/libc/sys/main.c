/* $Id: main.c,v 1.2 2003/06/05 22:02:13 pavlovskii Exp $ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libc/local.h>
#include "init.h"

#include <os/rtl.h>
#include <os/defs.h>
#include <os/syscall.h>

static module_info_t* DbgLookupModule(addr_t addr)
{
    module_info_t* mod;

    for (mod = ProcGetProcessInfo()->module_first; mod != NULL; mod = mod->next)
	if (addr >= mod->base && addr < mod->base + mod->length)
	    return mod;

    return NULL;
}


static bool DbgIsValidEsp(const void *esp)
{
    return ((addr_t) esp < 0x80000000 - 4) && ((addr_t) esp >= 0x1000);
}


static const char *DbgFormatAddress(uint32_t flags, uint32_t segment, uint32_t offset)
{
    static char str[9];
    sprintf(str, "%08lx", offset);
    return str;
}


static void DbgDumpStack(const context_t *ctx)
{
    uint32_t *pebp = (uint32_t*) ctx->regs.ebp;
    char *path, *file;
    unsigned line;

    printf("ebp\t\t\tReturn\tParameters\n");
    if (DbgLookupLineNumber(ctx->eip, &path, &file, &line))
	printf("\t%s%s(%hu)\n", path, file, line);
    else
    {
	module_info_t* mod;

	mod = DbgLookupModule(ctx->eip);
	if (mod == NULL)
	    printf("\t(unknown)\n");
	else
	    wprintf(L"\t%s\n", mod->name);
    }

    do
    {
	printf("%p\t", pebp);

	if (DbgIsValidEsp(pebp))
	{
	    printf("%s (",
		DbgIsValidEsp(pebp + 1) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[1]) : "????????");
	    printf("%s, ",
		DbgIsValidEsp(pebp + 2) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[2]) : "????????");
	    printf("%s, ", 
		DbgIsValidEsp(pebp + 3) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[3]) : "????????");
	    printf("%s)\n", 
		DbgIsValidEsp(pebp + 4) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[4]) : "????????");

	    if (DbgLookupLineNumber(pebp[1], &path, &file, &line))
		printf("\t%s%s(%hu)\n", path, file, line);
	    else
	    {
		module_info_t* mod;

		mod = DbgLookupModule(pebp[1]);
		if (mod == NULL)
		    printf("\t(unknown)\n");
		else
		    wprintf(L"\t%s\n", mod->name);
	    }

	    pebp = (uint32_t*) *pebp;
	}
	else
	    printf("(invalid)\n");
    } while (DbgIsValidEsp(pebp));
}


static bool DbgHandleException(context_t *ctx)
{
    printf("libc: exception %ld at %08lx\n",
	ctx->intr, ctx->eip);
    _wdprintf(L"libc: exception %ld at %08lx\n",
	ctx->intr, ctx->eip);
    DbgDumpStack(ctx);
    return false;
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
    ThrContinue(ctx);
    else
	exit(0);
}


void __main(void)
{
}


void __libc_init(void)
{
    extern void (*__CTOR_LIST__[])();
    void (**pfunc)() = __CTOR_LIST__;

    if (ProcGetProcessInfo()->std_out != 0)
	ThrGetThreadInfo()->exception_handler = __libc_exception_handler;

    __malloc_lock_init();
    __malloc_debug_init();
    atexit(__malloc_lock_cleanup);
    __get_stdin();
    __get_stdout();
    __get_stderr();
    __setup_file_rec_list();

    while (*++pfunc)
	;
    while (--pfunc > __CTOR_LIST__)
	(*pfunc) ();
}
