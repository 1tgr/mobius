#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/driver.h>
#include <kernel/sys.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/fs.h>

#include <errno.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include <os/keyboard.h>
#include <os/coff.h>
#include "../keyboard/british.h"
#include "nasm.h"
#include "insns.h"
#include "disasm.h"
#include "sync.h"

/*!
 *  \ingroup	drivers
 *  \defgroup	kdebug	Kernel debugger
 *  @{
 */

#define KSTACK_BOTTOM	0xc0009000
#define KSTACK_TOP		0xc000b000
#define USTACK_BOTTOM	0
#define USTACK_TOP		0xc0000000

bool key_read;
wchar_t key;
device_t* keyboard;

static const wchar_t* thread_states[] =
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
};

thread_t* thr;
addr_t fault_addr;

void dbgBreak()
{
	asm("int3");
}

bool in_debugger;

bool dbgLookupSymbol(module_t *mod, void* sym, addr_t* address, SYMENT *syment)
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
	pe = (IMAGE_PE_HEADERS*) ((dword) mod->base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	/*symbols = (SYMENT*) ((byte*) rawdata + 
		pe->FileHeader.PointerToSymbolTable);*/

	if (mod->file)
		fsSeek(mod->file, pe->FileHeader.PointerToSymbolTable);
	else
		return false;

	closest_diff = (addr_t) (dword) -1;
	found = false;
	for (i = 0; i < pe->FileHeader.NumberOfSymbols; i++)
	{
		fsRead(mod->file, &symbol, sizeof(symbol));

		if (symbol.e_sclass == C_EXT ||
			symbol.e_sclass == C_STAT || 
			symbol.e_sclass == C_LABEL)
		{
			addr = mod->base + symbol.e_value;
			if (symbol.e_scnum > 0)
				addr += sections[symbol.e_scnum - 1].VirtualAddress;
				
			//wprintf(L"%d scnum = %d addr = %x\n", 
				//i, symbols[i].e_scnum, addr);

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

char* dbgGetStringsTable(module_t *mod)
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

char* dbgGetSymbolName(void* strings, SYMENT* se)
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

module_t* dbgLookupModule(process_t* proc, addr_t addr)
{
	module_t* mod;
	
	for (mod = proc->mod_first; mod; mod = mod->next)
		if (addr >= mod->base && addr < mod->base + mod->length)
			return mod;
	
	return NULL;
}

bool dbgIsValidEsp(process_t* proc, dword _esp)
{
	if ((_esp >= KSTACK_BOTTOM && _esp <= KSTACK_TOP - 4) ||
		_esp >= 0xf0000000 ||
		(proc->stack_end && _esp >= proc->stack_end && _esp < 0x80000000 - 4))
		return true;
	else
		return false;
}

const wchar_t* dbgFormatAddress(dword flags, dword seg, dword ofs)
{
	static wchar_t str[50];
	if (flags & EFLAG_VM)
		swprintf(str, L"%04x:%04x", seg, ofs);
	else
		swprintf(str, L"%08x", ofs);
	return str;
}

void dbgDumpStack(process_t* proc, dword _ebp)
{
	dword *pebp = (dword*) _ebp;
	module_t* mod;
	SYMENT sym;
	char *strings, *name;
	addr_t addr;

	_cputws(L"ebp\t\tReturn To\tModule\n");
	do
	{
		wprintf(L"%08x\t", (dword) pebp);

		if (dbgIsValidEsp(proc, (dword) pebp))
		{
			wprintf(L"%08x\t", pebp[1]);

			if (proc)
			{
				mod = dbgLookupModule(proc, pebp[1]);
				if (mod)
				{
					_cputws(mod->name);
					if (dbgLookupSymbol(mod, (void*) pebp[1], &addr, &sym))
					{
						strings = dbgGetStringsTable(mod);
						name = dbgGetSymbolName(strings, &sym);
						if (name)
							wprintf(L"\t%S + 0x%x", name, pebp[1] - addr);
					}
				}
				else
					_cputws(L"(unknown)");
			}

			pebp = (dword*) *pebp;
		}

		putwchar('\n');
	} while (dbgIsValidEsp(proc, (dword) pebp));
}

void dbgDumpModules(process_t* proc)
{
	module_t* mod;
	
	_cputws(L"Name\t\tBase\t\tLength\n");
	for (mod = proc->mod_first; mod; mod = mod->next)
		wprintf(L"%s\t%08x\t%08x\n", mod->name, mod->base, mod->length);
}

void dbgDumpThreads()
{
	thread_t *t;
	context_t *c;

	for (t = thr_first; t; t = t->next)
	{
		c = thrContext(t);
		wprintf(L"%d\t%s\t%s\n",
			t->id, dbgFormatAddress(c->eflags, c->cs, c->eip),
			thread_states[t->state]);
	}
}

void dbgDumpContext(const context_t* ctx)
{
	wprintf(L"EAX %08x\tEBX %08x\tECX %08x\tEDX %08x\n",
		ctx->regs.eax, ctx->regs.ebx, ctx->regs.ecx, ctx->regs.edx);
	wprintf(L"ESI %08x\tEDI %08x\tESP %08x\tEBP %08x\n",
		/* note: ctx->regs.esp refers to kernel esp at time of pusha => ignored.
		   Use ctx->esp instead => pushed by interrupt */
		ctx->regs.esi, ctx->regs.edi, ctx->esp, ctx->regs.ebp);
	wprintf(L" DS %08x\t ES %08x\t FS %08x\t GS %08x\t\n",
		ctx->ds, ctx->es, ctx->fs, ctx->gs);
	wprintf(L" CS %08x\t SS %08x\t\n", ctx->cs, ctx->ss);
	wprintf(L"EIP %08x\t eflags %08x\tintr %08x\n", ctx->eip, ctx->eflags, ctx->intr);
}

void dbgSwitchThreads(thread_t* t, context_t* ctx)
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
	handle_t* hnd;
	const wchar_t *msg;
	wchar_t temp[20];
	
	thr = t;
	mod = dbgLookupModule(thr->process, ctx->eip);
	if (ctx->eflags & EFLAG_TF && ctx->intr == EXCEPTION_DEBUG)
		;
		/*wprintf(L"Step: %s (%s)\n", dbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), 
			mod ? mod->name : L"unknown module");*/
	else
	{
		dword code = ctx->intr;
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
			dbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), 
			mod ? mod->name : L"unknown module", 
			fault_addr);
		wprintf(L"State: %s ", thread_states[thr->state]);

		switch (thr->state)
		{
		case THREAD_RUNNING:
		case THREAD_DELETED:
			putwchar('\n');
			break;
		case THREAD_WAIT_TIMEOUT:
			wprintf(L"at %d, in %d ms\n", thr->wait.time, thr->wait.time - sysUpTime());
			break;
		case THREAD_WAIT_HANDLE:
			hnd = hndHandle(thr->wait.handle.handles[0]);
			wprintf(L"for %p (%s:%d)\n", hnd, hnd->file, hnd->line);
			break;
		//case THREAD_WAIT_OBJECT:
			//wprintf(L"for %p\n", thr->wait.object);
			//break;
		}
	}
}

