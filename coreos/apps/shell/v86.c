/* $Id: v86.c,v 1.1.1.1 2002/12/21 09:51:10 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include "common.h"

static FARPTR i386LinearToFp(void *ptr)
{
    unsigned seg, off;
    off = (addr_t) ptr & 0xf;
    seg = ((addr_t) ptr - ((addr_t) ptr & 0xf)) / 16;
    return MK_FP(seg, off);
}


static void ShInt21(context_v86_t *ctx)
{
    void *ptr;
    char *ch;
    uint32_t key;
    
    switch ((uint16_t) ctx->regs.eax >> 8)
    {
    case 0:
	ThrExitThread(0);
	break;

    case 1:
        while (!FsRead(ProcGetProcessInfo()->std_in, &key, 0, sizeof(key), NULL))
            ;

	wprintf(L"%c", (uint8_t) key);
	ctx->regs.eax = (ctx->regs.eax & 0xffffff00) | (uint8_t) key;
	break;

    case 2:
	wprintf(L"%c", ctx->regs.edx & 0xff);
	break;

    case 9:
	ptr = FP_TO_LINEAR(ctx->v86_ds, ctx->regs.edx);
	ch = strchr(ptr, '$');
	fwrite(ptr, 1, ch - (char*) ptr, stdout);
	break;

    case 0x4c:
	ThrExitThread(ctx->regs.eax & 0xff);
	break;
    }
}


static void ShV86Handler(void)
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


uint8_t *sh_v86stack;

static void *aligned_alloc(size_t bytes)
{
    return VmmAlloc(PAGE_ALIGN_UP(bytes) / PAGE_SIZE, NULL, VM_MEM_READ | VM_MEM_WRITE);
}


void ShCmdV86(const wchar_t *cmd, wchar_t *params)
{
    uint8_t *code;
    psp_t *psp;
    FARPTR fp_code, fp_stackend;
    handle_t thr, file;
    dirent_standard_t di;
    bool doWait;
    wchar_t **names, **values;

    ShParseParams(params, &names, &values, &params);
    params = ShPrompt(L" .COM file? ", params);
    if (*params == '\0')
        return;

    doWait = true;
    if (ShHasParam(names, L"nowait"))
        doWait = false;
    
    free(names);
    free(values);

    /*if (!FsQueryFile(params, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
        _pwerror(params);
        return;
    }*/
    di.length = 0x10000;

    file = FsOpen(params, FILE_READ);
    if (file == NULL)
    {
        wprintf(L"FsOpen: ");
        _pwerror(params);
        return;
    }

    code = aligned_alloc(di.length + 0x100);
    psp = (psp_t*) code;
    memset(psp, 0, sizeof(*psp));
    psp->int20 = 0x20cd;

    if (!FsRead(file, code + 0x100, 0, di.length, NULL))
    {
        wprintf(L"FsRead: ");
        _pwerror(params);
        HndClose(file);
        return;
    }

    HndClose(file);

    fp_code = i386LinearToFp(code);
    fp_code = MK_FP(FP_SEG(fp_code), FP_OFF(fp_code) + 0x100);
        
    if (sh_v86stack == NULL)
        sh_v86stack = aligned_alloc(65536);

    fp_stackend = i386LinearToFp(sh_v86stack);
    memset(sh_v86stack, 0, 65536);
    
    thr = ThrCreateV86Thread(fp_code, fp_stackend, 15, ShV86Handler, params);
    if (doWait)
        ThrWaitHandle(thr);

    /* xxx - need to clean up HndClose() implementation before we can use this */
    /*HndClose(thr);*/
}
