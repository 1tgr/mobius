#include <os/filesys.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "folder.h"

#if 0
#define TRACE   wprintf
#else

static int TRACE(...)
{
	return 0;
}

#endif

extern "C" bool wildcard(const wchar_t* str, const wchar_t* spec);

extern "C" IFolder* Folder_Create()
{
	return new CFolder;
}

CFolder::CFolder()
{
	m_refs = 0;
	m_item_first = NULL;
	m_scanned = false;
}

CFolder::~CFolder()
{
	folderitem_ext_t *item, *next;
	
	for (item = m_item_first; item; item = next)
	{
		next = item->next;

		if (item->mount)
			item->mount->Release();
			//IUnknown_Release(item->mount);

		item->mount = NULL;
		free(item->name);
		delete item;
	}
}

HRESULT CFolder::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IFolder))
	{
		AddRef();
		*ppvObject = (IFolder*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CFolder::FindFirst(const wchar_t* szSpec, folderitem_t* item)
{
	if (!m_scanned)
	{
		ScanDir();
		m_scanned = true;
	}

	item->spec = wcsdup(szSpec);
	item->u.find_handle = (dword) m_item_first;
	return S_OK;
}

HRESULT CFolder::FindNext(folderitem_t* item)
{
	folderitem_ext_t *fitem = (folderitem_ext_t*) item->u.find_handle;
	
	if (fitem == NULL)
		return E_FAIL;
	else
	{
		while (fitem)
		{
			if (wildcard(fitem->name, item->spec))
			{
				item->u.find_handle = (dword) fitem->next;

				wcsncpy(item->name, fitem->name, item->name_max);
				item->attributes = fitem->attributes;
				item->length = fitem->length;

				if (item->size == sizeof(folderitem_ext_t))
				{
					((folderitem_ext_t*) item)->next = fitem->next;
					((folderitem_ext_t*) item)->mount = fitem->mount;
					((folderitem_ext_t*) item)->data = fitem->data;
				}

				return S_OK;
			}

			fitem = fitem->next;
			item->u.find_handle = (dword) fitem;
		}

		return E_FAIL;
	}
}

HRESULT CFolder::FindClose(folderitem_t* pBuf)
{
	free((void*) pBuf->spec);
	return S_OK;
}

HRESULT CFolder::Open(folderitem_t* item, const wchar_t* params)
{
	folderitem_ext_t buf;
	IUnknown* pFile;
	wchar_t temp[MAX_PATH];
	
	TRACE(L"[OpenChild] %s\n", item->name);
	memset(&buf, 0, sizeof(buf));
	buf.size = sizeof(buf);
	buf.name = temp;
	buf.name_max = countof(temp);
	if (FAILED(FindFirst(item->name, &buf)) ||
		FAILED(FindNext(&buf)))
		return E_FAIL;

	pFile = DoOpen(&buf);
		
	if (item->size == sizeof(folderitem_ext_t))
		memcpy(item, &buf, sizeof(folderitem_ext_t));
	else
		*item = buf;

	item->u.item_handle = pFile;
	FindClose(&buf);
	return S_OK;
}

HRESULT CFolder::Mount(const wchar_t* name, IUnknown* obj)
{
	folderitem_ext_t* item;
	folderitem_t info;
	//IStream* strm;
	
	TRACE(L"CFolder::Mount(%s, %p)\n", name, obj);

	for (item = m_item_first; item; item = item->next)
	{
		if (wcsicmp(name, item->name) == 0)
		{
			if (item->mount)
				item->mount->Release();

			break;
		}
	}

	if (item == NULL)
	{
		item = new folderitem_ext_t;
		memset(item, 0, sizeof(item));
		
		item->next = m_item_first;
		m_item_first = item;
		item->name = wcsdup(name);
	}

	info.size = sizeof(info);
	info.name = NULL;
	info.name_max = 0;
	info.attributes = 0;
	info.length = 0;

	/*if (obj && SUCCEEDED(obj->QueryInterface(IID_IStream, (void**) &strm)))
	{
		strm->Stat(&info);
		strm->Release();
	}*/

	item->size = sizeof(folderitem_ext_t);
	item->attributes = ATTR_LINK | info.attributes;
	item->length = info.length;
	item->mount = obj;
	obj->AddRef();

	return S_OK;
}

void CFolder::ScanDir()
{
}

IUnknown* CFolder::DoOpen(folderitem_ext_t* buf)
{
	if (buf->mount)
	{
		buf->mount->AddRef();
		return buf->mount;
	}
	else
		return NULL;
}