wint_t getwchar()
{
	dword key;
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
				_cputws(L"\b \b");
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
}

//! Bugger me! This function's huge! I can't believe the amount of effort 
//!	required to extract line & file information from COFF debugging info.
void dbgLookupLineNumber(addr_t base, void* rawdata, void* symbol, 
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
	pe = (IMAGE_PE_HEADERS*) ((dword) base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	found = NULL;
	for (i = 0; i < pe->FileHeader.NumberOfSections; i++)
	{
		if (strncmp(sections[i].Name, ".text", sizeof(sections[i].Name)) == 0)
		{
			line_numbers = malloc(sizeof(LINENO) * sections[i].NumberOfLinenumbers);
			memcpy(line_numbers, 
				(byte*) rawdata + sections[i].PointerToLinenumbers,
				sizeof(LINENO) * sections[i].NumberOfLinenumbers);
			
			if (line_numbers)
			{
				found = NULL;
				func = NULL;
				for (j = 0; j < sections[i].NumberOfLinenumbers; j++)
				{
					if (line_numbers[j].l_lnno)
					{
						//wprintf(L"line %d\t%x\n", line_numbers[j].l_lnno, 
							//line_numbers[j].l_addr.l_paddr);
					
						if (line_numbers[j].l_addr.l_paddr >= (dword) symbol)
						{
							found = line_numbers + j;
							break;
						}
					}
					else
						func = line_numbers + j;
						//wprintf(L"function %d\n", line_numbers[j].l_addr.l_symndx);
				}

				if (found)
					break;
			}
			
			free(line_numbers);
			break;
		}
	}

	if (found)
	{
		*line = found->l_lnno - 1;
		symbols = (SYMENT*) ((byte*) rawdata + 
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
			*file = dbgGetSymbolName(strings, filename);
	}
}

int dbgCompareVmArea(const void *elem1, const void *elem2)
{
	vm_area_t *a1 = *(vm_area_t**) elem1, 
		*a2 = *(vm_area_t**) elem2;

	return a1->start - a2->start;
}

void dbgDumpVmm(process_t *proc)
{
	vm_area_t *area;
	unsigned count, i;
	vm_area_t **ary;

	wprintf(L"Low memory:\t%uKB\n", (pool_low.free_pages * PAGE_SIZE) / 1024);
	wprintf(L"All memory:\t%uKB\n", (pool_all.free_pages * PAGE_SIZE) / 1024);

	wprintf(L"\tBlock\t\tStart\t\tEnd\t\t\tPages\n");

	count = 0;
	for (area = proc->first_vm_area; area; area = area->next)
		count++;

	ary = malloc(count * sizeof(vm_area_t*));
	assert(ary != NULL);

	for (area = proc->first_vm_area, i = 0; 
		area && i < count; 
		area = area->next, i++)
		ary[i] = area;

	qsort(ary, count, sizeof(vm_area_t*), dbgCompareVmArea);

	for (i = 0; i < count; i++)
		wprintf(L"%c\t%p\t%08x\t%08x\t%x\n",
			//area->is_committed ? 'C' : '_',
			' ',
			ary[i],
			ary[i]->start,
			ary[i]->start + ary[i]->pages * PAGE_SIZE,
			ary[i]->pages);

	free(ary);
}

int add_label(struct itemplate* p, insn* ins, wchar_t* str, int operand)
{
	SYMENT sym;
	module_t *mod;
	addr_t addr, realaddr;
	char *name, *strings;
	
	addr = ins->oprs[operand].offset;
	mod = dbgLookupModule(thr->process, addr);
	
	if (mod && dbgLookupSymbol(mod, (void*) addr, &realaddr, &sym))
	{
		strings = dbgGetStringsTable(mod);
		name = dbgGetSymbolName(strings, &sym);
		if (addr == realaddr)
			return swprintf(str, L"%S", name);
		else
			return swprintf(str, L"%S + 0x%x", name, addr - realaddr);
	}
	else
		return swprintf(str, L"0x%x", addr);
}

bool dbgAttach(thread_t* t, context_t* ctx, addr_t addr)
{
	static wchar_t buf[256];
	static int lastCommand;

	unsigned id;
	int i;
	module_t *mod;
	//unsigned line;
	char *name;
	void* strings;
	addr_t symaddr;
	SYMENT sym;
	
	if (keyboard == NULL)
		keyboard = devOpen(L"keyboard", NULL);
	
	t->suspend++;
	enable();
	fault_addr = addr;
	dbgSwitchThreads(t, ctx);
	while (true)
	{
		if (memTranslate(thr->process->page_dir, (void*) ctx->eip))
			disasm((unsigned char*) ctx->eip, buf, 32, ctx->eip, true, 0);
		else
			wcscpy(buf, L"(disassembly unavailable)");

		/*mod = dbgLookupModule(thr->process, ctx->eip);
		if (mod)
		{
			//dbgLookupLineNumber(mod, (void*) ctx->eip, &line, &file);
			//wprintf(L"%S:%d:\t", file, line);

			if (dbgLookupSymbol(mod, (void*) ctx->eip, &symaddr, &sym))
			{
				strings = dbgGetStringsTable(mod);
				name = dbgGetSymbolName(strings, &sym);
				if (name)
					wprintf(L"%S + 0x%x", name, ctx->eip - symaddr);
			}

			_cputws(L"\n");
		}*/

		wprintf(L"%s\t%s\n> ", 
			dbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), buf);

		_getws(buf);

		if (buf[0])
		{
			for (i = 0; i < countof(commands); i++)
				if (wcsicmp(buf, commands[i]) == 0)
					break;
			lastCommand = i;
		}
		else
			i = lastCommand;

		switch (i)
		{
		case 0:
			_cputws(
				L"help\t\t"		L"Help\n"
				L"ctx\t\t"		L"Dump context\n"
				L"stk\t\t"		L"Dump stack\n"
				L"vmm\t\t"		L"Dump process memory blocks\n"
				L"mods\t\t"		L"List modules\n"
				L"threads\t"	L"List all threads\n"
				L"thread\t"		L"Switch threads\n"
				L"t\t\t\t"		L"Single step\n"
				L"go\t\t"		L"Quit and continue thread\n"
				L"quit\t\t"		L"Terminate thread and quit\n");
			break;
		case 1:
			dbgDumpContext(ctx);
			break;
		case 2:
			dbgDumpStack(thr->process, ctx->regs.ebp);
			break;
		case 3:
			dbgDumpModules(thr->process);
			break;
		case 4:
			dbgDumpThreads();
			break;
		case 5:
			_cputws(L"Thread id: ");
			_getws(buf);
			id = wcstol(buf, NULL, 10);
			for (t = thr_first; t; t = t->next)
				if (t->id == id)
				{
					in_debugger = false;
					ctx->eflags &= ~EFLAG_TF;
					dbgInvoke(t, thrContext(t), 0);
					return true;
				}

			wprintf(L"Thread %d not found\n", id);
			break;
		case 6:
			ctx->eflags |= EFLAG_TF;
			in_debugger = false;
			t->suspend--;
			return true;
		case 7:
			ctx->eflags &= ~EFLAG_TF;
			in_debugger = false;
			t->suspend--;
			return true;
		case 8:
			if (thr->id == 0)
				halt(0);
			else
				procTerminate(thr->process);
			in_debugger = false;
			t->suspend--;
			return true;
		case 9:
			dbgDumpVmm(thr->process);
			break;
		default:
			wprintf(L"%s: invalid command\n", buf);
			break;
		}
	}
}

bool dbgRequest(device_t* dev, request_t* req)
{
	byte k;
	wchar_t* temp;

	wprintf(L"<%c%c>", req->code / 256, req->code % 256);
	switch (req->code)
	{
	case DEV_ISR:
		k = in(0x60);

		if (k < countof(keys))
		{
			key = keys[k];
			key_read = true;

			if (key == 27 && !in_debugger)
				dbgInvoke(thrCurrent(), thrContext(thrCurrent()), 0);
		}

		return true;

	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_WRITE:
		//wprintf(L"Write: %p, %d bytes\n", req->params.write.buffer, req->params.write.length);
		temp = (wchar_t*) malloc(req->params.write.length + sizeof(wchar_t));
		memcpy(temp, req->params.write.buffer, req->params.write.length);
		temp[req->params.write.length / sizeof(wchar_t)] = 0;
		_cputws(temp);
		free(temp);
		hndSignal(req->event, true);
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

bool STDCALL drvInit(driver_t* drv)
{
	device_t* dev;
	init_sync();

	dev = hndAlloc(sizeof(device_t), NULL);
	dev->driver = drv;
	dev->request = dbgRequest;
	devRegister(L"debugger", dev, NULL);
	devRegisterIrq(dev, 1, true);
	return true;
}

//@}