/* $Id: debug.c,v 1.12 2002/08/14 16:23:59 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/driver.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/fs.h>
#include <kernel/debug.h>
#include <kernel/arch.h>
#include <kernel/proc.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include <os/coff.h>
#include <os/pe.h>

#define KSTACK_BOTTOM	0x80000000
#define KSTACK_TOP		0xe0000000
#define USTACK_BOTTOM	0
#define USTACK_TOP		0xc0000000

thread_t* thr;
addr_t fault_addr;

bool in_debugger;

static bool dbg_exit;

static process_t *DbgFindProcess(unsigned id)
{
    process_t *proc;

    FOREACH (proc, proc)
        if (proc->id == id)
            return proc;

    return NULL;
}

bool DbgLookupSymbol(module_t *mod, void* sym, addr_t* address, SYMENT *syment)
{
	static SYMENT closest;

	IMAGE_DOS_HEADER* dos;
	IMAGE_PE_HEADERS* pe;
	IMAGE_SECTION_HEADER* sections;
	SYMENT symbol;
	unsigned i;
	addr_t addr;
	size_t closest_diff;
	bool found;
	
	dos = (IMAGE_DOS_HEADER*) mod->base;
	pe = (IMAGE_PE_HEADERS*) ((uint32_t) mod->base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	/*symbols = (SYMENT*) ((uint8_t*) rawdata + 
		pe->FileHeader.PointerToSymbolTable);*/

	if (mod->file)
		FsSeek(mod->file, pe->FileHeader.PointerToSymbolTable, FILE_SEEK_SET);
	else
		return false;

	closest_diff = (addr_t) (uint32_t) -1;
	found = false;
	for (i = 0; i < pe->FileHeader.NumberOfSymbols; i++)
	{
		FsReadSync(mod->file, &symbol, sizeof(symbol), NULL);

		if (symbol.e_sclass == C_EXT ||
			symbol.e_sclass == C_STAT || 
			symbol.e_sclass == C_LABEL)
		{
			addr = mod->base + symbol.e_value;
			if (symbol.e_scnum > 0)
				addr += sections[symbol.e_scnum - 1].VirtualAddress;
				
			/*wprintf(L"%d scnum = %d addr = %x\n",  */
				/*i, symbols[i].e_scnum, addr); */

			if ((addr_t) sym >= addr &&
				(addr_t) sym - addr < closest_diff)
			{
				closest = symbol;
				closest_diff = (addr_t) sym - addr;
				found = true;
			}
		}
	}

	if (found && address)
	{
		addr = mod->base + closest.e_value;
		if (closest.e_scnum > 0)
			addr += sections[closest.e_scnum - 1].VirtualAddress;
		*address = addr;
	}

	*syment = closest;
	return found;
}

