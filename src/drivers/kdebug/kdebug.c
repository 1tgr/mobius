#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/driver.h>
#include <kernel/sys.h>
#include <kernel/memory.h>
#include <errno.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

#include <os/keyboard.h>
#include <os/coff.h>
#include "../keyboard/british.h"
#include "nasm.h"
#include "insns.h"
#include "disasm.h"
#include "sync.h"

#define KSTACK_BOTTOM	0xc0009000
#define KSTACK_TOP		0xc000b000
#define USTACK_BOTTOM	0
#define USTACK_TOP		0xc0000000

bool key_read;
wchar_t key;

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
};

thread_t* thr;
addr_t fault_addr;

void dbgBreak()
{
	asm("int3");
}

bool in_debugger;

SYMENT* dbgLookupSymbol(addr_t base, void* rawdata, void* symbol, addr_t* address)
{
	IMAGE_DOS_HEADER* dos;
	IMAGE_PE_HEADERS* pe;
	IMAGE_SECTION_HEADER* sections;
	SYMENT *symbols, *found, *closest;
	int i;
	addr_t addr, closest_diff;
	
	dos = (IMAGE_DOS_HEADER*) base;
	pe = (IMAGE_PE_HEADERS*) ((dword) base + dos->e_lfanew);
	sections = IMAGE_FIRST_SECTION(pe);

	symbols = (SYMENT*) ((byte*) rawdata + 
		pe->FileHeader.PointerToSymbolTable);

	found = NULL;
	closest = NULL;
	closest_diff = (addr_t) (dword) -1;
	for (i = 0; i < pe->FileHeader.NumberOfSymbols; i++)
	{
		if (symbols[i].e_sclass == C_EXT ||
			symbols[i].e_sclass == C_STAT || 
			symbols[i].e_sclass == C_LABEL)
		{
			addr = base + symbols[i].e_value;
			if (symbols[i].e_scnum > 0)
				addr += sections[symbols[i].e_scnum - 1].VirtualAddress;
				
			//wprintf(L"%d scnum = %d addr = %x\n", 
				//i, symbols[i].e_scnum, addr);

			if (addr >= (addr_t) symbol &&
				addr - (addr_t) symbol < closest_diff)
			{
				closest = symbols + i;
				closest_diff = addr - (addr_t) symbol;
			}
		}
	}

	if (closest && address)
	{
		addr = base + closest->e_value;
		if (closest->e_scnum > 0)
			addr += sections[closest->e_scnum - 1].VirtualAddress;
		*address = addr;
	}

	return closest;
}

char* dbgGetStringsTable(addr_t base, void* rawdata)
{
	IMAGE_DOS_HEADER *dos;
	IMAGE_PE_HEADERS *pe;
	SYMENT *symbols;

	dos = (IMAGE_DOS_HEADER*) base;
	pe = (IMAGE_PE_HEADERS*) (base + dos->e_lfanew);
	symbols = symbols = (SYMENT*) ((byte*) rawdata + 
		pe->FileHeader.PointerToSymbolTable);
	return (char*) (symbols + pe->FileHeader.NumberOfSymbols);
}

char* dbgGetSymbolName(void* strings, SYMENT* se)
{
	if (se->e.e.e_zeroes == 0)
	{
		if (se->e.e.e_offset)
			return (char*) strings + se->e.e.e_offset;
		else
			return NULL;
	}
	else
		return se->e.e_name;
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
	SYMENT* sym;
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
					sym = dbgLookupSymbol(mod->base, mod->raw_data, (void*) pebp[1], &addr);
					if (sym)
					{
						strings = dbgGetStringsTable(mod->base, mod->raw_data);
						name = dbgGetSymbolName(strings, sym);
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
	module_t* mod;
	handle_t* hnd;
	
	thr = t;
	mod = dbgLookupModule(thr->process, ctx->eip);
	if (ctx->eflags & EFLAG_TF && ctx->intr == EXCEPTION_DEBUG)
		;
		/*wprintf(L"Step: %s (%s)\n", dbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), 
			mod ? mod->name : L"unknown module");*/
	else
	{
		wprintf(L"Thread %d: exception %d at %s (%s): address %08x\n", thr->id, ctx->intr, 
			dbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), 
			mod ? mod->name : L"unknown module", fault_addr);
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
	device_t* dev;
	dword key;
	size_t len = sizeof(key);

	dev = devOpen(L"keyboard", NULL);

	do
	{
		key = 0;
		devReadSync(dev, 0, &key, &len);
	} while ((wchar_t) key == 0);
	devClose(dev);

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
			buf--;
			_cputws(L"\b \b");
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

int add_label(struct itemplate* p, insn* ins, wchar_t* str, int operand)
{
	SYMENT *sym;
	module_t *mod;
	addr_t addr, realaddr;
	char *name, *strings;
	
	addr = ins->oprs[operand].offset;
	//mod = dbgLookupModule(thr->process, addr);
	//if (mod)
		//sym = dbgLookupSymbol(mod->base, mod->raw_data, (void*) addr, &realaddr);
	//else
		sym = NULL;

	if (sym)
	{
		strings = dbgGetStringsTable(mod->base, mod->raw_data);
		name = dbgGetSymbolName(strings, sym);
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
	unsigned id;
	int i;
	module_t *mod;
	unsigned line;
	char* file;
	
	enable();
	fault_addr = addr;
	dbgSwitchThreads(t, ctx);
	while (true)
	{
		if (memTranslate(thr->process->page_dir, (void*) ctx->eip))
			disasm((unsigned char*) ctx->eip, buf, 32, ctx->eip, true, 0);
		else
			wcscpy(buf, L"(disassembly unavailable)");

		mod = dbgLookupModule(thr->process, ctx->eip);
		if (mod)
		{
			dbgLookupLineNumber(mod->base, mod->raw_data, (void*) ctx->eip,
				&line, &file);
			wprintf(L"%S:%d:\n", file, line);
		}

		wprintf(L"%s\t%s\n> ", 
			dbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip), buf);

		_getws(buf);

		for (i = 0; i < countof(commands); i++)
			if (wcsicmp(buf, commands[i]) == 0)
				break;

		switch (i)
		{
		case 0:
			_cputws(
				L"help\t\t"		L"Help\n"
				L"ctx\t\t"		L"Dump context\n"
				L"stk\t\t"		L"Dump stack\n"
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
			return true;
		case 7:
			ctx->eflags &= ~EFLAG_TF;
			in_debugger = false;
			return true;
		case 8:
			if (thr->id == 0)
				halt(0);
			else
				procTerminate(thr->process);
			in_debugger = false;
			return true;
		}
	}
}

bool dbgRequest(device_t* dev, request_t* req)
{
	byte k;
	wchar_t* temp;

	//wprintf(L"<%c%c>", req->code / 256, req->code % 256);
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
	//devRegisterIrq(dev, 1, true);
	return true;
}
