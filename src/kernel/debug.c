/* $Id: debug.c,v 1.2 2001/11/05 22:41:06 pavlovskii Exp $ */

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

/*!
 *  \ingroup	drivers
 *  \defgroup	kdebug	Kernel debugger
 *  @{
 */

#define KSTACK_BOTTOM	0xc0000000
#define KSTACK_TOP		0xd0000000
#define USTACK_BOTTOM	0
#define USTACK_TOP		0xc0000000

/*static const wchar_t* thread_states[] =
{
	L"THREAD_RUNNING",
	L"THREAD_DELETED",
	L"THREAD_WAIT_TIMEOUT",
	L"THREAD_WAIT_HANDLE",
	L"THREAD_WAIT_OBJECT",
};

static const wchar_t* commands[] =
{
	L"help",
	L"ctx",
	L"stk",
	L"mods",
	L"threads",
	L"thread",
	L"t",
	L"go",
	L"quit",
	L"vmm",
};*/

thread_t* thr;
addr_t fault_addr;

bool in_debugger;

bool DbgLookupSymbol(module_t *mod, void* sym, addr_t* address, SYMENT *syment)
{
	static SYMENT closest;

	IMAGE_DOS_HEADER* dos;
	IMAGE_PE_HEADERS* pe;
	IMAGE_SECTION_HEADER* sections;
	SYMENT symbol;
	int i;
	addr_t addr;
	size_t closest_diff;
	bool found;
	
	dos = (IMAGE_DOS_HEADER*) mod->base;
	pe = (IMAGE_PE_HEADERS*) ((uint32_t) mod->base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	/*symbols = (SYMENT*) ((uint8_t*) rawdata + 
		pe->FileHeader.PointerToSymbolTable);*/

	if (mod->file)
		FsSeek(mod->file, pe->FileHeader.PointerToSymbolTable);
	else
		return false;

	closest_diff = (addr_t) (uint32_t) -1;
	found = false;
	for (i = 0; i < pe->FileHeader.NumberOfSymbols; i++)
	{
		FsRead(mod->file, &symbol, sizeof(symbol));

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
	if (flags & EFLAG_VM)
		swprintf(str, L"%04x:%04x", seg, ofs);
	else
		swprintf(str, L"%08x", ofs);
	return str;
}

void DbgDumpStack(process_t* proc, uint32_t _ebp)
{
	uint32_t *pebp = (uint32_t*) _ebp;
	module_t* mod;
	SYMENT sym;
	char *strings, *name;
	addr_t addr;

	wprintf(L"ebp\t\tReturn To\tModule\n");
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
			wprintf(L"(invalid)\n");
	} while (DbgIsValidEsp(proc, (uint32_t) pebp));
}

void DbgDumpModules(process_t* proc)
{
	module_t* mod;
	
	wprintf(L"Name\t\tBase\t\tLength\n");
	for (mod = proc->mod_first; mod; mod = mod->next)
		wprintf(L"%s\t%08x\t%08x\n", mod->name, mod->base, mod->length);
}

void DbgDumpThreads(void)
{
	thread_t *t;
	context_t *c;

	for (t = thr_first; t; t = t->all_next)
	{
		c = ThrGetContext(t);
		wprintf(L"%d\t%s\n",
			t->id, DbgFormatAddress(c->eflags, c->cs, c->eip));
	}
}

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
	int i, j;
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

static int DbgCompareVmArea(const void *elem1, const void *elem2)
{
	vm_area_t *a1 = *(vm_area_t**) elem1, 
		*a2 = *(vm_area_t**) elem2;

	return a1->start - a2->start;
}

void DbgDumpVmm(process_t *proc)
{
	vm_area_t *area;
	unsigned count, i;
	vm_area_t **ary;

	wprintf(L"Low memory:\t%uKB\n", (pool_low.free_pages * PAGE_SIZE) / 1024);
	wprintf(L"All memory:\t%uKB\n", (pool_all.free_pages * PAGE_SIZE) / 1024);

	wprintf(L"\tBlock\t\tStart\t\tEnd\t\t\tPages\n");

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
		wprintf(L"%c\t%p\t%08x\t%08x\t%x\n",
			/*area->is_committed ? 'C' : '_', */
			' ',
			ary[i],
			ary[i]->start,
			ary[i]->start + ary[i]->pages * PAGE_SIZE,
			ary[i]->pages);

	free(ary);
}

/*@} */