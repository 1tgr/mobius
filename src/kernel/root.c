#include <kernel/kernel.h>
#include <os/filesys.h>

typedef struct CFolderRoot CFolderRoot;
struct CFolderRoot
{
	IFolder folder;
	dword refs;
};

#undef THIS
#define THIS ((CFolderRoot*) this)

HRESULT CFolderRoot_QueryInterface(IFolder* this, REFIID iid, void ** ppvObject)
{
	return E_FAIL;
};

ULONG CFolderRoot_AddRef(IFolder* this)
{
	return 0;
}

ULONG CFolderRoot_Release(IFolder* this)
{
	return 0;
}

HRESULT CFolderRoot_FindFirst(IFolder* this, const wchar_t* spec, folderitem_t* buf)
{
	return E_FAIL;
}

HRESULT CFolderRoot_FindNext(IFolder* this, folderitem_t* buf)
{
	return E_FAIL;
}

HRESULT CFolderRoot_Open(IFolder* this, folderitem_t* item)
{
	return E_FAIL;
}

const struct IFolderVtbl CFolderRoot_vtbl =
{
	CFolderRoot_QueryInterface,
	CFolderRoot_AddRef,
	CFolderRoot_Release,
	CFolderRoot_FindFirst,
	CFolderRoot_FindNext,
	CFolderRoot_Open
};

CFolderRoot root_folder = { { &CFolderRoot_vtbl }, 0 };
IFolder* root = &root_folder.folder;
