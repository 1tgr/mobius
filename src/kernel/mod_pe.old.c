#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/vmm.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/thread.h>
#include <kernel/ramdisk.h>
#include <kernel/handle.h>
#include <os/os.h>

typedef struct CModulePe CModulePe;
//! A structure containing all the data used by a PE module
struct CModulePe
{
	//! IModule vtable
	IModule mod;
	//! IPager vtable
	IPager pager;
	//! Process which loaded this module
	process_t* proc;
	//! Reference count of this module
	dword refs,
		//! Address at which the module was loaded
		base,
		//! Address at which the module should be loaded
		pref_base,
		//! Entry point of the module
		entry;
	//! true if imports have been performed
	bool imported;
	//! Pointer to the start of the image data in the ramdisk
	byte* image_data;
	//! Base file name of the module
	wchar_t name[20];
};

#undef THIS
#define THIS	((CModulePe*) this)

HRESULT STDCALL CModulePe_QueryInterface(IModule* this, REFIID iid, void ** ppvObject)
{
	//_cputws(L"CModulePe_QueryInterface\n");
	if (InlineIsEqualGUID(iid, &IID_IUnknown) ||
		InlineIsEqualGUID(iid, &IID_IModule))
	{
		this->vtbl->AddRef(this);
		*ppvObject = &THIS->mod;
		return S_OK;
	}
	else if (InlineIsEqualGUID(iid, &IID_IPager))
	{
		this->vtbl->AddRef(this);
		*ppvObject = &THIS->pager;
		return S_OK;
	}
	else
		return E_FAIL;
}

ULONG STDCALL CModulePe_AddRef(IModule* this)
{
	return ++THIS->refs;
}

ULONG STDCALL CModulePe_Release(IModule* this)
{
	if (THIS->refs == 0)
	{
		hndFree(THIS);
		return 0;
	}
	else
		return THIS->refs--;
}

const void* STDCALL CModulePe_GetEntryPoint(IModule* this)
{
	return (const void*) THIS->entry;
}

const void* STDCALL CModulePe_GetBase(IModule* this)
{
	return (const void*) THIS->base;
}

