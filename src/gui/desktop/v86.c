/* $Id: v86.c,v 1.1 2002/04/03 23:26:44 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>

#include <stdio.h>
#include <string.h>

#include "v86.h"

FARPTR i386LinearToFp(void *ptr)
{
    unsigned seg, off;
    off = (addr_t) ptr & 0xf;
    seg = ((addr_t) ptr - ((addr_t) ptr & 0xf)) / 16;
    return MK_FP(seg, off);
}

char *sbrk(size_t diff);

void ShInt21(context_v86_t *ctx)
{
    void *ptr;
    char *ch;
    uint8_t b;
    
    switch ((uint16_t) ctx->regs.eax >> 8)
    {
    case 0:
	ThrExitThread(0);
	break;

    case 1:
	do
	{
	    b = (uint8_t) ConReadKey();
	} while (b == 0);

	wprintf(L"%c", b);
	ctx->regs.eax = (ctx->regs.eax & 0xffffff00) | b;
	break;

    case 2:
	wprintf(L"%c", ctx->regs.edx & 0xff);
	break;

    case 9:
	ptr = FP_TO_LINEAR(ctx->v86_ds, ctx->regs.edx);
	ch = strchr(ptr, '$');
	_cputs(ptr, ch - (char*) ptr);
	break;

    case 0x4c:
	ThrExitThread(ctx->regs.eax & 0xff);
	break;
    }
}

void ShV86Handler(void)
{
    context_v86_t ctx;
    uint8_t *ip;

    ThrGetV86Context(&ctx);
    
    ip = FP_TO_LINEAR(ctx.cs, ctx.eip);
    switch (ip[0])
    {
    case 0xcd:
	switch (ip[1])
	{
	case 0x20:
	    ThrExitThread(0);
	    break;

	case 0x21:
	    ShInt21(&ctx);
	    break;
	}

	ctx.eip += 2;
	break;

    default:
	wprintf(L"v86: illegal instruction %x\n", ip[0]);
	ThrExitThread(0);
	break;
    }

    ThrSetV86Context(&ctx);
    ThrContinueV86();
}
