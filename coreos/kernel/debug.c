/*
 * $History: debug.c $
 * 
 * *****************  Version 4  *****************
 * User: Tim          Date: 13/03/04   Time: 22:18
 * Updated in $/coreos/kernel
 * Ignore \r, for serial debugger with uncooperative comms program
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 8/03/04    Time: 20:30
 * Updated in $/coreos/kernel
 * Added slab command
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/driver.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/fs.h>
#include <kernel/debug.h>
#include <kernel/arch.h>
#include <kernel/proc.h>
#include <kernel/multiboot.h>
#include <kernel/syscall.h>
#include <kernel/counters.h>
#include <kernel/init.h>
#include <kernel/slab.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include <os/coff.h>
#include <os/pe.h>

#define KSTACK_BOTTOM   0x80000000
#define KSTACK_TOP      0xe0000000
#define USTACK_BOTTOM   0
#define USTACK_TOP      0xc0000000

extern uint8_t start_stack[];
extern thread_queue_t thr_sleeping, thr_priority[32];
extern unsigned sc_uptime;
extern addr_t mem_pages_phys;
extern __maldbg_header_t *maldbg_first, *maldbg_last;
extern slab_t *slab_first, *slab_last;

static bool dbg_exit, dbg_handled;
const debugio_t *debug_io;

__maldbg_header_t *__malloc_find_block(void *addr);
bool __malloc_check_block(__maldbg_header_t *header);

int DbgWrite(const wchar_t *str, size_t count)
{
    if (debug_io->write_string == NULL)
        return -1;
    else
        return debug_io->write_string(str, count);
}


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
    uint64_t offset;
    
    dos = (IMAGE_DOS_HEADER*) mod->base;
    pe = (IMAGE_PE_HEADERS*) ((uint32_t) mod->base + dos->e_lfanew);
    sections = IMAGE_FIRST_SECTION(pe);

    /*symbols = (SYMENT*) ((uint8_t*) rawdata + 
        pe->FileHeader.PointerToSymbolTable);*/

    if (mod->file)
        offset = pe->FileHeader.PointerToSymbolTable;
    else
        return false;

    closest_diff = (addr_t) (uint32_t) -1;
    found = false;
    for (i = 0; i < pe->FileHeader.NumberOfSymbols; i++)
    {
        FsRead(mod->file, &symbol, offset, sizeof(symbol), NULL);
        offset += sizeof(symbol);

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

bool DbgIsValidEsp(thread_t* thr, uint32_t _esp)
{
    if ((_esp >= (uint32_t) thr->kernel_stack && 
        _esp <= (uint32_t) thr->kernel_stack + PAGE_SIZE * 2 - 4) ||
        (_esp >= (uint32_t) start_stack &&
        _esp <= (uint32_t) start_stack + PAGE_SIZE - 4))
        return (MemTranslate((void*) _esp) & PRIV_PRES) != 0;
    else if (thr->process == current()->process &&
        thr->process->stack_end != NULL && 
        _esp >= thr->process->stack_end && 
        _esp < 0x80000000 - 4)
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

void DbgDumpStack(thread_t* thr, const context_t *ctx)
{
    uint32_t *pebp = (uint32_t*) ctx->regs.ebp;
    module_t* mod;
    unsigned i;
    /*SYMENT sym;
    char *strings, *name;
    addr_t addr;*/

    wprintf(L"ebp\t\t\tReturn To\tParameters\n");
    wprintf(L"        \t%s\n", DbgFormatAddress(ctx->eflags, ctx->cs, ctx->eip));
    i = 0;
    do
    {
        wprintf(L"%08x\t", (uint32_t) pebp);

        if (DbgIsValidEsp(thr, (uint32_t) pebp))
        {
            wprintf(L"%s (",
                DbgIsValidEsp(thr, (uint32_t) (pebp + 1)) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[1]) : L"????????");
            wprintf(L"%s, ",
                DbgIsValidEsp(thr, (uint32_t) (pebp + 2)) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[2]) : L"????????");
            wprintf(L"%s, ", 
                DbgIsValidEsp(thr, (uint32_t) (pebp + 3)) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[3]) : L"????????");
            wprintf(L"%s) ", 
                DbgIsValidEsp(thr, (uint32_t) (pebp + 4)) ? DbgFormatAddress(ctx->eflags, ctx->ss, pebp[4]) : L"????????");

            if (thr != NULL)
            {
                mod = DbgLookupModule(thr->process, pebp[1]);
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
            wprintf(L"(invalid: stack_end = %x, kernel_stack = %p)\n", 
                thr->process->stack_end, thr->kernel_stack);
        
        i++;

        if (i >= 20)
        {
            wprintf(L"DbgDumpStack: stopped after %u lines\n", i);
            break;
        }
    } while (DbgIsValidEsp(thr, (uint32_t) pebp));
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
    const wchar_t *exe, *name;

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

            if (t->name == NULL)
                name = L"(null)";
            else
                name = t->name;

            wprintf(L"%s%*s\t%s%*s\t%u\t",
                exe, 40 - wcslen(exe), L"",
                name, 40 - wcslen(name), L"",
                t->id);

#ifdef WIN32
            wprintf(L"(not available)\n");
#else
            c = ThrGetContext(t);

            if ((addr_t) c < PAGE_SIZE)
                wprintf(L"(unknown)\n");
            else
            {
                wprintf(L"%s ", DbgFormatAddress(c->eflags, c->cs, c->eip));
                wprintf(L"%s\n", DbgFormatAddress(c->eflags, c->ss, c->regs.ebp));
            }
#endif
        }
    }
}


