/* $Id: mod_pe.c,v 1.1 2002/12/21 09:49:26 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/fs.h>
#include <kernel/vmm.h>
#include <kernel/arch.h>
#include <kernel/profile.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <os/pe.h>
#include <os/rtl.h>
#include <os/defs.h>

#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern module_t mod_kernel;

static IMAGE_PE_HEADERS* PeGetHeaders(addr_t base)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*) base;
    return (IMAGE_PE_HEADERS*) (base + dos->e_lfanew);
}

bool PeFindFile(const wchar_t *relative, wchar_t *full, const wchar_t *search)
{
    const wchar_t *ch;
    wchar_t *buf;
    size_t l1, l2;

    if (_wcsicmp(relative, L"kernel.exe") == 0)
    {
        /* kernel.exe doesn't appear in the ramdisk */
        wcscpy(full, mod_kernel.name);
        return true;
    }

    l1 = wcslen(relative);
    for (ch = search; *ch != '\0'; ch += l2 + 1)
    {
        l2 = wcslen(ch);
        buf = malloc(sizeof(wchar_t) * (l1 + l2 + 2));
        if (buf == NULL)
            return false;

        wcscpy(buf, ch);
        wcscpy(buf + l2, L"/");
        wcscpy(buf + l2 + 1, relative);
        /*wprintf(L"PeFindFile: %s\n", buf);*/
        if (FsFullPath(buf, full) &&
            FsQueryFile(full, FILE_QUERY_NONE, NULL, 0))
        {
            free(buf);
            return true;
        }

        free(buf);
    }

    //wprintf(L"PeFindFile: unable to find %s\n", relative);
    return false;
}

//void PeInitImage(module_t *mod);

module_t* PeLoad(process_t* proc, const wchar_t* file, uint32_t base)
{
    static int nesting;
    static wchar_t full_file[256];

    handle_t fd;
    module_t* mod;
    IMAGE_DOS_HEADER dos;
    IMAGE_PE_HEADERS pe;
    addr_t new_base;
    size_t size;
    const wchar_t *temp;
    wchar_t *search_path, *ch;
    module_info_t *info;

    if (proc == NULL)
        proc = current()->process;

    KeAtomicInc(&nesting);

    temp = ProGetString(L"", L"LibrarySearchPath", L"/hd/boot,/System/Boot,.");
    search_path = malloc(sizeof(wchar_t) * (wcslen(temp) + 2));
    if (search_path == NULL)
        return NULL;

    wcscpy(search_path, temp);
    wcscat(search_path, L",");
    for (ch = search_path; *ch != '\0'; ch++)
        if (*ch == ',')
            *ch = '\0';

    if (file[0] == '/')
        wcsncpy(full_file, file, _countof(full_file) - 1);
    else if (!PeFindFile(file, full_file, search_path))
    {
        free(search_path);
        KeAtomicDec(&nesting);
        return NULL;
    }

    free(search_path);
    /*FsFullPath(file, full_file);*/

    /*if (_wcsicmp(full_file, mod_kernel.name) == 0)
    {
        mod_kernel.refs++;
        return &mod_kernel;
    }
    else*/
        FOREACH (mod, proc->mod)
            if (_wcsicmp(mod->name, full_file) == 0)
            {
                KeAtomicInc(&mod->refs);
                KeAtomicDec(&nesting);
                return mod;
            }

    //wprintf(L"%*sPeLoad: %s\n", nesting * 2, L"", full_file);
    fd = FsOpen(full_file, FILE_READ);
    if (!fd)
    {
        KeAtomicDec(&nesting);
        return NULL;
    }
    
    FsSeek(fd, 0, FILE_SEEK_SET);
    if (!FsReadSync(fd, &dos, sizeof(dos), &size) ||
        size < sizeof(dos))
    {
        wprintf(L"%s: only %d bytes read (%d)\n", file, size, errno);
        KeAtomicDec(&nesting);
        return NULL;
    }
    
    if (dos.e_magic != IMAGE_DOS_SIGNATURE)
    {
        wprintf(L"%s: not an executable (%S)\n", file, &dos);
        KeAtomicDec(&nesting);
        return NULL;
    }

    TRACE2("%s: e_lfanew = 0x%x\n", full_file, dos.e_lfanew);
    FsSeek(fd, dos.e_lfanew, FILE_SEEK_SET);
    if (!FsReadSync(fd, &pe, sizeof(pe), &size) ||
        size < sizeof(pe))
    {
        wprintf(L"%s: only %d bytes of PE header read (%d)\n", file, size, errno);
        KeAtomicDec(&nesting);
        return NULL;
    }

    if (pe.Signature != IMAGE_NT_SIGNATURE)
    {
        char *c = (char*) &pe.Signature;
        wprintf(L"%s: not PE format (%c%c%c%c)\n", full_file, 
            c[0], c[1], c[2], c[3]);
        KeAtomicDec(&nesting);
        return NULL;
    }

    mod = malloc(sizeof(module_t));
    mod->name = _wcsdup(full_file);

    if (base == NULL)
        base = pe.OptionalHeader.ImageBase;

#ifdef WIN32
    if (base >= 0x80000000)
        base -= 0x60000000;
#endif

    mod->refs = 1;
    mod->base = base;
    mod->length = pe.OptionalHeader.SizeOfImage;
    mod->entry = base + pe.OptionalHeader.AddressOfEntryPoint;
    mod->file = fd;
    mod->sizeof_headers = pe.OptionalHeader.SizeOfHeaders;
    mod->imported = false;

    new_base = (addr_t) VmmMap(PAGE_ALIGN_UP(mod->length) / PAGE_SIZE,
        base,
        mod,
        NULL,
        VM_AREA_IMAGE,
        VM_MEM_USER);

    assert(new_base == mod->base);

    /*wprintf(L"%s: %x..%x (pbase = %x)\n",
        file, mod->base, mod->base + mod->length, 
        pe.OptionalHeader.ImageBase);*/

    LIST_ADD(proc->mod, mod);

    if (mod->base < 0x80000000 &&
        proc->user_heap != NULL)
    {
        info = amalloc(proc->user_heap, sizeof(*info));
        wcscpy(info->name, full_file);
        info->base = mod->base;
        info->length = mod->length;
        LIST_ADD(proc->info->module, info);
    }

    PeGetHeaders(mod->base);

    //if (proc == current()->process)
        //PeInitImage(mod);

    KeAtomicDec(&nesting);
    return mod;
}

