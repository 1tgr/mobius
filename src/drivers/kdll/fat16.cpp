#include <kernel/driver.h>
#include <os/stream.h>
#include "fat.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define FAT_BYTES       2   // FAT16
// #define FAT_BYTES    4   // FAT32 (nyi)
#define CLUSTER_CACHE   1

#if 0
#define TRACE   wprintf
#else

static int TRACE(...)
{
	return 0;
}

#endif

extern "C" bool wildcard(const wchar_t* str, const wchar_t* spec);

struct folderitem_fat_t : folderitem_t
{
	folderitem_fat_t* next;
	IUnknown* mount;
	fat_dirent_t dirent;
};

class CFatFile : public IUnknown, public IStream
{
protected:
	folderitem_fat_t m_stat;
	addr_t m_dwPosition;
	byte* m_pCache;
	dword m_dwCacheSize, m_dwCachePtr;
	IBlockDevice* m_pDevice;
	dword m_dwReadCluster;
	fat_bootsector_t m_bpb;
	bool m_bIsRoot;

	dword GetNextCluster(dword dwCluster);
	bool ReadCluster(dword dwCluster, void* pBuf, size_t size);

	friend class CFatFolder;

public:
	CFatFile(IBlockDevice* pDev, const folderitem_fat_t* stat);
	virtual ~CFatFile();

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CFatFile);
	
	STDMETHOD_(size_t, Read) (void* pBuffer, size_t dwLength);
	STDMETHOD_(size_t, Write) (const void* pBuffer, size_t dwLength);
	STDMETHOD(SetIoMode)(dword mode);
	STDMETHOD(IsReady)();
	STDMETHOD(Stat)(folderitem_t* buf);
	STDMETHOD(Seek)(long offset, int origin);
};

class CFatFolder : public IUnknown, public IFolder
{
public:
	CFatFolder(IBlockDevice* pDev, const folderitem_fat_t* stat, bool bIsRoot);
	virtual ~CFatFolder();
	
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CFatFolder);
	
	STDMETHOD(FindFirst)(const wchar_t* spec, folderitem_t* buf);
	STDMETHOD(FindNext)(folderitem_t* buf);
	STDMETHOD(FindClose)(folderitem_t* pBuf);
	STDMETHOD(Open)(folderitem_t* item, const wchar_t* params);
	STDMETHOD(Mount)(const wchar_t* name, IUnknown* obj);

protected:
	CFatFile m_file;
	folderitem_fat_t* m_item_first;

	void ScanDir();
};

extern "C" IFolder* FatRoot_Create(IBlockDevice* pDevice)
{
	folderitem_fat_t buf;
	memset(&buf, 0, sizeof(buf));
	buf.size = sizeof(buf);
	return new CFatFolder(pDevice, &buf, true);
}

/****************************************************************************
 * CFatFile                                                                 *
 ****************************************************************************/
CFatFile::CFatFile(IBlockDevice* pDev, const folderitem_fat_t* stat)
{
	m_pDevice = pDev;
	//IUnknown_AddRef(pDev);
	pDev->AddRef();

	memset(&m_bpb, 0, sizeof(m_bpb));
	//pDev->Seek(0, SEEK_SET);
	//pDev->ReadFile(&m_bpb, sizeof(m_bpb));
	//IBlockDevice_BlockRead(pDev, 0, 1, &m_bpb);
	pDev->BlockRead(0, 1, &m_bpb);

	m_stat = *stat;
	m_dwReadCluster = (dword) -1;
	m_bIsRoot = false;
	m_dwCacheSize = m_bpb.nBytesPerSector * m_bpb.nSectorsPerCluster * 
		CLUSTER_CACHE;
	m_dwCachePtr = (dword) -1;
	m_pCache = (byte*) malloc(m_dwCacheSize);
	m_refs = 0;
}

CFatFile::~CFatFile()
{
	TRACE(L"[CFatFile] deleting\n");
	if (m_pCache)
		free(m_pCache);
	if (m_pDevice)
		m_pDevice->Release();
		//IUnknown_Release(m_pDevice);
}