char* DbgGetStringsTable(module_t *mod)
{
	IMAGE_DOS_HEADER *dos;
	IMAGE_PE_HEADERS *pe;
	IMAGE_SECTION_HEADER* sections;
	SYMENT *symbols;
	int i;

	dos = (IMAGE_DOS_HEADER*) mod->base;
	pe = (IMAGE_PE_HEADERS*) (mod->base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	for (i = 0; i < pe->FileHeader.NumberOfSections; i++)
		if (pe->FileHeader.PointerToSymbolTable >= sections[i].PointerToRawData &&
			pe->FileHeader.PointerToSymbolTable < 
				sections[i].PointerToRawData + sections[i].SizeOfRawData)
		{
			symbols = (SYMENT*) ((char*) mod->base + sections[i].VirtualAddress +
				pe->FileHeader.PointerToSymbolTable - sections[i].PointerToRawData);
			return (char*) (symbols + pe->FileHeader.NumberOfSymbols);
		}

	return NULL;
}

char* DbgGetSymbolName(void* strings, SYMENT* se)
{
	static char temp[9];

	if (se->e.e.e_zeroes == 0)
	{
		if (se->e.e.e_offset)
			return (char*) strings + se->e.e.e_offset;
		else
			return NULL;
	}
	else
	{
		strncpy(temp, se->e.e_name, 8);
		return temp;
	}
}

module_t* DbgLookupModule(process_t* proc, addr_t addr)
{
	module_t* mod;

	for (mod = proc->mod_first; mod; mod = mod->next)
		if (addr >= mod->base && addr < mod->base + mod->length)
			return mod;
	
	if (proc == &proc_idle)
		return NULL;
	else
		return DbgLookupModule(&proc_idle, addr);
}

bool DbgIsValidEsp(process_t* proc, uint32_t _esp)
{
	if ((_esp >= KSTACK_BOTTOM && _esp <= KSTACK_TOP - 4) ||
		_esp >= 0xf0000000 ||
		(proc->stack_end && _esp >= proc->stack_end && _esp < 0x80000000 - 4))
		return true;
	else
		return false;
}

const wchar_t* DbgFormatAddress(uint32_t flags, uint32_t seg, uint32_t ofs)
{
	static wchar_t str[50];
#ifdef i386
	if (flags & EFLAG_VM)
		swprintf(str, L"%04x:%04x", seg, ofs);
	else
#endif
		swprintf(str, L"%08x", ofs);
	return str;
}

void DbgDumpStack(process_t* proc, uint32_t _ebp)
{
	uint32_t *pebp = (uint32_t*) _ebp;
	module_t* mod;
	unsigned i;
	/*SYMENT sym;
	char *strings, *name;
	addr_t addr;*/

	wprintf(L"ebp\t\t\tReturn To\tModule\n");
	i = 0;
	do
	{
		wprintf(L"%08x\t", (uint32_t) pebp);

		if (DbgIsValidEsp(proc, (uint32_t) pebp))
		{
			wprintf(L"%08x\t", pebp[1]);

			if (proc)
			{
				mod = DbgLookupModule(proc, pebp[1]);
				if (mod)
				{
					wprintf(L"%s\n", mod->name);
					/*if (DbgLookupSymbol(mod, (void*) pebp[1], &addr, &sym))
					{
						strings = DbgGetStringsTable(mod);
						name = DbgGetSymbolName(strings, &sym);
						if (name)
							wprintf(L"\t%S + 0x%x", name, pebp[1] - addr);
					}*/
				}
				else
					wprintf(L"(unknown)\n");
			}

			pebp = (uint32_t*) *pebp;
		}
		else
			wprintf(L"(invalid: stack_end = %x)\n", proc->stack_end);
		
		i++;

		if (i >= 20)
		{
			wprintf(L"DbgDumpStack: stopped after %u lines\n", i);
			break;
		}
	} while (DbgIsValidEsp(proc, (uint32_t) pebp));
}

static void DbgCmdSymbol(wchar_t *cmd, wchar_t *params)
{
    addr_t addr;
    module_t *mod;
    char *strings;
    SYMENT syment;

    if (*params == '\0')
    {
        wprintf(L"Please give the address of a symbol\n");
        return;
    }

    addr = wcstoul(params, NULL, 16);
    mod = DbgLookupModule(current()->process, addr);
    if (mod == NULL)
    {
        wprintf(L"%08x: no module found\n", addr);
        return;
    }

    strings = DbgGetStringsTable(mod);
    if (strings == NULL)
    {
        wprintf(L"%s: no strings table present\n", mod->name);
        return;
    }

    if (!DbgLookupSymbol(mod, (void*) addr, NULL, &syment))
    {
        wprintf(L"%08x: no symbol found in module %s\n", addr, mod->name);
        return;
    }

    wprintf(L"%s\n", DbgGetSymbolName(strings, &syment));
}

static void DbgCmdModules(wchar_t *cmd, wchar_t *params)
{
	module_t* mod;
    process_t *proc;

    if (*params == '\0')
        proc = current()->process;
    else
    {
        proc = DbgFindProcess(wcstol(params, NULL, 0));
        if (proc == NULL)
        {
            wprintf(L"%s: unknown process\n", params);
            return;
        }
    }

	wprintf(L"Name%36sBase\t\tLength\n", L"");
	for (mod = proc->mod_first; mod; mod = mod->next)
		wprintf(L"%s%*s%08x\t%08x\n", mod->name, 40 - wcslen(mod->name), L"", mod->base, mod->length);
}

static void DbgCmdProcesses(wchar_t *cmd, wchar_t *params)
{
    process_t *proc;

    FOREACH (proc, proc)
        wprintf(L"%u\t%s\n", proc->id, proc->exe);
}

static void DbgCmdThreads(wchar_t *cmd, wchar_t *params)
{
	thread_t *t;
#ifndef WIN32
	context_t *c;
#endif
    process_t *proc;
    const wchar_t *exe, *addr;

    if (*params == '\0')
    {
        proc = NULL;
        wprintf(L"Listing all threads:\n");
    }
    else
    {
        proc = DbgFindProcess(wcstol(params, NULL, 0));
        if (proc == NULL)
        {
            wprintf(L"%s: unknown process\n", params);
            return;
        }

        wprintf(L"Listing threads for process %u: (%s)\n", proc->id, proc->exe);
    }

	for (t = thr_first; t != NULL; t = t->all_next)
	{
        if (proc == NULL || 
            t->process == proc)
        {
            if (t->process == NULL)
                exe = L"(null)";
            else
                exe = t->process->exe;

#ifdef WIN32
            addr = L"(not available)";
#else
		    c = ThrGetContext(t);

            if ((addr_t) c >= PAGE_SIZE)
                addr = L"(unknown)";
            else
                addr = DbgFormatAddress(c->eflags, c->cs, c->eip);
#endif

		    wprintf(L"%s%*s\t%u\t%s\n",
			    exe, 40 - wcslen(exe), L"",
                t->id, addr);
        }
	}
}

#if 0
void DbgSwitchThreads(thread_t* t, context_t* ctx)
{
	static const wchar_t *msg1[] =
	{
		L"divide error", L"debug exception", L"NMI", L"INT3",
		L"INTO", L"BOUND overrange", L"invalid opcode", L"no coprocessor",
		L"double fault", L"coprocessor segment overrun",
			L"bad TSS", L"segment not present",
		L"stack fault", L"GPF", L"page fault", L"??",
		L"coprocessor error", L"alignment check", L"??", L"??",
		L"??", L"??", L"??", L"??",
		L"??", L"??", L"??", L"??",
		L"??", L"??", L"??", L"??"
	}, *msg2[] =
	{
		L"missing DLL", L"missing import"
	};

	module_t* mod;
	const wchar_t *msg;
	wchar_t temp[20];
	
	thr = t;
	mod = DbgLookupModule(thr->process, ctx->eip);
	if (ctx->eflags & EFLAG_TF && ctx->intr == 1)
		;
		/*wprintf(L"Step: %s (%s)\n", DbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), 
			mod ? mod->name : L"unknown module");*/
	else
	{
		uint32_t code = ctx->intr;
		/*if (code < countof(msg1))
			msg = msg1[code];
		else if (code >= 0x80000000 &&
			code - 0x80000000 < countof(msg2))
			msg = msg2[code - 0x80000000];
		else*/
		{
			swprintf(temp, L"exception %d", code);
			msg = temp;
		}

		wprintf(L"Thread %d: %s at %s (%s): address %08x\n", 
			thr->id, 
			msg, 
			DbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), 
			mod ? mod->name : L"unknown module", 
			fault_addr);
	}
}
#endif