/*! Bugger me! This function's huge! I can't believe the amount of effort  */
/*! required to extract line & file information from COFF debugging info. */
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


static unsigned DbgCountVmNodes(vm_node_t *parent)
{
    if (parent == NULL)
        return 0;
    else
        return 1 + DbgCountVmNodes(parent->left) + DbgCountVmNodes(parent->right);
}


static vm_node_t **DbgEnumVmNodes(vm_node_t *parent, vm_node_t **ary)
{
    if (parent != NULL)
    {
        ary = DbgEnumVmNodes(parent->left, ary);
        *ary = parent;
        ary++;
        ary = DbgEnumVmNodes(parent->right, ary);
    }

    return ary;
}


static void DbgCmdVmm(wchar_t *cmd, wchar_t *params)
{
    process_t *proc;
    unsigned count, i, num_pages;
    vm_node_t **ary;
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

    wprintf(L"Block   \tStart   \tEnd     \tType            \tFlags\t\n");

    count = DbgCountVmNodes(proc->vmm_top);

    ary = malloc(count * sizeof(vm_node_t*));
    assert(ary != NULL);

    DbgEnumVmNodes(proc->vmm_top, ary);

    for (i = 0; i < count; i++)
    {
        if (VMM_NODE_IS_EMPTY(ary[i]))
        {
            type = L"(empty)";
            num_pages = ary[i]->u.empty_pages;
        }
        else
        {
            switch (ary[i]->u.desc->type)
            {
            case VM_AREA_NORMAL:
                type = L"VM_AREA_NORMAL";
                break;
            case VM_AREA_MAP:
                type = L"VM_AREA_MAP";
                break;
            case VM_AREA_FILE:
                type = L"VM_AREA_FILE";
                break;
            case VM_AREA_IMAGE:
                type = L"VM_AREA_IMAGE";
                break;
            case VM_AREA_CALLBACK:
                type = L"VM_AREA_CALLBACK";
                break;
            default:
                type = L"VM_AREA_?";
                break;
            }

            num_pages = ary[i]->u.desc->num_pages;
        }

        wprintf(L"%p\t%08x\t%08x\t%-16s\t%04x\n",
            ary[i],
            ary[i]->base,
            ary[i]->base + num_pages * PAGE_SIZE,
            type,
            ary[i]->flags);
    }

    free(ary);
}


