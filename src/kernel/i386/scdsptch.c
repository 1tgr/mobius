/* $Id: scdsptch.c,v 1.5 2002/08/06 11:02:57 pavlovskii Exp $ */

#include <kernel/i386.h>
#include <kernel/memory.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <os/syscall.h>

typedef struct systab_t systab_t;
struct systab_t
{
	uint32_t code;
	const char *name;
	void *routine;
	uint32_t argbytes;
};

#undef SYS_BEGIN_GROUP
#define SYS_BEGIN_GROUP(n)	systab_t i386_sys##n[256] = {

#undef SYSCALL
#define SYSCALL(rtn, name, argbytes, args...) \
	{ SYS_##name, #name, name, argbytes },

#undef SYS_END_GROUP
#define SYS_END_GROUP(n)	};

#include <os/sysdef.h>

#undef SYS_BEGIN_GROUP
#define SYS_BEGIN_GROUP(n)	i386_sys##n,

#undef SYSCALL
#define SYSCALL(rtn, name, argbytes, args...)

#undef SYS_END_GROUP
#define SYS_END_GROUP(n)

systab_t *i386_systab[] = 
{
#include <os/sysdef.h>
};

void i386DispatchSysCall(context_t *ctx)
{
	unsigned tab, code;

	assert(MemVerifyBuffer((void*) ctx->regs.edx, ctx->regs.ecx));

	tab = (ctx->regs.eax >> 8) & 0xf;
	code = ctx->regs.eax & 0xff;
	if (tab > _countof(i386_systab))
	{
		wprintf(L"syscall: table %x out of range\n", tab);
		return;
	}

	assert(i386_systab[tab][code].code == ctx->regs.eax);
	assert(i386_systab[tab][code].argbytes == ctx->regs.ecx);

	ctx->regs.eax = i386DoCall(i386_systab[tab][code].routine,
		(void*) ctx->regs.edx, i386_systab[tab][code].argbytes);
}

void *KeGetSysCall(unsigned id)
{
    unsigned tab, code;
    tab = (id >> 8) & 0xf;
	code = id & 0xff;
	if (tab > _countof(i386_systab) || 
        i386_systab[tab][code].code != id)
	{
        errno = ENOTFOUND;
		return NULL;
	}

	return i386_systab[tab][code].routine;
}

void *KeSetSysCall(unsigned id, void *ptr)
{
    unsigned tab, code;
    void *old;

    old = KeGetSysCall(id);
    if (ptr == NULL)
        return old;

    tab = (id >> 8) & 0xf;
	code = id & 0xff;
	if (tab > _countof(i386_systab) || 
        i386_systab[tab][code].code != id)
	{
        errno = EINVALID;
		return NULL;
	}

    i386_systab[tab][code].routine = ptr;
	return old;
}