addr_t PeGetExport(module_t* mod, const char* name, uint16_t hint)
{
    IMAGE_PE_HEADERS* header;
    IMAGE_DATA_DIRECTORY* directories;
    IMAGE_EXPORT_DIRECTORY* exp;
    unsigned i;
    uint32_t *name_table, *function_table;
    uint16_t *ordinal_table;
    const char *export_name;
    
    header = PeGetHeaders(mod->base);
    directories = header->OptionalHeader.DataDirectory;
    
    if (directories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
    {
        wprintf(L"%s: no export directory looking for %S\n", 
            mod->name, name);
        return NULL;
    }

    exp = (IMAGE_EXPORT_DIRECTORY*) 
        (mod->base + directories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    if (exp->AddressOfNames == 0)
    {
        wprintf(L"%s: DLL has no exported name table (%S)\n",
            mod->name, name);
        return NULL;
    }

    name_table = (uint32_t*) (mod->base + (addr_t) exp->AddressOfNames);
    ordinal_table = (uint16_t*) (mod->base + (addr_t) exp->AddressOfNameOrdinals);
    function_table = (uint32_t*) (mod->base + (addr_t) exp->AddressOfFunctions);

    if (hint != (uint16_t) -1)
    {
        if (hint > exp->NumberOfNames)
            wprintf(L"PeGetExport(%s, %S): invalid hint %u (NumberOfNames = %u)\n",
                mod->name, name, hint, exp->NumberOfNames);
        else
        {
            export_name = (const char*) (mod->base + name_table[hint]);
            if (name_table[hint] == 0 ||
                _stricmp(export_name, name) == 0)
                return mod->base + function_table[ordinal_table[hint]];
        }

        /*wprintf(L"%s: hint %u for %S incorrect (found %S instead)\n", 
            mod->name, hint, name, export_name);*/
    }

    for (i = 0; i < exp->NumberOfNames; i++)
    {
        if (name_table[i] == 0)
            continue;

        export_name = (const char*) (mod->base + name_table[i]);
        if (_stricmp(export_name, name) == 0)
        {
            if (ordinal_table[i] > exp->NumberOfFunctions)
            {
                wprintf(L"%s: invalid ordinal %u (%S)\n",
                    mod->name, ordinal_table[i], name);
                return NULL;
            }

            return mod->base + function_table[ordinal_table[i]];
        }
    }

    wprintf(L"%s: export not found (%S)\n", mod->name, name);
    return NULL;
}

static bool PeDoImports(process_t* proc,
                        module_t* mod, 
                        const IMAGE_DATA_DIRECTORY *directories)
{
    IMAGE_IMPORT_DESCRIPTOR *imp;
    IMAGE_THUNK_DATA *thunk, *thunk2;
    IMAGE_IMPORT_BY_NAME *ibn;

    const char *name;
    module_t *other;
    wchar_t name_wide[16];
    size_t count;
    
    if (directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
        return true;

    imp = (IMAGE_IMPORT_DESCRIPTOR*) (mod->base + 
        directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (; imp->Name; imp++)
    {
        name = (const char*) (mod->base + (addr_t) imp->Name);
        
        count = mbstowcs(name_wide, name, _countof(name_wide));
        if (count == -1)
            continue;
        else
            name_wide[count] = '\0';

        if (imp->u.OriginalFirstThunk == 0)
            continue;

        other = PeLoad(proc, name_wide, 0);
        if (other == NULL)
        {
            //wprintf(L"%s: failed to load %s\n", mod->name, name_wide);
            return false;
        }

        thunk = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->u.OriginalFirstThunk);
        thunk2 = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->FirstThunk);
        for (; thunk->u1.AddressOfData; thunk++, thunk2++)
        {
            ibn = (IMAGE_IMPORT_BY_NAME*) (mod->base + (addr_t) thunk->u1.AddressOfData);
            thunk2->u1.Function = (uint32_t*) PeGetExport(other, ibn->Name, ibn->Hint);
            if (thunk2->u1.Function == NULL)
            {
                wprintf(L"%s: import from %s!%S failed\n", mod->name, name_wide, ibn->Name);
                return false;
            }
        }
    }
    
    return true;
}

void PeRelocateSection(module_t *mod, 
                       addr_t section_start_virtual, 
                       addr_t section_length)
{
    IMAGE_PE_HEADERS *pe;
    IMAGE_DATA_DIRECTORY* directories;
	const IMAGE_BASE_RELOCATION* rel;
	bool valid, found = false;
	uint32_t* addr;
	int adjust;

    pe = PeGetHeaders(mod->base);
    directories = pe->OptionalHeader.DataDirectory;
    adjust = mod->base - pe->OptionalHeader.ImageBase;
	if (directories[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
		adjust == 0)
		return;

	//wprintf(L"peRelocateSection: adjust = %d (%x)\n", adjust, adjust);
	rel = (const IMAGE_BASE_RELOCATION*) (mod->base + 
		directories[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	
	while (rel->VirtualAddress)
	{
		const unsigned long reloc_num = (rel->SizeOfBlock - sizeof(*rel)) / sizeof(uint16_t);
		unsigned i;
		uint16_t *ad = (void *)(rel + 1);

		valid = rel->VirtualAddress >= section_start_virtual &&
			rel->VirtualAddress < section_start_virtual + section_length;

		if (valid)
		{
			wprintf(L"\t%lu relocations starting at 0x%04lx\n", reloc_num, rel->VirtualAddress);
			found = true;
        }

		for (i = 0; i < reloc_num; i++, ad++)
			if (valid)
			{
				const wchar_t *type;
				switch (*ad >> 12)
				{
				case IMAGE_REL_BASED_ABSOLUTE:
					type = L"nop";
					break;
				case IMAGE_REL_BASED_HIGH:
					type = L"fix high";
					break;
				case IMAGE_REL_BASED_LOW:
					type = L"fix low";
					break;
				case IMAGE_REL_BASED_HIGHLOW:
					addr = (uint32_t*) ((*ad & 0xfffU) + rel->VirtualAddress + mod->base);
					//wprintf(L"hl(%p) %x = %x + %x\n", 
						//addr, *addr + adjust, *addr, adjust);
					*addr += adjust;
					type = L"fix hilo";
					break;
				case IMAGE_REL_BASED_HIGHADJ:
					type = L"fix highadj";
					break;
				case IMAGE_REL_BASED_MIPS_JMPADDR:
					type = L"jmpaddr";
					break;
				case IMAGE_REL_BASED_SECTION:
					type = L"section";
					break;
				case IMAGE_REL_BASED_REL32:
					type = L"fix rel32";
					break;
				default:
					type = L"???";
					break;
				}
				
				if (*ad >> 12 != IMAGE_REL_BASED_ABSOLUTE &&
					*ad >> 12 != IMAGE_REL_BASED_HIGHLOW)
					wprintf(L"Relocation not implemented: offset 0x%03x (%s)\n", *ad & 0xfffU, type);

                *ad = 0;
			}

		rel = (void *)ad;
	}
}

static IMAGE_SECTION_HEADER *PeFindSection(module_t *mod, IMAGE_PE_HEADERS *pe, addr_t addr)
{
    IMAGE_SECTION_HEADER *first_scn, *scn;
    unsigned i;

    first_scn = IMAGE_FIRST_SECTION(pe);
    scn = NULL;
    for (i = 0; i < pe->FileHeader.NumberOfSections; i++)
    {
        if ((first_scn[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) &&
            scn == NULL)
            scn = first_scn + i;
        else if (addr >= mod->base + first_scn[i].VirtualAddress &&
            addr < mod->base + first_scn[i].VirtualAddress + first_scn[i].Misc.VirtualSize)
        {
            scn = first_scn + i;
            break;
        }
    }

    return scn;
}

void PeProcessSection(module_t *mod, addr_t addr)
{
    IMAGE_PE_HEADERS *pe;
    IMAGE_SECTION_HEADER *scn;

    pe = PeGetHeaders(mod->base);
    if (!mod->imported)
        PeDoImports(current()->process, mod, pe->OptionalHeader.DataDirectory);

    if (addr > mod->base + mod->sizeof_headers)
    {
        scn = PeFindSection(mod, pe, addr);

        if (scn != NULL)
            PeRelocateSection(mod, 
                scn->VirtualAddress, 
                scn->Misc.VirtualSize);
    }
}

bool PeMapAddressToFile(module_t *mod, addr_t addr, uint64_t *off, 
                        size_t *bytes, uint32_t *flags)
{
    if (addr >= mod->base && 
        addr < mod->base + mod->sizeof_headers)
    {
        /* Map headers as a special case */
        *off = 0;
        *bytes = mod->sizeof_headers;
        *flags = VM_MEM_USER | VM_MEM_READ;
    }
    else
    {
        IMAGE_PE_HEADERS *pe;
        IMAGE_SECTION_HEADER *scn;
        addr_t scn_base;

        pe = PeGetHeaders(mod->base);
        scn = PeFindSection(mod, pe, addr);

        if (scn == NULL)
        {
            wprintf(L"%x: section not found\n", addr);
            return false;
        }

        scn_base = mod->base + scn->VirtualAddress;

        *flags = VM_MEM_ZERO;
        if (scn->Characteristics & IMAGE_SCN_MEM_READ)
            *flags |= VM_MEM_READ;
        if (scn->Characteristics & IMAGE_SCN_MEM_WRITE)
            *flags |= VM_MEM_WRITE;

        if ((scn->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) == 0)
            *off = scn->PointerToRawData + addr - scn_base;
        else
            *off = -1;

        if (*off < scn->PointerToRawData + scn->SizeOfRawData)
            *bytes = min(PAGE_SIZE, scn->PointerToRawData + scn->SizeOfRawData - *off);
        else
        {
            *flags |= VM_MEM_ZERO;
            *bytes = -1;
        }

        if (mod->base < 0x80000000)
            *flags |= VM_MEM_USER;
    }

    return true;
}

void PeUnload(process_t* proc, module_t* mod)
{
    KeAtomicDec(&mod->refs);

    if (mod->refs == 0)
    {
        if (mod->next)
            mod->next->prev = mod->prev;
        if (mod->prev)
            mod->prev->next = mod->next;

        if (proc->mod_last == mod)
            proc->mod_last = mod->prev;
        if (proc->mod_first == mod)
            proc->mod_first = mod->next;

        VmmFree((void*) mod->base);
        free(mod->name);
        free(mod);
    }
}