static void DbgCmdHandles(wchar_t *cmd, wchar_t *params)
{
    process_t *cur;
    unsigned i;
    handle_hdr_t *h;
    char *tag;

    if (*params == NULL)
        cur = current()->process;
    else
    {
        cur = DbgFindProcess(wcstol(params, NULL, 0));
        if (cur == NULL)
        {
            wprintf(L"%s: unknown process\n", params);
            return;
        }
    }

    wprintf(L"%s: %u handles, handles = %p\n", cur->exe, cur->handle_count, cur->handles);
    wprintf(L"Handle\tPointer  Cp Tag \tAllocator\n");
    for (i = 0; i < cur->handle_count; i++)
    {
        h = HANDLE_ENTRY_TO_POINTER(cur->handles[i]);
        if (h != NULL)
        {
        wprintf(L"%4u\t%p", i, h);
            tag = (char*) &h->cls->tag;
            wprintf(L" %2d %c%c%c%c\t%S(%d) ", 
                h->copies, 
                tag[3], tag[2], tag[1], tag[0], 
                h->file, h->line);

            switch (h->cls->tag)
            {
            case 'proc':
                wprintf(L"ID = %u, %s ", 
                    ((process_t*) h)->id,
                    ((process_t*) h)->exe);
                break;
            case 'thrd':
                wprintf(L"ID = %u, %s", 
                    ENCLOSING_STRUCT(h, thread_t, hdr)->id,
                    ENCLOSING_STRUCT(h, thread_t, hdr)->name);
                break;
            case 'file':
                wprintf(L"file = %p, node = %p/%x", 
                    ((file_handle_t*) h)->file,
            ((file_handle_t*) h)->file->vnode.fsd,
            ((file_handle_t*) h)->file->vnode.id);
                break;
            }

        wprintf(L"\n");
        }
    }
}


static void DbgCmdShutdown(wchar_t *cmd, wchar_t *params)
{
    unsigned type;

    if (*params == '\0')
        type = SHUTDOWN_POWEROFF;
    else if (_wcsicmp(params, L"reboot") == 0)
        type = SHUTDOWN_REBOOT;
    else
    {
        wprintf(L"%s: invalid shutdown command\n", params);
        return;
    }

    WrapSysShutdown(type);
}


static void DbgCmdMalloc(wchar_t *cmd, wchar_t *params)
{
    void *ptr;
    __maldbg_header_t *header;

    if (*params == '\0')
    {
        wprintf(L"maldbg_first = %p, maldbg_last = %p\n",
            maldbg_first + 1,
            maldbg_last + 1);
        swprintf(cmd, L"mal %p", maldbg_first + 1);
    }
    else
    {
        ptr = (void*) wcstoul(params, NULL, 16);
        header = __malloc_find_block(ptr);
        if (header == NULL)
        {
            wprintf(L"%p: block not found in kernel list, displaying info for user mode\n", ptr);
            header = (__maldbg_header_t*) ptr - 1;
        }

        wprintf(L"%p: block at %p: %s\n", 
            ptr, 
            header,
            __malloc_check_block(header) ? L"valid" : L"invalid");
        wprintf(L"%u+%u bytes, allocated at %S(%d), tag = %08x\n", 
            sizeof(__maldbg_header_t), header->size - sizeof(__maldbg_header_t),
            header->file, header->line,
            header->tag);

        if (header->prev == NULL)
            wprintf(L"prev = (none), ");
        else
            wprintf(L"prev = %p, ", header->prev + 1);

        if (header->next == NULL)
            wprintf(L"next = (none)\n");
        else
        {
            wprintf(L"next = %p\n", header->next + 1);
            swprintf(cmd, L"mal %p", header->next + 1);
        }
    }
}


static void DbgCmdLeak(wchar_t *cmd, wchar_t *params)
{
    if (*params == '\0')
    {
        wprintf(L"%s options:\n"
            L"dump      Dump memory leaks\n"
            L"off       Stop tracking memory allocations\n"
            L"<hex tag> Start tracking memory allocations and assign a tag\n",
            cmd);
    }
    else if (_wcsicmp(params, L"dump") == 0)
        __malloc_leak_dump();
    else if (_wcsicmp(params, L"off") == 0)
        __malloc_leak(0);
    else
    {
        uint32_t tag;
        tag = wcstoul(params, NULL, 16);
        __malloc_leak(tag);
        wprintf(L"Leak tracing on with tag %08x; type \"leak dump\" to dump\n",
            tag);
    }
}


static void DbgCmdExit(wchar_t *cmd, wchar_t *params)
{
    dbg_exit = true;
    dbg_handled = true;
}


static void DbgCmdKill(wchar_t *cmd, wchar_t *params)
{
    dbg_exit = true;
    dbg_handled = false;
}


static void DbgCmdPdbr(wchar_t *cmd, wchar_t *params)
{
    process_t *proc;

    if (*params == '\0')
    {
        uint32_t pdbr;
        __asm__("mov %%cr3, %0" : "=r" (pdbr));
        wprintf(L"Current PDBR: cr3 = %08x\n", pdbr);
    }
    else
    {
        proc = DbgFindProcess(wcstol(params, NULL, 0));
        if (proc == NULL)
        {
            wprintf(L"%s: unknown process\n", params);
            return;
        }

        wprintf(L"PDBR for process %u: cr3 = %08x\n", 
            proc->id, proc->page_dir_phys);
    }
}


