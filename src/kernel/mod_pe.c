#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/ramdisk.h>
#include <kernel/vmm.h>
#include <kernel/handle.h>
#include <kernel/thread.h>
#include <os/pe.h>
#include <string.h>
#include <stdlib.h>

module_t* peLoad(process_t* proc, const wchar_t* file, dword base)
{
	void* data;
	module_t* mod;

	for (mod = proc->mod_first; mod; mod = mod->next)
		if (wcsicmp(mod->name, file) == 0)
			return mod;

	data = ramOpen(file);
	if (!data)
		return NULL;
	
	return peLoadMemory(proc, file, data, base);
}

module_t* peLoadMemory(process_t* proc, const wchar_t* file, const void* ptr, dword base)
{
	module_t *mod;
	IMAGE_DOS_HEADER *dos;
	IMAGE_PE_HEADERS *pe;
	vm_area_t *area;
	addr_t new_base;

	dos = (IMAGE_DOS_HEADER*) ptr;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		wprintf(L"%s: not an executable\n", file);
		return NULL;
	}

	pe = (IMAGE_PE_HEADERS*) ((byte*) ptr + dos->e_lfanew);
	if (pe->Signature != IMAGE_NT_SIGNATURE)
	{
		wprintf(L"%s: not PE format\n", file);
		return NULL;
	}

	mod = hndAlloc(sizeof(module_t), proc);
	mod->name = wcsdup(file);

	if (base == NULL)
		base = pe->OptionalHeader.ImageBase;

	new_base = base;
	while ((area = vmmArea(proc, (const void*) new_base)))
	{
		new_base = (addr_t) area->start + area->pages * PAGE_SIZE;
		wprintf(L"%s: clashed with %08x, adjusting base to %08x\n", 
			file, area->start, new_base);
	}

	assert(new_base == base);

	mod->base = base;
	mod->length = pe->OptionalHeader.SizeOfImage;
	mod->prev = proc->mod_last;
	mod->next = NULL;
	mod->entry = base + pe->OptionalHeader.AddressOfEntryPoint;
	mod->raw_data = (void*) ptr;
	mod->sizeof_headers = pe->OptionalHeader.SizeOfHeaders;
	mod->imported = false;

	/*wprintf(L"%s: %x..%x (pbase = %x)\n",
		file, mod->base, mod->base + mod->length, 
		pe->OptionalHeader.ImageBase);*/

	if (proc->mod_last)
		proc->mod_last->next = mod;
	if (proc->mod_first == NULL)
		proc->mod_first = mod;
	proc->mod_last = mod;
	
	/*area = vmmAlloc(proc, mod->length / PAGE_SIZE, base, 0);
	if (area == NULL ||
		(addr_t) area->start != base)
	{
		vmmFree(proc, area);
		return NULL;
	}*/

	return mod;
}

static IMAGE_PE_HEADERS* peGetHeaders(addr_t base)
{
	IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*) base;
	return (IMAGE_PE_HEADERS*) (base + dos->e_lfanew);
}