HRESULT CFatFile::QueryInterface(REFIID iid, void ** ppvObject)
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

size_t CFatFile::Read(void* pBuffer, size_t dwLength)
{
	size_t dwRead = 0;
	int i;

	if (m_dwReadCluster == (dword) -1)
	{
		if (m_dwPosition == 0)
			m_dwReadCluster = m_stat.dirent.nFirstCluster;
		else
			return 0;
	}

	if (m_dwPosition + dwLength > m_stat.length)
		dwLength = m_stat.length - m_dwPosition;

	while (dwRead < dwLength)
	{
		if (m_dwCachePtr >= m_dwCacheSize)
		{
			//TRACE(L"[clus = %x] ", m_dwReadCluster);

			if (m_dwReadCluster >= 0xFFF8)
			{
				m_dwPosition += dwRead;
				m_dwCachePtr = (dword) -1;
				return dwRead;
			}

			if (!ReadCluster(m_dwReadCluster, m_pCache, m_dwCacheSize))
			{
				TRACE(L"ReadCluster failed\n");
				m_dwPosition += dwRead;
				m_dwCachePtr = (dword) -1;
				return dwRead;
			}

			m_dwCachePtr = 0;
			m_dwReadCluster = GetNextCluster(m_dwReadCluster);
		}

		//TRACE(L"{cache = %d} ", m_dwCacheSize - m_dwCachePtr);
		i = min(m_dwCacheSize - m_dwCachePtr, dwLength - dwRead);
		memcpy((byte*) pBuffer + dwRead, m_pCache + m_dwCachePtr, i);
		m_dwCachePtr += i;
		dwRead += i;
	}

	m_dwPosition += dwLength;
	return dwRead;
}

size_t CFatFile::Write(const void* pBuffer, size_t dwLength)
{
	return 0;
}

HRESULT CFatFile::SetIoMode(dword mode)
{
	return S_OK;
}

HRESULT CFatFile::IsReady()
{
	return S_OK;
}

HRESULT CFatFile::Stat(folderitem_t* buf)
{
	if (buf->size == sizeof(folderitem_fat_t))
		memcpy(buf, &m_stat, sizeof(folderitem_fat_t));
	else
		*buf = m_stat;

	return S_OK;
}

dword CFatFile::GetNextCluster(dword dwCluster)
{
	dword FATOffset, ThisFATSecNum, ThisFATEntOffset;
	byte sector[512];

	FATOffset = dwCluster * FAT_BYTES;
	ThisFATSecNum = m_bpb.nReservedSectors + 
		(FATOffset / m_bpb.nBytesPerSector);
	ThisFATEntOffset = FATOffset % m_bpb.nBytesPerSector;

	//m_pDevice->Seek(ThisFATSecNum, SEEK_SET);
	//m_pDevice->ReadFile(sector, sizeof(sector));
	//IBlockDevice_BlockRead(m_pDevice, ThisFATSecNum, 1, sector);
	m_pDevice->BlockRead(ThisFATSecNum, 1, sector);
	return *(word*) (sector + ThisFATEntOffset);
}

HRESULT CFatFile::Seek(long offset, int origin)
{
	return E_FAIL;
}

bool CFatFile::ReadCluster(dword dwCluster, void* pBuf, size_t size)
{
	dword RootDirSectors, FirstDataSector, FirstSectorofCluster;

	if (m_bIsRoot)
		RootDirSectors = 0;
	else
		RootDirSectors = 
			((m_bpb.nRootDirectoryEntries * 32) + (m_bpb.nBytesPerSector - 1)) 
				/ m_bpb.nBytesPerSector;

	FirstDataSector = m_bpb.nReservedSectors + 
		(m_bpb.nFatCount * m_bpb.nSectorsPerFat) + RootDirSectors;

	if (m_bIsRoot)
		dwCluster += 2;

	FirstSectorofCluster = ((dwCluster - 2) * m_bpb.nSectorsPerCluster) +
		FirstDataSector;

	//m_pDevice->Seek(FirstSectorofCluster, SEEK_SET);
	//return m_pDevice->ReadFile(pBuf, size) == (int) size;
	size /= 512;
	//return IBlockDevice_BlockRead(m_pDevice, FirstSectorofCluster, size, pBuf) == size;
	return m_pDevice->BlockRead(FirstSectorofCluster, size, pBuf) == size;
}

