#ifndef __STREAM_H
#define __STREAM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <os/com.h>

enum { ioRaw, ioUnicode, ioAnsi };
enum { seekSet, seekCur, seekEnd };

typedef struct folderitem_t folderitem_t;
struct folderitem_t
{
	dword size;
	union
	{
		dword find_handle;
		IUnknown *item_handle;
	} u;
	const wchar_t* spec;
	wchar_t* name;
	size_t name_max;
	dword attributes;
	size_t length;
};

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_DEVICE         0x1000
#define ATTR_LINK			0x2000

#undef INTERFACE
#define INTERFACE IStream
DECLARE_INTERFACE(IStream)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD_(size_t, Read)(THIS_ void* buffer, size_t length) PURE;
	STDMETHOD_(size_t, Write)(THIS_ const void* buffer, size_t length) PURE;
	STDMETHOD(SetIoMode)(THIS_ dword mode) PURE;
	STDMETHOD(IsReady)(THIS) PURE;
	STDMETHOD(Stat)(THIS_ folderitem_t* buf) PURE;
	STDMETHOD(Seek)(THIS_ long offset, int origin) PURE;
};

// {816B7D54-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IStream, 
0x816b7d54, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

size_t IStream_Read(IStream* ptr, void* buf, size_t len);
size_t IStream_Write(IStream* ptr, const void* buf, size_t len);
HRESULT IStream_SetIoMode(IStream* ptr, dword mode);
HRESULT IStream_IsReady(IStream* ptr);
HRESULT IStream_Stat(IStream* ptr, folderitem_t* buf);
HRESULT IStream_Seek(IStream* ptr, long offset, int origin);

#ifdef __cplusplus
}
#endif

#endif