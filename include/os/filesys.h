#ifndef __FILESYS_H
#define __FILESYS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <os/stream.h>

#undef INTERFACE
#define INTERFACE	IFolder
DECLARE_INTERFACE(IFolder)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(FindFirst)(THIS_ const wchar_t* spec, folderitem_t* buf) PURE;
	STDMETHOD(FindNext)(THIS_ folderitem_t* buf) PURE;
	STDMETHOD(FindClose)(THIS_ folderitem_t* pBuf) PURE;
	STDMETHOD(Open)(THIS_ folderitem_t* item, const wchar_t* params) PURE;
	STDMETHOD(Mount)(THIS_ const wchar_t* name, IUnknown* obj) PURE;
};

HRESULT IFolder_FindFirst(IFolder* ptr, const wchar_t* spec, folderitem_t* buf);
HRESULT IFolder_FindNext(IFolder* ptr, folderitem_t* buf);
HRESULT IFolder_FindClose(IFolder* ptr, folderitem_t* pBuf);
HRESULT IFolder_Open(IFolder* ptr, folderitem_t* item, const wchar_t* params);
HRESULT IFolder_Mount(IFolder* ptr, const wchar_t* name, IUnknown* obj);

// {816B7D56-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IFolder, 
0x816b7d56, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

#ifdef __cplusplus
}
#endif

#endif