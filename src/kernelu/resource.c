#include <os/os.h>
#include <os/pe.h>
#include <string.h>
#include <stdlib.h>

static const IMAGE_RESOURCE_DIRECTORY_ENTRY* resFindItem(dword base, 
	const IMAGE_RESOURCE_DIRECTORY* dir, const word* ids)
{
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
	int i;

	entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY*) (dir + 1);
	for (i = 0; i < dir->NumberOfNamedEntries + dir->NumberOfIdEntries; i++)
	{
		if (entry[i].u.Id == ids[0] || ids[0] == 0)
		{
			ids++;
			if (entry[i].u2.s.DataIsDirectory)
			{
				//wprintf(L"%x: Directory: offset to directory = %x\n", 
					//entry[i].Id, entry[i].OffsetToDirectory);
				dir = (const IMAGE_RESOURCE_DIRECTORY*) (base + entry[i].u2.s.OffsetToDirectory);
				return resFindItem(base, dir, ids);
			}
			else
			{
				//wprintf(L"%x: Resource: offset to data = %x\n", 
					//entry[i].Id, entry[i].OffsetToData);
				return entry + i;
			}
		}
	}

	//wprintf(L"%x: Not found\n", ids[0]);
	return NULL;
}
  
//! Finds a resource in the specified module.
/*!
 *	\param	base	The base address of the module where the resource is to be 
 *		found. Use _info.base for the current module, or another base address if 
 *		a different module is to be used.
 *
 *	\param	type	A pre-defined or numeric user-defined resource type.
 *	Pre-defined resource types are:
 *	- RT_CURSOR		A cursor at a specific resolution and colour depth
 *	- RT_BITMAP		A bitmap
 *	- RT_ICON		An icon at a specific resolution and colour depth
 *	- RT_MENU		A menu definition
 *	- RT_DIALOG		A dialog box definition
 *	- RT_STRING		A block of up to 16 strings (use resLoadString() to load a 
 *		specific string)
 *	- RT_FONTDIR	A list of fonts
 *	- RT_FONT		A font with a specific typeface, weight, size and attributes
 *	- RT_ACCEL		A list of keyboard accelerators
 *	- RT_RCDATA		Arbitrary binary data
 *	- RT_MSGTABLE	A table of error messages
 *	- RT_GROUP_CURSOR	A group of cursors which describe the same image but at
 *		different resolutions and colour depths
 *	- RT_GROUP_ICON	A group of icons which describe the same image but at
 *		different resolutions and colour depths
 *	- RT_VERSION	A version information block
 *	\param	id		The numeric identifier of the resource to be loaded.
 *	\param	language	The ID of the specific language of the resource to
 *		be loaded. Passing NULL for this parameter specifies the default 
 *		language for the process.
 *
 *	\note The Möbius only supports numeric resource IDs; string IDs (such
 *		as those used in Microsoft Windows) are not supported.
 *
 *	\return	A pointer to the start of the resource within the specified module,
 *		or NULL if the resource could not be found.
 *		Since the resource is contained within the image of the module in 
 *		memory, it is read-only (or it conforms to the attributes of the 
 *		PE section that contains it). There is also no corresponding function
 *		to free a loaded resource; all resources are freed when the process
 *		terminates.
 */
const void* resFind(dword base, word type, word id, word language)
{
	word ids[4] = { type, id, language, 0 };
	const IMAGE_DOS_HEADER* dos_head;
	const IMAGE_PE_HEADERS* header;
	const IMAGE_RESOURCE_DIRECTORY *dir;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
	const IMAGE_RESOURCE_DATA_ENTRY *data;
	
	if (!base)
		return NULL;

	dos_head = (const IMAGE_DOS_HEADER*) base;
	header = (const IMAGE_PE_HEADERS*) ((char *) dos_head + dos_head->e_lfanew);
	dir = (const IMAGE_RESOURCE_DIRECTORY*) 
		(base + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress);

	if ((dword) dir <= base)
		return NULL;

	entry = resFindItem((dword) dir, dir, ids);
	if (entry)
	{
		//wprintf(L"entry->OffsetToData = %x\n", entry->OffsetToData);
		data = (const IMAGE_RESOURCE_DATA_ENTRY*) ((byte*) dir + entry->u2.OffsetToData);
		//wprintf(L"data->OffsetToData = %x\n", data->OffsetToData);
		return (const void*) (base + data->OffsetToData);
	}
	else
		return NULL;
}

//! Loads a string from the specified module.
/*!
 *	\param	base	The base address of the module where the resource is to be 
 *		found. Use _info.base for the current module, or another base address if 
 *		a different module is to be used.
 *	\param	id		The numeric identifier of the string to be loaded.
 *	\param	str		String buffer to receive the loaded string.
 *	\param	str_max	Size, in characters, of the buffer pointed to by str.
 *
 *	\return	\p true if the string was found and loaded successfully; false 
 *		otherwise.
 *	\note	Due to a limitation of the PE resource file format, it is not 
 *		possible to distinguish between a zero-length string and a not-present
 *		string if there are any strings within the same block of 16.
 */
bool resLoadString(dword base, word id, wchar_t* str, size_t str_max)
{
	const wchar_t* buf;
	word i;

	buf = resFind(base, RT_STRING, (word) ((id >> 4) + 1), 0);
	if (buf)
	{
		id &= 15;

		for (i = 0; i < id; i++)
		{
			//wprintf(L"%x\t%s\n", buf[0], buf + 1);
			buf += buf[0] + 1;
		}

		wcsncpy(str, buf + 1, min((word) buf[0], str_max));
		return true;
	}
	else
		return false;
}