static void DbgCmdPte(wchar_t *cmd, wchar_t *params)
{
    addr_t addr;
    uint32_t pte;

    if (*params == '\0')
    {
        wprintf(L"Please specify a virtual memory address\n");
        return;
    }

    addr = wcstoul(params, 0, 16);
    pte = MemTranslate((void*) addr);
    wprintf(L"0x%08x => 0x%08x: %c%c%c%c%c%c%c%c\n", 
        addr, 
        pte & ~(PAGE_SIZE - 1),
        (pte & PRIV_PRES)           ? 'P' : 'p', 
        (pte & PRIV_WR)             ? 'W' : 'w', 
        (pte & PRIV_USER)           ? 'U' : 'u',
        (pte & PRIV_WRITETHROUGH)   ? 'T' : 't', 
        (pte & PRIV_NOCACHE)        ? 'N' : 'n', 
        (pte & PAGE_ACCESSED)       ? 'A' : 'a', 
        (pte & PAGE_DIRTY)          ? 'D' : 'd', 
        (pte & PAGE_GLOBAL)         ? 'G' : 'g');
}


static void DbgCmdDisplay(wchar_t *cmd, wchar_t *params)
{
    addr_t addr, last_page;
    void *ptr;
    unsigned i, hex_char, width;

    if (*params == '\0')
    {
        wprintf(L"Please specify a virtual memory address\n");
        return;
    }

    if (cmd[1] == 'b')
        width = 1;
    else if (cmd[1] == 'w')
        width = 2;
    else
        width = 4;

    addr = wcstoul(params, 0, 16);
    last_page = (addr_t) -1;
    ptr = (void*) addr;
    for (hex_char = 0; hex_char < 2; hex_char++)
    {
        i = 0;
        while (i < 16)
        {
            if ((addr & -PAGE_SIZE) != last_page)
            {
                last_page = addr & -PAGE_SIZE;
                if ((MemTranslate((void*) last_page) & PRIV_PRES) == 0)
                {
                    if (hex_char == 0)
                        DbgWrite(L"??", 2);
                    else
                        DbgWrite(L".", 1);

                    goto out_of_loop;
                }
            }

            if (hex_char == 0)
            {
                switch (width)
                {
                case 1:
                    wprintf(L"%02x ", ((uint8_t*) ptr)[i]);
                    break;

                case 2:
                    wprintf(L"%04x ", *(uint16_t*) ((uint8_t*) ptr + i));
                    break;

                case 4:
                    wprintf(L"%08x ", *(uint32_t*) ((uint8_t*) ptr + i));
                    break;
                }

                i += width;
            }
            else
            {
                switch (((uint8_t*) ptr)[i])
                {
                case '\b':
                case '\t':
                case '\n':
                case '\r':
                case '\0':
                    DbgWrite(L".", 1);
                    break;

                default:
                    wprintf(L"%C", ((uint8_t*) ptr)[i]);
                    break;
                }

                i++;
            }
        }
    }

out_of_loop:
    DbgWrite(L"\n", 1);
    swprintf(cmd, L"d%c %p", cmd[1], (uint8_t*) ptr + i);
}


static void DbgDumpPfnList(const pfn_list_t *list)
{
    wprintf(L"first = %u (%08x), last = %u (%08x), count = %u (%uKB)\n",
        list->first, PFN_TO_PHYS(list->first),
        list->last, PFN_TO_PHYS(list->last),
        list->num_pages, (list->num_pages * PAGE_SIZE) / 1024);
}


