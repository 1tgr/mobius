/* $Id: mod_pe.c,v 1.3 2002/01/05 00:54:11 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/fs.h>
#include <kernel/vmm.h>
#include <kernel/arch.h>

#include <os/pe.h>
#include <os/rtl.h>
#include <os/defs.h>

#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static IMAGE_PE_HEADERS* PeGetHeaders(addr_t base)
{
	IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*) base;
	return (IMAGE_PE_HEADERS*) (base + dos->e_lfanew);
}

module_t* PeLoad(process_t* proc, const wchar_t* file, uint32_t base)
{
	wchar_t full_file[256];
	handle_t fd;
	module_t* mod;
	IMAGE_DOS_HEADER dos;
	IMAGE_PE_HEADERS pe;
	vm_area_t *area;
	addr_t new_base;
	size_t size;
	
	FsFullPath(file, full_file);
	FOREACH (mod, proc->mod)
		if (_wcsicmp(mod->name, full_file) == 0)
		{
			mod->refs++;
			return mod;
		}
	
	fd = FsOpen(full_file, FILE_READ);
	if (!fd)
		return NULL;
	
	FsSeek(fd, 0);
	size = FsRead(fd, &dos, sizeof(dos));
	if (size < sizeof(dos))
	{
		wprintf(L"%s: only %d bytes read (%d)\n", file, size, errno);
		return NULL;
	}
	
	if (dos.e_magic != IMAGE_DOS_SIGNATURE)
	{
		wprintf(L"%s: not an executable (%S)\n", file, &dos);
		return NULL;
	}

	FsSeek(fd, dos.e_lfanew);
	FsRead(fd, &pe, sizeof(pe));

	if (pe.Signature != IMAGE_NT_SIGNATURE)
	{
		char *c = (char*) &pe.Signature;
		wprintf(L"%s: not PE format (%c%c%c%c)\n", file, 
			c[0], c[1], c[2], c[3]);
		return NULL;
	}

	mod = malloc(sizeof(module_t));
	mod->name = _wcsdup(full_file);

	if (base == NULL)
		base = pe.OptionalHeader.ImageBase;

	new_base = base;
	while ((area = VmmArea(proc, (const void*) new_base)))
	{
		new_base = (addr_t) area->start + area->pages * PAGE_SIZE;
		wprintf(L"%s: clashed with %08x, adjusting base to %08x\n", 
			file, area->start, new_base);
	}

	assert(new_base == base);

	mod->refs = 1;
	mod->base = base;
	mod->length = pe.OptionalHeader.SizeOfImage;
	mod->entry = base + pe.OptionalHeader.AddressOfEntryPoint;
	mod->file = fd;
	mod->sizeof_headers = pe.OptionalHeader.SizeOfHeaders;
	mod->imported = false;
	
	/*wprintf(L"%s: %x..%x (pbase = %x)\n",
		file, mod->base, mod->base + mod->length, 
		pe.OptionalHeader.ImageBase);*/

	LIST_ADD(proc->mod, mod);

	PeGetHeaders(mod->base);
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
		export_name = (const char*) (mod->base + name_table[hint]);
		if (name_table[hint] == 0 ||
			_stricmp(export_name, name) == 0)
			return mod->base + function_table[ordinal_table[hint]];

		wprintf(L"%s: hint %u for %S incorrect (found %S instead)\n", 
			mod->name, hint, name, export_name);
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
	wchar_t name_wide[256];
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
			wprintf(L"%s: failed to load %s\n", mod->name, name_wide);
			return false;
		}

		thunk = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->u.OriginalFirstThunk);
		thunk2 = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->FirstThunk);
		for (; thunk->u1.AddressOfData; thunk++, thunk2++)
		{
			ibn = (IMAGE_IMPORT_BY_NAME*) (mod->base + (addr_t) thunk->u1.AddressOfData);
			thunk2->u1.Function = (uint32_t*) PeGetExport(other, ibn->Name, ibn->Hint);
			if (thunk2->u1.Function == NULL)
				return false;
		}
	}
	
	return true;
}

bool PePageFault(process_t* proc, module_t* mod, addr_t addr)
{
	IMAGE_PE_HEADERS *pe;
	size_t size, raw_size;
	void *scn_base;
	uint32_t priv;
	addr_t raw_offset;
	
	if (addr < mod->base || addr >= mod->base + mod->length)
		return false;

	/*wprintf(L"pePageFault: %s at %p\n", mod->name, addr);*/
	
	if (addr >= mod->base && addr < mod->base + mod->sizeof_headers)
	{
		/*wprintf(L"%x: headers: %d bytes\n", addr, mod->sizeof_headers);*/

		/* Map headers as a special case */

		size = (mod->sizeof_headers + PAGE_SIZE - 1) & -PAGE_SIZE;
		raw_size = mod->sizeof_headers;
		priv = 3 | MEM_READ | MEM_COMMIT | MEM_ZERO;
		scn_base = (void*) mod->base;
		raw_offset = 0;
		pe = NULL;
	}
	else
	{
		IMAGE_SECTION_HEADER *first_scn, *scn;
		int i;
		
		pe = PeGetHeaders(mod->base);
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

		if (scn == NULL)
		{
			wprintf(L"%x: section not found\n", addr);
			return false;
		}

		scn_base = (void*) (mod->base + scn->VirtualAddress);
		size = (scn->Misc.VirtualSize + PAGE_SIZE - 1) & -PAGE_SIZE;
		/*wprintf(L"%x: section %d: %.8S %d bytes\n", addr, i, scn->Name, size);*/

		priv = 3 | MEM_COMMIT | MEM_ZERO;
		if (scn->Characteristics & IMAGE_SCN_MEM_READ)
			priv |= MEM_READ;
		if (scn->Characteristics & IMAGE_SCN_MEM_WRITE)
			priv |= MEM_WRITE;
		
		if ((scn->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) == 0)
			raw_offset = scn->PointerToRawData;
		else
			raw_offset = -1;

		raw_size = scn->SizeOfRawData;
	}

	if (VmmArea(proc, scn_base))
	{
		wprintf(L"Section already mapped\n");
		return false;
	}

	/*wprintf(L"%s: 0x%x bytes (= 0x%x bytes) at 0x%x\t", 
		mod->name, size, raw_size, scn_base);*/
	if (!VmmAlloc(size / PAGE_SIZE, (addr_t) scn_base, priv))
	{
		wprintf(L"%s: failed to allocate section\n", mod->name);
		return false;
	}

	if (raw_offset != (addr_t) -1)
	{
		FsSeek(mod->file, raw_offset);
		FsRead(mod->file, scn_base, raw_size);
	}
	
	if (!pe)
		pe = PeGetHeaders(mod->base);

	if (!mod->imported &&
		!PeDoImports(proc, mod, pe->OptionalHeader.DataDirectory))
	{
		wprintf(L"%s: imports failed\n", mod->name);
		return false;
	}

	return true;
}

void PeUnload(process_t* proc, module_t* mod)
{
	/*vm_area_t *area;*/

	mod->refs--;

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

		/* xxx - free all areas allocated */
		/*area = VmmArea(proc, (const void*) mod->base);
		VmmFree(proc, area);*/

		free(mod->name);
		free(mod);
	}
}