/*wint_t getwchar()
{
	uint32_t key;
	size_t len = sizeof(key);

	if (keyboard == NULL)
		return 0;

	do
	{
		key = 0;

		if (devReadSync(keyboard, 0, &key, &len) != 0)
			return 0;

	} while ((wchar_t) key == 0);
	
	return key;
}

wchar_t* _getws(wchar_t* buffer)
{
	wchar_t *buf, ch;

	buf = buffer;
	do
	{
		ch = getwchar();

		switch (ch)
		{
		case '\n':
			putwchar('\n');
			*buf = 0;
			break;
		case '\b':
			if (buf > buffer)
			{
				buf--;
				wprintf(L"\b \b");
			}
			break;
		default:
			putwchar(ch);
			*buf = ch;
			buf++;
			break;
		}
	} while (ch != '\n');

	return buffer;
}*/

/*! Bugger me! This function's huge! I can't believe the amount of effort  */
/*!	required to extract line & file information from COFF debugging info. */
void DbgLookupLineNumber(addr_t base, void* rawdata, void* symbol, 
						 unsigned* line, char** file)
{
	IMAGE_DOS_HEADER* dos;
	IMAGE_PE_HEADERS* pe;
	IMAGE_SECTION_HEADER* sections;
	unsigned i, j;
	LINENO *line_numbers, *found, *func;
	SYMENT *se, *symbols, *filename;
	AUXENT *aux;
	char *strings;
	
	*line = 0;
	*file = NULL;

	dos = (IMAGE_DOS_HEADER*) base;
	pe = (IMAGE_PE_HEADERS*) ((uint32_t) base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	line_numbers = NULL;
	func = NULL;
	filename = NULL;
	found = NULL;
	for (i = 0; i < pe->FileHeader.NumberOfSections; i++)
	{
		if (strncmp(sections[i].Name, ".text", sizeof(sections[i].Name)) == 0)
		{
			line_numbers = malloc(sizeof(LINENO) * sections[i].NumberOfLinenumbers);
			memcpy(line_numbers, 
				(uint8_t*) rawdata + sections[i].PointerToLinenumbers,
				sizeof(LINENO) * sections[i].NumberOfLinenumbers);
			
			found = NULL;
			func = NULL;
			for (j = 0; j < sections[i].NumberOfLinenumbers; j++)
			{
				if (line_numbers[j].l_lnno)
				{
					/*wprintf(L"line %d\t%x\n", line_numbers[j].l_lnno,  */
						/*line_numbers[j].l_addr.l_paddr); */
				
					if (line_numbers[j].l_addr.l_paddr >= (uint32_t) symbol)
					{
						found = line_numbers + j;
						break;
					}
				}
				else
					func = line_numbers + j;
					/*wprintf(L"function %d\n", line_numbers[j].l_addr.l_symndx); */
			}

			if (found)
				break;
			
			free(line_numbers);
			break;
		}
	}

	if (found)
	{
		*line = found->l_lnno - 1;
		symbols = (SYMENT*) ((uint8_t*) rawdata + 
			pe->FileHeader.PointerToSymbolTable);

		strings = (char*) (symbols + pe->FileHeader.NumberOfSymbols);
		
		se = symbols + func->l_addr.l_symndx;
		if (func)
		{
			aux = NULL;
			for (i = func->l_addr.l_symndx; i < pe->FileHeader.NumberOfSymbols; i++)
			{
				if (symbols[i].e_sclass == C_FCN)
				{
					aux = (AUXENT*) (symbols + i + 1);
					*line += aux->x_sym.x_misc.x_lnsz.x_lnno;
					break;
				}
			}

			if (aux)
			{
				filename = symbols + func->l_addr.l_symndx;
				for (i = 0; i < pe->FileHeader.NumberOfSymbols; i++)
				{
					if (symbols[i].e_sclass == C_FILE)
						filename = symbols + i;

					if (i >= func->l_addr.l_symndx)
						break;
				}
			}
		}

		free(line_numbers);

		if (filename->e_sclass == C_FILE)
		{
			aux = (AUXENT*) (filename + 1);
			if (aux->x_file.x_n.x_zeroes == 0)
			{
				if (aux->x_file.x_n.x_offset)
					*file = (char*) strings + aux->x_file.x_n.x_offset;
				else
					*file = NULL;
			}
			else
				*file = aux->x_file.x_fname;
		}
		else
			*file = DbgGetSymbolName(strings, filename);
	}
}

void DbgDumpBuffer(const void* buf, size_t size)
{
	const uint8_t *ptr;
	unsigned i, j;

	ptr = (const uint8_t*) buf;
	for (j = 0; j < size; j += i, ptr += i)
	{
		wprintf(L"%08x ", (addr_t) ptr);
		for (i = 0; i < 16; i++)
		{
			wprintf(L"%02x ", ptr[i]);
			if (i + j >= size)
				break;
		}

		wprintf(L"\n");
	}
}

static int DbgCompareVmArea(const void *elem1, const void *elem2)
{
	vm_area_t *a1 = *(vm_area_t**) elem1, 
		*a2 = *(vm_area_t**) elem2;

    if (a1->start == a2->start)
        return 0;
    else
        return a1->start > a2->start ? 1 : -1;
}

static void DbgCmdVmm(wchar_t *cmd, wchar_t *params)
{
    process_t *proc;
	vm_area_t *area;
	unsigned count, i;
	vm_area_t **ary;
    const wchar_t *type;

    if (*params == '\0')
        proc = current()->process;
    else
    {
        proc = DbgFindProcess(wcstol(params, NULL, 0));
        if (proc == NULL)
        {
            wprintf(L"%s: unknown process\n", params);
            return;
        }
    }

	wprintf(L"Low memory:\t%uKB\n", (pool_low.free_pages * PAGE_SIZE) / 1024);
	wprintf(L"All memory:\t%uKB\n", (pool_all.free_pages * PAGE_SIZE) / 1024);

	wprintf(L"\tBlock   \tStart   \tEnd     \tType\n");

	count = 0;
	for (area = proc->area_first; area; area = area->next)
		count++;

	ary = malloc(count * sizeof(vm_area_t*));
	assert(ary != NULL);

	for (area = proc->area_first, i = 0; 
		area && i < count; 
		area = area->next, i++)
		ary[i] = area;

	qsort(ary, count, sizeof(vm_area_t*), DbgCompareVmArea);

	for (i = 0; i < count; i++)
    {
        switch (ary[i]->type)
        {
        case VM_AREA_EMPTY:
            type = L"EMPTY";
            break;
        case VM_AREA_NORMAL:
            type = L"NORMAL";
            break;
        case VM_AREA_MAP:
            type = L"MAP";
            break;
        case VM_AREA_SHARED:
            type = L"SHARED";
            break;
        case VM_AREA_FILE:
            type = L"FILE";
            break;
        case VM_AREA_IMAGE:
            type = L"IMAGE";
            break;
        default:
            type = L"?";
            break;
        }

		wprintf(L"%c\t%p\t%08x\t%08x\tVM_AREA_%s\n",
			/*area->is_committed ? 'C' : '_', */
			' ',
			ary[i],
			ary[i]->start,
			ary[i]->start + ary[i]->num_pages * PAGE_SIZE,
			type);
    }

	free(ary);
}

static void DbgCmdExit(wchar_t *cmd, wchar_t *params)
{
    dbg_exit = true;
}

static struct
{
    const wchar_t *str;
    void (*fn)(wchar_t *, wchar_t *);
} dbg_commands[] = 
{
    { L"exit",      DbgCmdExit },
    { L"vmm",       DbgCmdVmm },
    { L"modules",   DbgCmdModules },
    { L"mod",       DbgCmdModules },
    { L"threads",   DbgCmdThreads },
    { L"thr",       DbgCmdThreads },
    { L"processes", DbgCmdProcesses },
    { L"proc",      DbgCmdProcesses },
    { L"symbol",    DbgCmdSymbol },
    { L"sym",       DbgCmdSymbol },
};

void DbgStartShell(void)
{
    wchar_t ch, str[80], *space;
    size_t len;
    unsigned i;

    wprintf(L"Mobius Debugging Shell\n");
    dbg_exit = false;
    while (!dbg_exit)
    {
        wprintf(L"> ");

        len = 0;
        do
        {
            ch = ArchGetKey();
            switch (ch)
            {
            case 0:
                break;

            case '\b':
                if (len > 0)
                {
                    wprintf(L"\b \b");
                    len--;
                }
                break;

            case '\n':
                wprintf(L"%c", ch);
                str[len] = '\0';
                break;

            default:
                if (len < _countof(str) - 1)
                {
                    wprintf(L"%c", ch);
                    str[len++] = ch;
                }
                break;
            }
        } while (ch != '\n');

        space = wcschr(str, ' ');
        if (space == NULL)
            space = str + len;
        else
        {
            *space = '\0';
            space++;
        }

        for (i = 0; i < _countof(dbg_commands); i++)
            if (_wcsicmp(str, dbg_commands[i].str) == 0)
                dbg_commands[i].fn(str, space);
    }
}