static void DbgCmdPfn(wchar_t *cmd, wchar_t *params)
{
    addr_t addr;
    pfn_t pfn;
    page_phys_t *page;

    if (*params == '\0')
    {
        wprintf(L"mem_pages from %p to %p (%08x to %08x)\n",
            mem_pages, 
            mem_pages + kernel_startup.memory_size / PAGE_SIZE,
            mem_pages_phys, 
            mem_pages_phys + sizeof(*mem_pages) * (kernel_startup.memory_size / PAGE_SIZE));
        wprintf(L"Free list:\n"
            L"\t");
        DbgDumpPfnList(&mem_free);
        wprintf(L"Free low list:\n"
            L"\t");
        DbgDumpPfnList(&mem_free_low);
        wprintf(L"Zero list:\n"
            L"\t");
        DbgDumpPfnList(&mem_zero);
    }
    else
    {
        addr = wcstoul(params, NULL, 16);
        pfn = PHYS_TO_PFN(addr);
        page = mem_pages + pfn;
        wprintf(L"Page at %08x (PFN %u):\n"
            L"\t", 
            addr & -PAGE_SIZE, pfn);

        switch (page->flags & PAGE_STATE_MASK)
        {
        case PAGE_STATE_FREE:
            wprintf(L"free ");
            break;
        case PAGE_STATE_ZERO:
            wprintf(L"zero ");
            break;
        case PAGE_STATE_ALLOCATED:
            wprintf(L"allocated ");
            break;
        case PAGE_STATE_RESERVED:
            wprintf(L"reserved ");
            break;
        }

        if (page->flags & PAGE_FLAG_LOW)
            wprintf(L"[from low list] ");
        if (page->flags & PAGE_FLAG_LARGE)
            wprintf(L"[large page] ");
        if (page->flags & PAGE_FLAG_TEMP_IN_USE)
            wprintf(L"[in temp use] ");
        if (page->flags & PAGE_FLAG_TEMP_LOCKED)
            wprintf(L"[temp locked] ");

        wprintf(L"temp = %u (%08x)", 
            page->temp_index, 
            page->temp_index * PAGE_SIZE + ADDR_TEMP_VIRT);

        if ((page->flags & PAGE_STATE_MASK) == PAGE_STATE_FREE ||
            (page->flags & PAGE_STATE_MASK) == PAGE_STATE_ZERO)
            wprintf(L"\n"
                L"\tprev = %u (%08x), next = %u (%08x)\n", 
                page->u.list.prev, PFN_TO_PHYS(page->u.list.prev), 
                page->u.list.next, PFN_TO_PHYS(page->u.list.next));
        else
            wprintf(L"\n"
                L"\tdescriptor = %p, offset = %u bytes (%08x)\n", 
                page->u.alloc_vm.desc,
                page->u.alloc_vm.offset, page->u.alloc_vm.offset);
    }
}


static void DbgBreakApc(void *param)
{
    __asm__("int3");
}


static void DbgCmdBreak(wchar_t *cmd, wchar_t *params)
{
    unsigned id;
    thread_t *t;

    if (*params == '\0')
        wprintf(L"Please enter a thread ID\n");
    else
    {
        id = wcstoul(params, NULL, 0);
        for (t = thr_first; t != NULL; t = t->all_next)
        {
            if (t->id == id)
            {
                ThrQueueKernelApc(t, DbgBreakApc, NULL);
                wprintf(L"Breakpoint set in thread %u (%s). Type 'go' to continue.\n",
                    id, t->process->exe);
                break;
            }
        }

        if (t == NULL)
            wprintf(L"%u: no such thread\n", id);
    }
}


static void DbgCmdBoot(wchar_t *cmd, wchar_t *params)
{
    wprintf(L"memory_size = %uKB (%x)\n", kernel_startup.memory_size / 1024, kernel_startup.memory_size);
    wprintf(L"kernel_size = %uKB (%x)\n", kernel_startup.kernel_size / 1024, kernel_startup.kernel_size);
    wprintf(L"kernel_phys = %08x\n", kernel_startup.kernel_phys);
    wprintf(L"multiboot_info = %p\n", kernel_startup.multiboot_info);
    wprintf(L"kernel_data = %uKB (%x)\n", kernel_startup.kernel_data / 1024, kernel_startup.kernel_data);
    wprintf(L"num_cpus = %u\n", kernel_startup.num_cpus);

    wprintf(L"multiboot: mods_count = %u\n", kernel_startup.multiboot_info->mods_count);
    wprintf(L"multiboot: mods_addr = %08x\n", kernel_startup.multiboot_info->mods_addr);
}


static void DbgCmdRunQueue(wchar_t *cmd, wchar_t *params)
{
    unsigned i, count;
    thread_queuent_t *entry;

    if (*params == '\0')
    {
        for (i = 0; i < _countof(thr_priority); i++)
        {
            count = 0;
            for (entry = thr_priority[i].first; entry != NULL; entry = entry->next)
                count++;

            if (count > 0)
                wprintf(L"%u: %u threads\n", i, count);
        }
    }
    else
    {
        i = wcstoul(params, NULL, 0);

        if (thr_priority[i].first == NULL)
            wprintf(L"Priority level %u has no threads ready\n", i);
        else
        {
            for (entry = thr_priority[i].first; entry != NULL; entry = entry->next)
                wprintf(L"%s/%u (%s)\n", entry->thr->process->exe, entry->thr->id, entry->thr->name);
        }
    }
}