/****************************************************************************
 * CFatFolder                                                               *
 ****************************************************************************/
CFatFolder::CFatFolder(IBlockDevice* pDev, const folderitem_fat_t* stat, bool bIsRoot) :
	m_file(pDev, stat)
{
	m_refs = 0;
	m_file.m_bIsRoot = bIsRoot;
	m_item_first = NULL;

	if (bIsRoot)
		m_file.m_stat.length = sizeof(fat_dirent_t) * m_file.m_bpb.nRootDirectoryEntries;
}

CFatFolder::~CFatFolder()
{
	folderitem_fat_t *item, *next;
	
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

HRESULT CFatFolder::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IFolder))
	{
		AddRef();
		*ppvObject = (IFolder*) this;
		return S_OK;
	}
	else if (InlineIsEqualGUID(iid, IID_IStream))
	{
		AddRef();
		*ppvObject = (IStream*) &m_file;
		return S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CFatFolder::FindFirst(const wchar_t* szSpec, folderitem_t* item)
{
	if (m_item_first == NULL)
		ScanDir();

	item->spec = wcsdup(szSpec);
	item->u.find_handle = (dword) m_item_first;
	TRACE(L"FindFirst %s\n", item->spec);
	return S_OK;
}

HRESULT CFatFolder::FindNext(folderitem_t* item)
{
	folderitem_fat_t *fitem = (folderitem_fat_t*) item->u.find_handle;
	
	if (fitem == NULL)
		return E_FAIL;
	else
	{
		while (fitem)
		{
			if (wildcard(fitem->name, item->spec))
			{
				TRACE(L"[fat] item = %s spec = %s\n", fitem->name, item->spec);
				item->u.find_handle = (dword) fitem->next;

				wcsncpy(item->name, fitem->name, item->name_max);
				item->attributes = fitem->attributes;
				item->length = fitem->length;

				if (item->size == sizeof(folderitem_fat_t))
				{
					((folderitem_fat_t*) item)->mount = fitem->mount;
					((folderitem_fat_t*) item)->next = fitem->next;
					((folderitem_fat_t*) item)->dirent = fitem->dirent;
				}

				return S_OK;
			}

			fitem = fitem->next;
			item->u.find_handle = (dword) fitem;
		}

		return E_FAIL;
	}
}

HRESULT CFatFolder::FindClose(folderitem_t* pBuf)
{
	//TRACE(L"FindClose\n");
	free((void*) pBuf->spec);
	return S_OK;
}

HRESULT CFatFolder::Open(folderitem_t* item, const wchar_t* params)
{
	folderitem_fat_t buf;
	IUnknown* pFile;
	wchar_t temp[MAX_PATH];
	
	TRACE(L"[OpenChild] %s\n", item->name);
	buf.size = sizeof(folderitem_fat_t);
	buf.name = temp;
	buf.name_max = countof(temp);
	if (FAILED(FindFirst(item->name, &buf)) ||
		FAILED(FindNext(&buf)))
		return E_FAIL;

	if (buf.mount)
	{
		TRACE(L"Found mount point %p\n", buf.mount);
		pFile = buf.mount;
		//IUnknown_AddRef(pFile);
		pFile->AddRef();
	}
	else if (buf.attributes & ATTR_DIRECTORY)
	{
		TRACE(L"Found dir at %x\n", buf.dirent.nFirstCluster);
		pFile = new CFatFolder(m_file.m_pDevice, &buf, false);
	}
	else
	{
		TRACE(L"Found file at %x\n", buf.dirent.nFirstCluster);
		pFile = new CFatFile(m_file.m_pDevice, &buf);
	}

	//FindClose((FileFind*) &buf);

	if (item->size == sizeof(folderitem_fat_t))
		memcpy(item, &buf, sizeof(folderitem_fat_t));
	else
		*item = buf;

	item->u.item_handle = pFile;
	FindClose(&buf);
	//return pFile;
	return S_OK;
}

HRESULT CFatFolder::Mount(const wchar_t* name, IUnknown* obj)
{
	folderitem_fat_t* item = new folderitem_fat_t;
	
	item->size = sizeof(folderitem_fat_t);
	memset(item, 0, sizeof(item));
	item->name = wcsdup(name);
	item->attributes = ATTR_DIRECTORY | ATTR_LINK;
	item->length = 0;
	item->mount = obj;
	obj->AddRef();
	item->next = m_item_first;
	m_item_first = item;

	return S_OK;
}

void CFatFolder::ScanDir()
{
	union
	{
		fat_dirent_t dirent;
		fat_lfnslot_t lfn;
	};
	int i, j, count;
	wchar_t name[MAX_PATH], temp[14], *nameptr;
	
	m_file.m_dwPosition = 0;
	m_file.m_dwCachePtr = (dword) -1;
	m_file.m_dwReadCluster = (dword) -1;

	count = 0;
	while (1)
	{
		if (m_file.Read(&dirent, sizeof(dirent)) < sizeof(dirent))
		{
			TRACE(L"End of file: %d\n", count);
			return;
		}

		if (dirent.szFilename[0] == 0)
		{
			TRACE(L"End of directory: %d\n", count);
			return;
		}

		if (dirent.szFilename[0] != 0xe5)
		{
			memset(name, 0, sizeof(name));

			if ((dirent.nAttributes & ATTR_LONG_NAME) != ATTR_LONG_NAME)
			{
				for (i = 0; i < 8; i++)
				{
					if (dirent.szFilename[i] == ' ')
					{
						name[i] = 0;
						break;
					}
					else if (iswupper(dirent.szFilename[i]))
						name[i] = towlower(dirent.szFilename[i]);
					else
						name[i] = dirent.szFilename[i];
				}

				if (dirent.szExtension[0] != ' ')
				{
					wcscat(name, L".");

					j = wcslen(name);
					for (i = 0; i < 3; i++)
					{
						if (dirent.szExtension[i] == ' ')
						{
							name[i + j] = 0;
							break;
						}
						else if (iswupper(dirent.szExtension[i]))
							name[i + j] = towlower(dirent.szExtension[i]);
						else
							name[i + j] = dirent.szExtension[i];
					}
				}

				//TRACE(L"short: name = \"%s\" (\"%s\")\n", name, item->spec);
			}
			else
			{
				while ((dirent.nAttributes & ATTR_LONG_NAME)
					== ATTR_LONG_NAME)
				{
					//TRACE(L"<lfn seq=%2x> ", dirent.szFilename[0]);
					if (dirent.szFilename[0] != 0xe5)
					{
						memset(temp, 0, sizeof(temp));
						nameptr = temp;

						for (i = 0; i < 5; i++)
						{
							*nameptr = lfn.name0_4[i];
							nameptr++;
						}

						for (i = 0; i < 6; i++)
						{
							*nameptr = lfn.name5_10[i];
							nameptr++;
						}

						for (i = 0; i < 2; i++)
						{
							*nameptr = lfn.name11_12[i];
							nameptr++;
						}

						i = wcslen(temp);
						memmove(name + i, name, wcslen(name) * sizeof(wchar_t));
						memcpy(name, temp, i * sizeof(wchar_t));
					}

					if (m_file.Read(&dirent, sizeof(dirent)) < sizeof(dirent))
					{
						TRACE(L"End of file (lfn): %d\n", count);
						return;
					}
				}

				//TRACE(L"long: name = \"%s\" (\"%s\")\n", name, item->spec);
			}

			//if (wildcard(name, item->spec))
			{
				folderitem_fat_t* item;
				//TRACE(L"Found item %s size = %d\n", name, dirent.dwFileSize);

				item = new folderitem_fat_t;
				memset(item, 0, sizeof(item));
				item->size = sizeof(folderitem_fat_t);
				item->name = wcsdup(name);
				item->length = dirent.dwFileSize;
				item->attributes = dirent.nAttributes;
				item->dirent = dirent;
				item->next = m_item_first;
				m_item_first = item;

				count++;
			}
		}
	}
}