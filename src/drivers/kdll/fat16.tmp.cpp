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

int TRACE(...)
{
	return 0;
}

#endif

extern "C" bool wildcard(const wchar_t* str, const wchar_t* spec);

struct folderitem_fat_t : folderitem_t
{
	folderitem_fat_t* next;
	IUnknown* mount;
};

class FatFile : public IUnknown, public IStream
{
protected:
	fat_filefind_t m_stat;
	addr_t m_dwPosition;
	byte* m_pCache;
	dword m_dwCacheSize, m_dwCachePtr;
	IBlockDevice* m_pDevice;
	dword m_dwReadCluster;
	fat_bootsector_t m_bpb;
	bool m_bIsRoot;

	dword GetNextCluster(dword dwCluster);
	bool ReadCluster(dword dwCluster, void* pBuf, size_t size);

	friend class FatFolder;

public:
	FatFile(IBlockDevice* pDev, const fat_filefind_t* stat);
	virtual ~FatFile();

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN;
	
	STDMETHOD_(size_t, Read) (void* pBuffer, size_t dwLength);
	STDMETHOD_(size_t, Write) (const void* pBuffer, size_t dwLength);
	STDMETHOD(SetIoMode)(dword mode);
	STDMETHOD(IsReady)();
	STDMETHOD(Stat)(folderitem_t* buf);
};

class FatFolder : public IUnknown, public IFolder
{
public:
	FatFolder(IBlockDevice* pDev, const fat_filefind_t* stat, bool bIsRoot);
	virtual ~FatFolder();
	
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN;
	
	STDMETHOD(FindFirst)(const wchar_t* spec, folderitem_t* buf);
	STDMETHOD(FindNext)(folderitem_t* buf);
	STDMETHOD(FindClose)(folderitem_t* pBuf);
	STDMETHOD(Open)(folderitem_t* item, const wchar_t* params);
	STDMETHOD(Mount)(const wchar_t* name, IUnknown* obj);

protected:
	FatFile m_file;
	folderitem_fat_t* m_item_first;

	void ScanDir();
};

extern "C" IFolder* FatRoot_Create(IBlockDevice* pDevice)
{
	fat_filefind_t buf =
	{
		{
			sizeof(buf),
			NULL,
			NULL,
			NULL,
			0,
			0,
			0
		},
		{
			"",
			"",
			0,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			0, 0, 0, 0
		}
	};

	return new FatFolder(pDevice, &buf, true);
}

/****************************************************************************
 * FatFile                                                                 *
 ****************************************************************************/
FatFile::FatFile(IBlockDevice* pDev, const fat_filefind_t* stat)
{
	m_pDevice = pDev;
	IUnknown_AddRef(pDev);

	memset(&m_bpb, 0, sizeof(m_bpb));
	//pDev->Seek(0, SEEK_SET);
	//pDev->ReadFile(&m_bpb, sizeof(m_bpb));
	IBlockDevice_BlockRead(pDev, 0, 1, &m_bpb);

	m_stat = *stat;
	m_dwReadCluster = (dword) -1;
	m_bIsRoot = false;
	m_dwCacheSize = m_bpb.nBytesPerSector * m_bpb.nSectorsPerCluster * 
		CLUSTER_CACHE;
	m_dwCachePtr = (dword) -1;
	m_pCache = (byte*) malloc(m_dwCacheSize);
}

FatFile::~FatFile()
{
	TRACE(L"[fatfile] deleting\n");
	if (m_pCache)
		free(m_pCache);
	if (m_pDevice)
		IUnknown_Release(m_pDevice);
}

HRESULT FatFile::QueryInterface(REFIID iid, void ** ppvObject)
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

size_t FatFile::Read(void* pBuffer, size_t dwLength)
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

	while (dwRead < dwLength)
	{
		if (m_dwCachePtr >= m_dwCacheSize)
		{
			TRACE(L"[clus = %x] ", m_dwReadCluster);

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

		TRACE(L"{cache = %d} ", m_dwCacheSize - m_dwCachePtr);
		i = min(m_dwCacheSize - m_dwCachePtr, dwLength - dwRead);
		memcpy((byte*) pBuffer + dwRead, m_pCache + m_dwCachePtr, i);
		m_dwCachePtr += i;
		dwRead += i;
	}

	m_dwPosition += dwLength;
	return dwRead;
}

size_t FatFile::Write(const void* pBuffer, size_t dwLength)
{
	return 0;
}

HRESULT FatFile::SetIoMode(dword mode)
{
	return S_OK;
}

HRESULT FatFile::IsReady()
{
	return S_OK;
}

HRESULT FatFile::Stat(folderitem_t* buf)
{
	if (buf->size == sizeof(fat_filefind_t))
		memcpy(buf, &m_stat, sizeof(fat_filefind_t));
	else
		*buf = m_stat.ff;

	return S_OK;
}