const void* STDCALL CModulePe_GetProcAddress(IModule* this, const char* proc)
{
#define adr(p)	((const void*) (THIS->base + (addr_t) p))
	const IMAGE_DOS_HEADER* dos_head;
	IMAGE_PE_HEADERS* header;
	const IMAGE_DATA_DIRECTORY* directories;
	const IMAGE_EXPORT_DIRECTORY* exp;
	
	dos_head = (IMAGE_DOS_HEADER*) THIS->base;
	header = (IMAGE_PE_HEADERS*) ((char *) dos_head + dos_head->e_lfanew);
	directories = header->OptionalHeader.DataDirectory;

	if (directories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
		return NULL;

	exp = (const IMAGE_EXPORT_DIRECTORY*) 
		(THIS->base + directories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	{
        const word *ordinal_table = adr(exp->AddressOfNameOrdinals);
        const dword *function_table = adr(exp->AddressOfFunctions);
        const dword *name_table = exp->AddressOfNames ? adr(exp->AddressOfNames) : 0;
        size_t i;
        unsigned unused_slots = 0;  /* functions with an RVA of 0 seem to be unused */
		
		for (i = 0; i < exp->NumberOfFunctions; i++)
		{
			const DWORD addr = function_table[i];
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
						if (stricmp(adr(name_table[n]), proc) == 0)
							return (const void*) (addr + THIS->base);

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

			//wprintf(L"%6lx\n", addr);
		}
		
        if (unused_slots)
            wprintf(L"\t-- there are %u unused slots --\n", unused_slots);
    }

	return NULL;
#undef adr
}

HRESULT STDCALL CModulePe_GetName(IModule* this, wchar_t* name, size_t max)
{
	wcscpy(name, THIS->name);
	return S_OK;
}

#undef THIS
#define THIS	((CModulePe*) (this - 1))

HRESULT STDCALL CModulePe_Pager_QueryInterface(IPager* this, REFIID iid, void ** ppvObject)
{
	return THIS->mod.vtbl->QueryInterface(&THIS->mod, iid, ppvObject);
}

ULONG STDCALL CModulePe_Pager_AddRef(IPager* this)
{
	return THIS->mod.vtbl->AddRef(&THIS->mod);
}

ULONG STDCALL CModulePe_Pager_Release(IPager* this)
{
	return THIS->mod.vtbl->Release(&THIS->mod);
}

#define adr(p)	((const void*) (this->base + (addr_t) p))
HRESULT peImport(CModulePe* this, 
				 const IMAGE_DATA_DIRECTORY *directories, 
				 void *section_data,
				 dword section_start_virtual, dword section_length)
{
	const IMAGE_IMPORT_DESCRIPTOR * imp;
	const IMAGE_THUNK_DATA *import_entry;
	IMAGE_THUNK_DATA *mapped_entry;
	const char* name;
	enum { bound_none, bound_old, bound_new } bound;
	wchar_t temp[256];
	IModule* mod;
	dword args[3];
	
	if (directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
		return S_OK;

	imp = (const IMAGE_IMPORT_DESCRIPTOR*) (this->base + 
		directories[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	this->imported = true;

	for (; imp->Name; imp++)
	{
		name = (const char*) adr(imp->Name);
		
		if (imp->TimeDateStamp == ~0UL)
			bound = bound_new;
		else if (imp->TimeDateStamp)
			bound = bound_old;
		else
			bound = bound_none;
		
		if (imp->u.OriginalFirstThunk)
		{
			import_entry = adr(imp->u.OriginalFirstThunk);
			mapped_entry = (IMAGE_THUNK_DATA*) adr(imp->FirstThunk);
		}
		else
		{
			_cputws(L"\t(hint table missing, probably Borland bug)");
			import_entry = adr(imp->FirstThunk);
			mapped_entry = 0;
			bound = bound_none;
		}
		
		mbstowcs(temp, name, countof(temp));
		//wprintf(L"Loading library %s...", temp);
		mod = modLoadPe(this->proc, temp, 0);

		//if (mod)
			//wprintf(L"done\n");
		if (!mod)
		{
			wprintf(L"Failed to load %s\n", temp);
			THIS->imported = false;
			exception(current, NULL, EXCEPTION_MISSING_DLL, (dword) name);
			return S_OK;
			//return E_FAIL;
		}
		
		args[0] = (dword) mod->vtbl->GetBase(mod);
		args[1] = 0;
		args[2] = 0;
		thrCall(current, (void*) mod->vtbl->GetEntryPoint(mod), args, sizeof(args));

		{
			int count, nextforwarder = bound==bound_old ? imp->ForwarderChain : -1;
			for (count = 0; import_entry->u1.Ordinal; count++, import_entry++, 
				/*bound ?*/ mapped_entry++/* : 0*/) // xxx - what is this?
			{
				if (IMAGE_SNAP_BY_ORDINAL(import_entry->u1.Ordinal))
					wprintf(L"\t%6lu %20S", IMAGE_ORDINAL(import_entry->u1.Ordinal),"<ordinal>");
				else
				{
					const IMAGE_IMPORT_BY_NAME *name_import = adr(import_entry->u1.AddressOfData);
					//wprintf(L"\t%6u %20S\n", name_import->Hint, name_import->Name);

					//mapped_entry->u1.Function = this->dummy_import;
					
					mapped_entry->u1.Function = 
						(dword*) mod->vtbl->GetProcAddress(mod, name_import->Name);

					if (mapped_entry->u1.Function == NULL)
					{
						wprintf(L"Failed to access %S from %s\n", name_import->Name, temp);
						THIS->imported = false;
						exception(current, NULL, EXCEPTION_MISSING_IMPORT, (dword) name_import->Name);
						return S_OK;
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

	return S_OK;
}
#undef adr

HRESULT peRelocateSection(CModulePe* this, 
						  const IMAGE_DATA_DIRECTORY *directories, 
						  void *section_data,
						  dword section_start_virtual, dword section_length)
{
	const IMAGE_BASE_RELOCATION* rel;
	bool valid, found = false;
	dword* addr;
	int adjust = this->base - this->pref_base;
	
	if (directories[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
		adjust == 0)
		return S_OK;

	//wprintf(L"peRelocateSection: adjust = %d (%x)\n", adjust, adjust);
	rel = (const IMAGE_BASE_RELOCATION*) (this->base + 
		directories[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	
	while (rel->VirtualAddress)
	{
		const unsigned long reloc_num = (rel->SizeOfBlock - sizeof(*rel)) / sizeof(word);
		unsigned i;
		const word *ad = (void *)(rel + 1);

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
					addr = (dword*) ((*ad & 0xfffU) + rel->VirtualAddress + this->base);
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
			}

		rel = (void *)ad;
	}

	return S_OK;
}

HRESULT STDCALL CModulePe_ValidatePage(IPager* this, const void* virt)
{
	static int level;

	const IMAGE_DOS_HEADER* dos_head;
	IMAGE_PE_HEADERS* header;
	addr_t page;
	int i, section, pages;
	void *sect_virt;
	IMAGE_SECTION_HEADER *section_header, *sections;
	dword priv;
	HRESULT hr;
	module_info_t* info;

	level++;
	wprintf(L"[%d] CModulePe_ValidatePage(%s, %p)...", level, THIS->name, virt);

	priv = THIS->proc->level;
	if (memTranslate(THIS->proc->page_dir, (void*) THIS->base) == 0)
	{
		/* it's the headers (hopefully) */
		sect_virt = vmmAlloc(THIS->proc, 1, THIS->base, 
			priv | MEM_COMMIT | MEM_ZERO | MEM_READ | MEM_WRITE);

		if (!sect_virt)
			return E_FAIL;

		memcpy(sect_virt, THIS->image_data, PAGE_SIZE);
		_cputws(L"headers\n");
	}

	section = -1;
	dos_head = (IMAGE_DOS_HEADER*) THIS->base;
	page = ((addr_t) virt & -PAGE_SIZE) - THIS->base;
	header = (IMAGE_PE_HEADERS*) ((char *) dos_head + dos_head->e_lfanew);

	if ((addr_t) virt < THIS->base ||
		(addr_t) virt > THIS->base + header->OptionalHeader.SizeOfImage)
	{
		//wprintf(L"outside %x - %x\n", THIS->base, THIS->base + header->OptionalHeader.SizeOfImage);
		_cputws(L"no\n");
		level--;
		return E_FAIL;
	}
	
	if (memTranslate(THIS->proc->page_dir, (void*) (page + THIS->base)))
	{
		wprintf(L"CModulePe_ValidatePage: page at %x already mapped\n", page + THIS->base);
		level--;
		return S_OK;
	}

	sections = IMAGE_FIRST_SECTION(header);
	for (i = 0; i < header->FileHeader.NumberOfSections; i++)
	{
		/*wprintf(L"%8S\t%x -> %x\n", header->section_header[i].Name, 
			header->section_header[i].VirtualAddress,
			header->section_header[i].VirtualAddress + 
				header->section_header[i].Misc.VirtualSize);*/

		if (page >= sections[i].VirtualAddress &&
			(page < sections[i].VirtualAddress + 
				sections[i].Misc.VirtualSize ||
			((sections[i].Characteristics & 
				IMAGE_SCN_CNT_UNINITIALIZED_DATA) && section == -1)))
		{
			/* the bss seems to be stuck onto the end of the data section,
				at least with the Microsoft linker */
			section = i;
			break;
		}
	}

	if (section == -1)
	{
		wprintf(L"CModulePe_ValidatePage: no section found at %x\n", THIS->base + page);
		level--;
		return E_FAIL;
	}

	wprintf(L"ok\n");
	section_header = sections + section;
	sect_virt = (void*) (section_header->VirtualAddress + THIS->base);
	
	priv = THIS->proc->level | MEM_COMMIT /*| MEM_ZERO*/;
	if (section_header->Characteristics & IMAGE_SCN_MEM_READ)
		priv |= MEM_READ;

	if (section_header->Characteristics & IMAGE_SCN_MEM_WRITE)
		priv |= MEM_WRITE;

	//if (section_header->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
		//pages = header->opt_head.SizeOfUninitializedData / PAGE_SIZE + 1;
	//else
		pages = section_header->Misc.VirtualSize / PAGE_SIZE + 1;

	wprintf(L"[%d] Paging in section at %p (%d page(s))\n", level, sect_virt, pages);
	
	/*phys = memAlloc(pages);
	if (!phys)
		return E_FAIL;
	
	memMap(THIS->proc->page_dir, (addr_t) sect_virt, (addr_t) phys, pages, priv);
	for (i = 0; i < pages; i++)
		invalidate_page((byte*) sect_virt + i * PAGE_SIZE);

	//if (section_header->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
	i386_lmemset32((addr_t) phys, 0, pages * PAGE_SIZE);*/

	if (!vmmAlloc(THIS->proc, pages, (addr_t) sect_virt, priv))
		return E_FAIL;

	//else
	memcpy(sect_virt, THIS->image_data + section_header->PointerToRawData, 
		section_header->SizeOfRawData);

	if (!THIS->imported)
	{
		hr = peImport(THIS, header->OptionalHeader.DataDirectory, sect_virt,
			section_header->VirtualAddress,
			section_header->SizeOfRawData);
		if (FAILED(hr))
			return hr;
	}

	hr = peRelocateSection(THIS, header->OptionalHeader.DataDirectory, sect_virt,
		section_header->VirtualAddress,
		section_header->SizeOfRawData);
	if (FAILED(hr))
		return hr;
	
	if (strncmp(section_header->Name, ".info", 8) == 0)
	{
		info = (module_info_t*) sect_virt;
		info->loader_version = 0x0100;
		info->next = NULL;
		info->base = THIS->base;
		info->length = header->OptionalHeader.SizeOfImage;
		wcscpy(info->name, THIS->name);

		if (THIS->proc->module_last)
			THIS->proc->module_last->next = info;

		THIS->proc->module_last = info;

		if (!THIS->proc->info->module_first)
			THIS->proc->info->module_first = info;
	}

	wprintf(L"[%d] CModulePe: done\n", level);
	level--;
	return S_OK;
}

const struct IModuleVtbl CModulePe_IModule_vtbl =
{
	CModulePe_QueryInterface,
	CModulePe_AddRef,
	CModulePe_Release,
	CModulePe_GetEntryPoint,
	CModulePe_GetBase,
	CModulePe_GetProcAddress,
	CModulePe_GetName
};

const struct IPagerVtbl CModulePe_IPager_vtbl =
{
	CModulePe_Pager_QueryInterface,
	CModulePe_Pager_AddRef,
	CModulePe_Pager_Release,
	CModulePe_ValidatePage
};

IModule* modLoadPeMemory(process_t* proc, const wchar_t* file, const void* ptr, dword base)
{
	const IMAGE_DOS_HEADER* dos_head;
	const IMAGE_PE_HEADERS *header;
	CModulePe* mod;
	
	dos_head = (IMAGE_DOS_HEADER*) ptr;
	//wprintf(L"file = %s, header = %p\n", file, dos_head);
	if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
	{
		wprintf(L"unknown type of file: %c%c%c%c%c%c (ptr = %p)\n", 
			((byte*) ptr)[0], 
			((byte*) ptr)[1], 
			((byte*) ptr)[2], 
			((byte*) ptr)[3], 
			((byte*) ptr)[4], 
			((byte*) ptr)[5], 
			dos_head, ptr);
		return NULL;
	}
	
	header = (const IMAGE_PE_HEADERS*) ((byte*) dos_head + dos_head->e_lfanew);

	if (header->Signature != IMAGE_NT_SIGNATURE)
	{
		switch ((unsigned short)header->Signature)
		{
		case IMAGE_DOS_SIGNATURE:
			_cputws(L"(MS-DOS signature)");
			break;
		case IMAGE_OS2_SIGNATURE:
			_cputws(L"(Win16 or OS/2 signature)");
			break;
		case IMAGE_OS2_SIGNATURE_LE:
			_cputws(L"(Win16, OS/2 or VxD signature)");
			break;
		default:
			_cputws(L"(unknown signature, probably MS-DOS)");
			break;
		}

		return NULL;
	}

	mod = hndAlloc(sizeof(CModulePe), proc);
	if (!mod)
		return NULL;

	mod->refs = 0;
	mod->mod.vtbl = &CModulePe_IModule_vtbl;
	mod->pager.vtbl = &CModulePe_IPager_vtbl;

	wcscpy(mod->name, file);
	mod->base = NULL;

	mod->image_data = (byte*) ptr;
	mod->pref_base = header->OptionalHeader.ImageBase;

	if (base)
		mod->base = base;
	else
		mod->base = header->OptionalHeader.ImageBase;
	
	/*wprintf(L"%s: headers = %p, base = %x, prefered base = %x\n", 
		file, header, base, header->OptionalHeader.ImageBase);
	wprintf(L"sizeof(FileHeader) = %d (should be 20)\n", sizeof(header->FileHeader));
	wprintf(L"offsetof(OptionalHeader) = %d (should be 24)\n", 
		(byte*) &header->OptionalHeader - (byte*) header);*/

	mod->proc = proc;
	mod->entry = mod->base + header->OptionalHeader.AddressOfEntryPoint;
	wcscpy(mod->name, file);
	mod->imported = false;

	proc->modules[proc->num_modules] = &mod->mod;
	proc->num_modules++;
	return &mod->mod;
}

//! Loads the module specified into the kernel, preferably at the 
//!		specified base address.
/*!
 *	\param	proc	The process into which the module will be loaded. At 
 *		time, the function may not be executing in the context of this
 *		process (such as when the module is being used as the core 
 *		executable).
 *	\param	file	The name of the file containing the module.
 *	\param	base	The base address where the module should be loaded. If 
 *		this is NULL then the module's preferred load address may be used.
 *	\return	S_OK if the module was loaded successfuly, or a failure code.
 */
IModule* modLoadPe(process_t* proc, const wchar_t* file, dword base)
{
	void* ptr;
	int i;
	wchar_t name[20];

	for (i = 0; i < proc->num_modules; i++)
	{
		proc->modules[i]->vtbl->GetName(proc->modules[i], name, countof(name));
		if (wcsicmp(name, file) == 0)
		{
			//wprintf(L"%s: already loaded\n", file);
			return proc->modules[i];
		}
	}

	ptr = ramOpen(file);
	if (!ptr)
	{
		//wprintf(L"%s not found in ramdisk\n", file);
		return NULL;
	}

	return modLoadPeMemory(proc, file, ptr, base);
}