static bool peDoImports(process_t* proc,
						module_t* mod, 
						const IMAGE_DATA_DIRECTORY *directories)
{
	IMAGE_IMPORT_DESCRIPTOR * imp;
	IMAGE_THUNK_DATA *import_entry;
	IMAGE_THUNK_DATA *mapped_entry;
	const char* name;
	enum { bound_none, bound_old, bound_new } bound;
	wchar_t temp[256];
	module_t* newmod;
	dword args[3];
	
	if (directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
		return true;

	imp = (IMAGE_IMPORT_DESCRIPTOR*) (mod->base + 
		directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	mod->imported = true;
	for (; imp->Name; imp++)
	{
		if (imp->Name >= mod->length)
		{
			//wprintf(L"%s: name %d is invalid (%x)\n",
				//mod->name, imp - (IMAGE_IMPORT_DESCRIPTOR*) (mod->base + 
					//directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress),
				//imp->Name);
			continue;
		}

		name = (const char*) (mod->base + (addr_t) imp->Name);
		
		if (imp->TimeDateStamp == ~0UL)
			bound = bound_new;
		else if (imp->TimeDateStamp)
			bound = bound_old;
		else
			bound = bound_none;
		
		if (imp->u.OriginalFirstThunk)
		{
			import_entry = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->u.OriginalFirstThunk);
			mapped_entry = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->FirstThunk);
		}
		else
		{
			_cputws(L"\t(hint table missing, probably Borland bug)");
			import_entry = (IMAGE_THUNK_DATA*) (mod->base + (addr_t) imp->FirstThunk);
			mapped_entry = 0;
			bound = bound_none;
		}
		
		//wprintf(L"base = %x, name = %x\n", mod->base, imp->Name);
		mbstowcs(temp, name, countof(temp));
		//wprintf(L"Loading library %s...", temp);
		newmod = peLoad(proc, temp, 0);

		//if (newmod)
			//wprintf(L"done\n");
		if (!newmod)
		{
			wprintf(L"Failed to load %s\n", temp);
			mod->imported = false;
			exception(current, NULL, EXCEPTION_MISSING_DLL, (dword) name);
			return true;
			//return E_FAIL;
		}
		
		args[0] = newmod->base;
		args[1] = 0;
		args[2] = 0;
		//thrCall(current, (void*) newmod->entry, args, sizeof(args));

		{
			int count, nextforwarder = bound==bound_old ? imp->ForwarderChain : -1;
			for (count = 0; import_entry->u1.Ordinal; count++, import_entry++, 
				/*bound ?*/ mapped_entry++/* : 0*/) // xxx - what is this?
			{
				if (IMAGE_SNAP_BY_ORDINAL(import_entry->u1.Ordinal))
					wprintf(L"\t%6lu %20S", IMAGE_ORDINAL(import_entry->u1.Ordinal),"<ordinal>");
				else
				{
					IMAGE_IMPORT_BY_NAME *name_import = (IMAGE_IMPORT_BY_NAME*) 
						(mod->base + (addr_t) import_entry->u1.AddressOfData);
					//wprintf(L"\t%6u %20S\n", name_import->Hint, name_import->Name);

					mapped_entry->u1.Function = 
						(dword*) peGetExport(newmod, name_import->Name);

					if (mapped_entry->u1.Function == NULL)
					{
						wprintf(L"Failed to access %S from %s\n", name_import->Name, temp);
						mod->imported = false;
						exception(current, NULL, EXCEPTION_MISSING_IMPORT, (dword) name_import->Name);
						return true;
						//return E_FAIL;
					}

					//wprintf(L"GetProcAddress returns %p\n", mapped_entry->u1.Function);
				}

				if (bound)
				{
					if (count != nextforwarder)
						wprintf(L"\t0x%12lx\n", (unsigned long)mapped_entry->u1.Function);
					else
					{
						wprintf(L"\t%12S\n", L"    --> forward");
						nextforwarder = (int)mapped_entry->u1.ForwarderString;
					}
                    //else
                        //_cputws(L"\n");
				}
			}
		}
	}

	return true;
}

