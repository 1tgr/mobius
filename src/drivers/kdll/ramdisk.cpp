// ramdisk.cpp: implementation of the CRamFolder class.
//
//////////////////////////////////////////////////////////////////////

#include "ramdisk.h"
#include "folder.h"
#include <kernel/ramdisk.h>
#include <string.h>
#include <stdlib.h>

#define RAMDISK_ADDR	0xd0000000

class CRamFolder : public CFolder
{
public:
	CRamFolder();
	virtual ~CRamFolder();

protected:
	virtual void ScanDir();
	virtual IUnknown* DoOpen(folderitem_ext_t* buf);
};

class CRamFile : public IUnknown, public IStream
{
public:
	CRamFile(void* data, ramfile_t* file);

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CFatFile);
	
	STDMETHOD_(size_t, Read) (void* pBuffer, size_t dwLength);
	STDMETHOD_(size_t, Write) (const void* pBuffer, size_t dwLength);
	STDMETHOD(SetIoMode)(dword mode);
	STDMETHOD(IsReady)();
	STDMETHOD(Stat)(folderitem_t* buf);
	STDMETHOD(Seek)(THIS long offset, int origin);

protected:
	byte* m_data;
	ramfile_t* m_file;
	dword m_pos;
};

extern "C" IFolder* RamDisk_Create()
{
	return new CRamFolder;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRamFolder::CRamFolder()
{
}

CRamFolder::~CRamFolder()
{

}

void CRamFolder::ScanDir()
{
	folderitem_ext_t* item;
	dword i;
	wchar_t temp[16];
	ramdisk_t* header;
	ramfile_t* files;

	header = (ramdisk_t*) RAMDISK_ADDR;
	files = (ramfile_t*) (header + 1);
	
	for (i = 0; i < header->num_files; i++)
	{
		item = new folderitem_ext_t;

		memset(item, 0, sizeof(folderitem_ext_t));
		item->size = sizeof(folderitem_t);

		mbstowcs(temp, files[i].name, countof(temp));
		
		item->name = wcsdup(temp);
		item->length = files[i].length;
		item->attributes = ATTR_READ_ONLY;
		item->next = m_item_first;
		item->data = files + i;
		m_item_first = item;
	}
}

IUnknown* CRamFolder::DoOpen(folderitem_ext_t* buf)
{
	IUnknown* file;

	file = CFolder::DoOpen(buf);
	if (file)
		return file;
	else if (buf->data)
	{
		ramfile_t* file = (ramfile_t*) buf->data;
		return new CRamFile((byte*) RAMDISK_ADDR + file->offset, file);
	}
	else
		return NULL;
}

CRamFile::CRamFile(void* data, ramfile_t* file)
{
	m_file = file;
	m_pos = 0;
	m_data = (byte*) data;
}

HRESULT CRamFile::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IStream))
	{
		AddRef();
		*ppvObject = (IStream*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}

size_t CRamFile::Read(void* pBuffer, size_t dwLength)
{
	if (dwLength + m_pos > m_file->length)
		dwLength = m_file->length - m_pos;

	memcpy(pBuffer, m_data + m_pos, dwLength);
	m_pos += dwLength;
	return dwLength;
}

size_t CRamFile::Write(const void* pBuffer, size_t dwLength)
{
	return 0;
}

HRESULT CRamFile::SetIoMode(dword mode)
{
	return S_OK;
}

HRESULT CRamFile::IsReady()
{
	return S_OK;
}

HRESULT CRamFile::Stat(folderitem_t* buf)
{
	if (buf->name_max && buf->name)
		mbstowcs(buf->name, m_file->name, buf->name_max);

	buf->length = m_file->length;
	buf->attributes = ATTR_READ_ONLY;
	return S_OK;
}

HRESULT CRamFile::Seek(long offset, int origin)
{
	switch (origin)
	{
	case seekSet:
		if (offset >= 0 && offset < (long) m_file->length)
			m_pos = offset;
		else
			return E_FAIL;
		break;
	case seekCur:
		if (m_pos + offset >= 0 && m_pos + offset < m_file->length)
			m_pos += offset;
		else
			return E_FAIL;
		break;
	case seekEnd:
		if (offset <= 0 && -offset < (long) m_file->length)
			m_pos = m_file->length + offset;
		else
			return E_FAIL;
		break;
	}

	return S_OK;
}