static void DbgCmdStack(wchar_t *cmd, wchar_t *params)
{
    thread_t *thr;
    unsigned id;

    if (*params == '\0')
        thr = current();
    else
    {
        id = wcstoul(params, NULL, 0);
        for (thr = thr_first; thr != NULL; thr = thr->all_next)
            if (thr->id == id)
                break;

        if (thr == NULL)
        {
            wprintf(L"%u: no such thread\n", id);
            return;
        }
    }

    DbgDumpStack(thr, ThrGetContext(thr));
}


static void DbgCmdCounter(wchar_t *cmd, wchar_t *params)
{
    if (*params == '\0')
        wprintf(L"Please enter a counter name\n");
    else
    {
        counter_handle_t *counter;
        counter = CounterOpen(params);
        if (counter == NULL)
            wprintf(L"Counter %s not found\n", params);
        else
        {
            wprintf(L"%d\n", CounterSample(counter));
            HndClose(&counter->hdr);
        }
    }
}


static void DbgCmdPages(wchar_t *cmd, wchar_t *params)
{
    addr_t i, j, start, end;
    wchar_t *space;

    if (*params == '\0')
    {
        start = 0;
        end = kernel_startup.memory_size;
    }
    else
    {
        space = wcschr(params, ' ');
        if (space == NULL)
        {
            start = wcstoul(params, NULL, 16);
            end = start + PAGE_SIZE;
        }
        else
        {
            *space = '\0';
            do
            {
                space++;
            } while (*space == ' ');

            start = wcstoul(params, NULL, 16);
            end = wcstoul(space, NULL, 16);
        }
    }

    for (i = start; i < end; i = j)
    {
        wprintf(L"%08x: ", i);
        for (j = i; j < min(end, i + 0x00040000); j += 0x00001000)
        {
            page_phys_t *page;

            page = mem_pages + PHYS_TO_PFN(j);
            if (page->locks == 0)
            {
                switch (page->flags & PAGE_STATE_MASK)
                {
                case PAGE_STATE_FREE:
                    DbgWrite(L".", 1);
                    break;

                case PAGE_STATE_ZERO:
                    DbgWrite(L"z", 1);
                    break;

                case PAGE_STATE_ALLOCATED:
                    DbgWrite(L"A", 1);
                    break;

                case PAGE_STATE_RESERVED:
                    DbgWrite(L"r", 1);
                    break;
                }
            }
            else if (page->locks < 10)
            {
                wchar_t ch;
                ch = page->locks + '0';
                DbgWrite(&ch, 1);
            }
            else
                DbgWrite(L"!", 1);
        }

        wprintf(L"\n");
    }
}


static void DbgCmdSleeping(wchar_t *cmd, wchar_t *params)
{
    thread_queuent_t *entry;

    for (entry = thr_sleeping.first; entry != NULL; entry = entry->next)
        wprintf(L"%8u (%8dms)\t%s/%u (%s)\n", 
            entry->thr->sleep_end,
            entry->thr->sleep_end - sc_uptime,
            entry->thr->process->exe, 
            entry->thr->id, 
            entry->thr->name);
}


static void DbgCmdSlab(wchar_t *cmd, wchar_t *params)
{
    if (*params == '\0')
    {
        wprintf(L"slab_first = %p, slab_last = %p\n",
            slab_first, slab_last);
        swprintf(cmd, L"slab %p", slab_first);
    }
    else
    {
        slab_t *slab, *ptr;
        slab_chunk_t *chunk;

        ptr = (slab_t*) wcstoul(params, NULL, 16);
        for (slab = slab_first; slab != NULL; slab = slab->next)
            if (slab == ptr)
                break;

        if (slab == NULL)
        {
            wprintf(L"%p: not a slab\n", ptr);
            return;
        }

        wprintf(L"prev = %p, next = %p, %u objects per chunk\n"
            L"chunk_first = %p, quantum = %u, ctor = %p, dtor = %p\n",
            slab->prev, slab->next, SLAB_OBJECTS_PER_CHUNK(slab->quantum),
            slab->chunk_first, slab->quantum, slab->ctor, slab->dtor);

        for (chunk = slab->chunk_first; chunk != NULL; chunk = chunk->next)
            wprintf(L"%u @ %p\t", chunk->num_free_objects, chunk->data);
        wprintf(L"\n");

        if (slab->next != NULL)
            swprintf(cmd, L"slab %p", slab->next);
    }
}