dword FatFile::GetNextCluster(dword dwCluster)
{
	dword FATOffset, ThisFATSecNum, ThisFATEntOffset;
	byte sector[512];

	FATOffset = dwCluster * FAT_BYTES;
	ThisFATSecNum = m_bpb.nReservedSectors + 
		(FATOffset / m_bpb.nBytesPerSector);
	ThisFATEntOffset = FATOffset % m_bpb.nBytesPerSector;

	//m_pDevice->Seek(ThisFATSecNum, SEEK_SET);
	//m_pDevice->ReadFile(sector, sizeof(sector));
	IBlockDevice_BlockRead(m_pDevice, ThisFATSecNum, 1, sector);
	return *(word*) (sector + ThisFATEntOffset);
}

bool FatFile::ReadCluster(dword dwCluster, void* pBuf, size_t size)
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
	return IBlockDevice_BlockRead(m_pDevice, FirstSectorofCluster, size, pBuf) == size;
}

/****************************************************************************
 * FatFolder                                                               *
 ****************************************************************************/
FatFolder::FatFolder(IBlockDevice* pDev, const fat_filefind_t* stat, bool bIsRoot) :
	m_file(pDev, stat)
{
	m_file.m_bIsRoot = bIsRoot;
	m_item_first = NULL;
}

FatFolder::~FatFolder()
{
	folderitem_fat_t *item, *next;
	
	for (item = m_item_first; item; item = next)
	{
		next = item->next;
		if (item->mount)
			IUnknown_Release(item->mount);
		item->mount = NULL;
		delete item;
	}
}

HRESULT FatFolder::QueryInterface(REFIID iid, void ** ppvObject)
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

HRESULT FatFolder::FindFirst(const wchar_t* szSpec, folderitem_t* item)
{
	item->spec = wcsdup(szSpec);
	//TRACE(L"FindFirst %s\n", item->spec);
	return S_OK;
}

HRESULT FatFolder::FindNext(folderitem_t* item)
{
	union
	{
		fat_dirent_t dirent;
		fat_lfnslot_t lfn;
	};
	int i, j;
	wchar_t name[MAX_PATH], temp[14], *nameptr;

	while (1)
	{
		if (m_file.Read(&dirent, sizeof(dirent)) < sizeof(dirent))
		{
			TRACE(L"End of file");
			return E_FAIL;
		}

		if (dirent.szFilename[0] == 0)
		{
			TRACE(L"End of directory\n");
			return E_FAIL;
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

				TRACE(L"short: name = \"%s\" (\"%s\")\n", name, item->spec);
			}
			else
			{
				while ((dirent.nAttributes & ATTR_LONG_NAME)
					== ATTR_LONG_NAME)
				{
					TRACE(L"<lfn seq=%2x> ", dirent.szFilename[0]);
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
						TRACE(L"End of file");
						return E_FAIL;
					}
				}

				//TRACE(L"long: name = \"%s\" (\"%s\")\n", name, item->spec);
			}

			if (wildcard(name, item->spec))
			{
				//TRACE(L"Found item size = %d\n", dirent.dwFileSize);
				wcscpy(item->name, name);
				item->length = dirent.dwFileSize;
				item->attributes = dirent.nAttributes;

				if (item->size == sizeof(fat_filefind_t))
					memcpy((fat_filefind_t*) (item + 1), &dirent, sizeof(dirent));

				return S_OK;
			}
		}
	}

	return E_FAIL;
}

HRESULT FatFolder::FindClose(folderitem_t* pBuf)
{
	//TRACE(L"FindClose\n");
	free((void*) pBuf->spec);
	m_file.m_dwPosition = 0;
	m_file.m_dwCachePtr = (dword) -1;
	m_file.m_dwReadCluster = (dword) -1;
	return S_OK;
}

HRESULT FatFolder::Open(folderitem_t* item, const wchar_t* params)
{
	fat_filefind_t buf;
	IUnknown* pFile;
	wchar_t temp[MAX_PATH];
	
	TRACE(L"[OpenChild] %s\n", item->name);
	buf.ff.size = sizeof(fat_filefind_t);
	buf.ff.name = temp;
	buf.ff.name_max = countof(temp);
	if (FAILED(FindFirst(item->name, &buf.ff)) ||
		FAILED(FindNext(&buf.ff)))
		return E_FAIL;

	if (buf.ff.attributes & ATTR_DIRECTORY)
	{
		TRACE(L"Found dir at %x\n", buf.dirent.nFirstCluster);
		pFile = new FatFolder(m_file.m_pDevice, &buf, false);
	}
	else
	{
		TRACE(L"Found file at %x\n", buf.dirent.nFirstCluster);
		pFile = new FatFile(m_file.m_pDevice, &buf);
	}

	//FindClose((FileFind*) &buf);

	if (item->size == sizeof(fat_filefind_t))
		memcpy(item, &buf, sizeof(fat_filefind_t));
	else
		*item = buf.ff;

	item->u.item_handle = pFile;
	FindClose(&buf.ff);
	//return pFile;
	return S_OK;
}

HRESULT FatFolder::Mount(const wchar_t* name, IUnknown* obj)
{
	folderitem_fat_t* item = new folderitem_fat_t;
	
	item->size = sizeof(folderitem_fat_t);
	memset(item, 0, sizeof(item));
	item->name = wcsdup(name);
	item->attributes = ATTR_DIRECTORY | ATTR_LINK;
	item->length = 0;

	if (m_item_first)
		m_item_first->next = item;
	else
		m_item_first = item;

	return S_OK;
}

void FatFolder::ScanDir()
{
}