bool pePageFault(process_t* proc, module_t* mod, addr_t addr)
{
	IMAGE_PE_HEADERS *pe;
	size_t size, raw_size;
	void *scn_base, *raw_data;
	dword priv;
	
	//if (wcsicmp(mod->name, L"kdebug.dll") == 0)
		//wprintf(L"%s: base = %08x length = %08x\n", mod->name, mod->base, mod->length);

	if (addr < mod->base || addr >= mod->base + mod->length)
		return false;

	//wprintf(L"pePageFault: %s at %p\n", mod->name, addr);
	if (addr >= mod->base && addr < mod->base + mod->sizeof_headers)
	{
		//wprintf(L"%x: headers: %d bytes\n", addr, mod->sizeof_headers);
		size = (mod->sizeof_headers + PAGE_SIZE - 1) & -PAGE_SIZE;
		raw_size = mod->sizeof_headers;
		priv = proc->level | MEM_READ | MEM_COMMIT | MEM_ZERO;
		scn_base = (void*) mod->base;
		raw_data = mod->raw_data;
		pe = NULL;
	}
	else
	{
		IMAGE_SECTION_HEADER *first_scn, *scn;
		int i;
		
		pe = peGetHeaders(mod->base);
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
		//wprintf(L"%x: section %d: %8S %d bytes\n", addr, i, scn->Name, size);

		priv = proc->level | MEM_COMMIT | MEM_ZERO;
		if (scn->Characteristics & IMAGE_SCN_MEM_READ)
			priv |= MEM_READ;
		if (scn->Characteristics & IMAGE_SCN_MEM_WRITE)
			priv |= MEM_WRITE;
		
		if ((scn->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) == 0)
			raw_data = (byte*) mod->raw_data + scn->PointerToRawData;
		else
			raw_data = NULL;

		raw_size = scn->SizeOfRawData;
	}

	if (vmmArea(proc, scn_base))
	{
		wprintf(L"Section already mapped\n");
		return false;
	}

	if (!vmmAlloc(proc, size / PAGE_SIZE, (addr_t) scn_base, priv))
		return false;

	if (raw_data)
	{
		//wprintf(L"Copying %d bytes from %x\n", raw_size, raw_data);
		memcpy(scn_base, raw_data, raw_size);
	}
	//else
		//wprintf(L"Uninitialized data\n");

	if (!pe)
		pe = peGetHeaders(mod->base);

	if (!mod->imported &&
		!peDoImports(proc, mod, pe->OptionalHeader.DataDirectory))
	{
		wprintf(L"%s: imports failed\n", mod->name);
		return false;
	}

	return true;
}

addr_t peGetExport(module_t* mod, const char* name)
{
	IMAGE_PE_HEADERS* header;
	IMAGE_DATA_DIRECTORY* directories;
	IMAGE_EXPORT_DIRECTORY* exp;
	word *ordinal_table;
    dword *function_table;
    dword *name_table;
    size_t i;
    unsigned unused_slots = 0;  /* functions with an RVA of 0 seem to be unused */
	
	header = peGetHeaders(mod->base);
	directories = header->OptionalHeader.DataDirectory;

	if (directories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
		return NULL;

	exp = (IMAGE_EXPORT_DIRECTORY*) 
		(mod->base + directories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	ordinal_table = (word*) (mod->base + (addr_t) exp->AddressOfNameOrdinals);
	function_table = (dword*) (mod->base + (addr_t) exp->AddressOfFunctions);
	name_table = exp->AddressOfNames ? (dword*) (mod->base + (addr_t) exp->AddressOfNames) : 0;

	for (i = 0; i < exp->NumberOfFunctions; i++)
	{
		dword addr = function_table[i];
		if (!addr)        /* skip unused slots */
		{
			++unused_slots;
			continue;
		}
		
		/* if names provided, list all names of this
		* export ordinal
		*/
		if (name_table)
		{
			size_t n;
			int found = 0;
			for (n = 0; n < exp->NumberOfNames; n++)
			{
				if (ordinal_table[n] == i)
				{
					if (stricmp((const char*) (mod->base + name_table[n]), name) == 0)
						return addr + mod->base;

					++found;
				}
			}
		}
		
		/* entry point */
		//if (addr >= section_base_virtual && addr < section_base_virtual + section_length)
			/* forwarder */
			//wprintf(L"%S", adr(addr));
		//else
			/* normal export */
	}
	
    if (unused_slots)
        wprintf(L"\t-- there are %u unused slots --\n", unused_slots);
    
	return NULL;
}

void peUnload(process_t* proc, module_t* mod)
{
	//vm_area_t *area;

	if (mod->next)
		mod->next->prev = mod->prev;
	if (mod->prev)
		mod->prev->next = mod->next;

	if (proc->mod_last == mod)
		proc->mod_last = mod->prev;
	if (proc->mod_first == mod)
		proc->mod_first = mod->next;

	/* xxx - free all areas allocated */
	//area = vmmArea(proc, (const void*) mod->base);
	//vmmFree(proc, area);

	free(mod->name);
	hndFree(mod);
}
