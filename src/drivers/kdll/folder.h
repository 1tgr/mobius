#ifndef __FOLDER_H
#define __FOLDER_H

#include <os/filesys.h>

struct folderitem_ext_t : folderitem_t
{
	folderitem_ext_t* next;
	void* data;
	IUnknown* mount;
};

class CFolder : public IUnknown, public IFolder
{
public:
	CFolder();
	~CFolder();

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CFolder);

	STDMETHOD(FindFirst)(const wchar_t* spec, folderitem_t* buf);
	STDMETHOD(FindNext)(folderitem_t* buf);
	STDMETHOD(FindClose)(folderitem_t* pBuf);
	STDMETHOD(Open)(folderitem_t* item, const wchar_t* params);
	STDMETHOD(Mount)(const wchar_t* name, IUnknown* obj);

protected:
	folderitem_ext_t* m_item_first;
	bool m_scanned;

	virtual void ScanDir();
	virtual IUnknown* DoOpen(folderitem_ext_t* buf);
};

#endif