static void DbgCmdHelp(wchar_t *cmd, wchar_t *params);

static struct
{
    const wchar_t *str;
    void (*fn)(wchar_t *, wchar_t *);
} dbg_commands[] = 
{
    { L"exit",      DbgCmdExit },
    { L"go",        DbgCmdExit },
    { L"kill",      DbgCmdKill },
    { L"help",      DbgCmdHelp },
    { L"shutdown",  DbgCmdShutdown },
    { L"vmm",       DbgCmdVmm },
    { L"modules",   DbgCmdModules },
    { L"mod",       DbgCmdModules },
    { L"threads",   DbgCmdThreads },
    { L"thr",       DbgCmdThreads },
    { L"processes", DbgCmdProcesses },
    { L"proc",      DbgCmdProcesses },
    { L"symbol",    DbgCmdSymbol },
    { L"sym",       DbgCmdSymbol },
    { L"handles",   DbgCmdHandles },
    { L"hnd",       DbgCmdHandles },
    { L"malloc",    DbgCmdMalloc },
    { L"mal",       DbgCmdMalloc },
    { L"leak",      DbgCmdLeak },
    { L"pdbr",      DbgCmdPdbr },
    { L"pte",       DbgCmdPte },
    { L"db",        DbgCmdDisplay },
    { L"dw",        DbgCmdDisplay },
    { L"dd",        DbgCmdDisplay },
    { L"pfn",       DbgCmdPfn },
    { L"break",     DbgCmdBreak },
    { L"boot",      DbgCmdBoot },
    { L"runqueue",  DbgCmdRunQueue },
    { L"stack",     DbgCmdStack },
    { L"k",         DbgCmdStack },
    { L"counter",   DbgCmdCounter },
    { L"pages",     DbgCmdPages },
    { L"sleeping",  DbgCmdSleeping },
    { L"slab",      DbgCmdSlab },
};

void DbgCmdHelp(wchar_t *cmd, wchar_t *params)
{
    unsigned i, j;

    wprintf(L"Valid commands are:\n");
    for (i = 0; i < _countof(dbg_commands); i += j)
        for (j = 0; j < min(8, _countof(dbg_commands) - i); j++)
            wprintf(L"%10s", dbg_commands[i + j].str);

    wprintf(L"\n");
}

bool DbgStartShell(void)
{
    wchar_t ch, str[80], *space;
    size_t len;
    unsigned i;

    wprintf(L"Mobius Debugging Shell\n");

    dbg_exit = debug_io->read_char == NULL;
    while (!dbg_exit)
    {
        wprintf(L"> ");

        len = 0;
        do
        {
            ch = debug_io->read_char();
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

            case '\r':
                break;

            case '\n':
                if (len > 0)
                {
                    wprintf(L"%c", ch);
                    str[len] = '\0';
                }
                else
                    wprintf(L"%s%c", str, ch);
                break;

            case '-':
                ch = '_';
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
            space++;

        for (i = 0; i < _countof(dbg_commands); i++)
            if (_wcsnicmp(str, dbg_commands[i].str, space - str - 1) == 0)
            {
                dbg_commands[i].fn(str, space);
                break;
            }
    }

    return dbg_handled;
}


void i386InitSerialDebug(void);

static const debugio_t dbg_none =
{
    NULL,
    NULL,
    NULL,
};

static const debugio_t dbg_console =
{
    DbgInitConsole,
    DbgWriteStringConsole,
    DbgGetCharConsole,
};

static const debugio_t dbg_serial =
{
    DbgInitSerial,
    DbgWriteStringSerial,
    DbgGetCharSerial,
};

bool DbgSetPort(unsigned port, unsigned speed)
{
    const debugio_t *old_io;

    kernel_startup.debug_port = port;
    kernel_startup.debug_speed = speed;

    old_io = debug_io;
    if (port == 0)
        debug_io = &dbg_none;
    else if (port == 1)
        debug_io = &dbg_console;
    else
        debug_io = &dbg_serial;

    if (old_io == debug_io)
        return true;
    else if (debug_io->init == NULL)
        return true;
    else
        return debug_io->init();
}
