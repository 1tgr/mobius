/* $Id: scdsptch.c,v 1.1.1.1 2002/12/21 09:49:34 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/i386.h>
#include <kernel/memory.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <kernel/syscall.h>

#undef SYS_BEGIN_GROUP
#define SYS_BEGIN_GROUP(n)	systab_t i386_sys##n[256] = {

#undef SYSCALL
#define SYSCALL(rtn, name, argbytes, args...) \
	{ SYS_##name, #name, Wrap##name, argbytes },

#undef SYS_END_GROUP
#define SYS_END_GROUP(n)	};

#include <os/sysdef.h>

#undef SYS_BEGIN_GROUP
#define SYS_BEGIN_GROUP(n)	i386_sys##n,

#undef SYSCALL
#define SYSCALL(rtn, name, argbytes, args...)

#undef SYS_END_GROUP
#define SYS_END_GROUP(n)

static spinlock_t i386_spin_systab;
static systab_t *i386_systab0[] = 
{
#include <os/sysdef.h>
};

systab_t **i386_systab[2] = { i386_systab0, };
unsigned i386_systab_range[2] = { _countof(i386_systab0), };

uint32_t i386DispatchSysCall(uint32_t number, void *args, size_t arg_bytes)
{
	unsigned prefix, tab, code;
	systab_t *table;

	assert(MemVerifyBuffer(args, arg_bytes));

	prefix = (number >> 12) & 0xf;
	tab = (number >> 8) & 0xf;
	code = number & 0xff;

	if (prefix > _countof(i386_systab_range))
	{
		wprintf(L"syscall: invalid prefix %x\n", prefix);
		return 0;
	}

	if (tab > i386_systab_range[prefix])
	{
		wprintf(L"syscall: table %x out of range for prefix %x\n", tab, prefix);
		return 0;
	}

	table = i386_systab[prefix][tab] + code;
	if (table->code != number)
	{
		wprintf(L"table->code = %x, eax = %x\n", table->code, number);
		assert(table->code == number);
	}

	assert(table->argbytes == arg_bytes);

	return i386DoCall(table->routine, args, arg_bytes);
}

void *KeGetSysCall(unsigned id)
{
    unsigned prefix, tab, code;
    prefix = (id >> 12) & 0xf;
    tab = (id >> 8) & 0xf;
    code = id & 0xff;
    if (prefix > _countof(i386_systab) ||
        tab > i386_systab_range[prefix] || 
        i386_systab[prefix][tab][code].code != id)
    {
        errno = ENOTFOUND;
        return NULL;
    }

    return i386_systab[prefix][tab][code].routine;
}

void *KeSetSysCall(unsigned id, void *ptr)
{
    unsigned prefix, tab, code;
    void *old;

    old = KeGetSysCall(id);
    if (ptr == NULL)
        return old;

    prefix = (id >> 12) & 0xf;
    tab = (id >> 8) & 0xf;
    code = id & 0xff;
    if (prefix > _countof(i386_systab) ||
        tab > i386_systab_range[prefix] || 
        i386_systab[prefix][tab][code].code != id)
    {
        errno = EINVALID;
        return NULL;
    }

    i386_systab[prefix][tab][code].routine = ptr;
    return old;
}

void KeInstallSysCallGroup(uint8_t prefix, systab_t **group, unsigned count)
{
    if (prefix != 1)
        return;

    SpinAcquire(&i386_spin_systab);
    i386_systab[prefix] = group;
    i386_systab_range[prefix] = count;
    SpinRelease(&i386_spin_systab);
}
