#ifndef __COM_H
#define __COM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

#ifndef callconv_defined
#ifdef _MSC_VER
#define STDCALL	__stdcall
#define CDECL	__cdecl
#else
#define STDCALL	__attribute__((stdcall))
#define CDECL	__attribute__((cdecl))
#endif
#define callconv_defined
#endif

typedef unsigned long ULONG;

#define S_OK			0
#define S_FALSE			1
#define E_FAIL			0x80000000

#define SUCCEEDED(hr)	(((hr) & 0x80000000) == 0)
#define FAILED(hr)		(((hr) & 0x80000000) == 0x80000000)

#ifndef GUID_DEFINED
#define GUID_DEFINED

typedef struct GUID GUID, IID, CLSID;
struct GUID
{
	dword Data1;
	word Data2;
	word Data3;
	byte Data4[8];
};

#endif

#ifdef __cplusplus
typedef const IID& REFIID;
#else
typedef const IID* REFIID;
#endif

#ifdef __cplusplus

#define DECLARE_INTERFACE(name)		struct name
#define STDMETHOD(name)				virtual HRESULT STDCALL name
#define STDMETHOD_(type, name)		virtual type STDCALL name
#define PURE						= 0
#define THIS
#define THIS_
#define EXTERN_C					extern "C"

#define InlineIsEqualGUID(rguid1, rguid2)  \
        (((long*) &rguid1)[0] == ((long*) &rguid2)[0] &&   \
        ((long*) &rguid1)[1] == ((long*) &rguid2)[1] &&    \
        ((long*) &rguid1)[2] == ((long*) &rguid2)[2] &&    \
        ((long*) &rguid1)[3] == ((long*) &rguid2)[3])

/* end C++ declaration */
#else

#define DECLARE_INTERFACE(name)			\
	typedef struct name name; \
	struct name { const struct name##Vtbl* vtbl; }; \
	struct name##Vtbl \

#define STDMETHOD(name)				HRESULT (STDCALL *name)
#define STDMETHOD_(type, name)		type (STDCALL *name)
#define PURE					
#define THIS_						INTERFACE* this, 
#define THIS						INTERFACE* this
#define EXTERN_C					

#define InlineIsEqualGUID(rguid1, rguid2)  \
        (((long*) rguid1)[0] == ((long*) rguid2)[0] &&   \
        ((long*) rguid1)[1] == ((long*) rguid2)[1] &&    \
        ((long*) rguid1)[2] == ((long*) rguid2)[2] &&    \
        ((long*) rguid1)[3] == ((long*) rguid2)[3])

/* end C declarations */
#endif

#ifdef INITGUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
#else
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
		EXTERN_C extern const GUID name
#endif

#define INTERFACE IUnknown

//! Implements standard behaviours for a COM object.
/*!
 *	All COM interfaces inherit from IUnknown: they share the three IUnknown 
 *		methods as the first three entries in their virtual method table.
 *		Therefore every interface can increment/decrement the object's
 *		reference count (AddRef()/Release()), and it is possible to access 
 *		any interface from any other interface (QueryInterface()).
 *	\implement	Whenever you are writing an object which implements a COM
 *		interface.
 *	\use	Whenever you use a COM interface.
 */
DECLARE_INTERFACE(IUnknown)
{
	//! Queries the object for a pointer to the specified interface.
	/*!
	 *	As part of the COM standard, every interface to an object should be 
	 *		able to access every other interface. Therefore the 
	 *		QueryInterface() function is present in every interface supported
	 *		by the object.
	 *
	 *	The new interface should be independent of the old one, and must be 
	 *		Release()'d once it it no longer needed.
	 *
	 *	\param	iid	Specifies the interface to be accessed
	 *	\param	ppvObject	Points to a variable which will receive a pointer
	 *		to the new interface.
	 *	\return	S_OK if the interface was found correctly, or a failure code.
	 */
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;

	//!	Increments the reference count of the object.
	/*!
	 *	Every COM object maintains an internal reference count to allow
	 *		for automatic memory management. This reference count should be
	 *		incremented when a copy of an interface is made, and
	 *		decremented when that interface is no longer needed. When the
	 *		reference count reaches zero the object itself destroys itself.
	 *	\return	Returns the new reference count on the object. This may not be
	 *		accurate and should only be used for debugging purposes.
	 */
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	
	//!	Decrements the reference count of the object.
	/*!
	 *	The interface should be treated as no longer valid once Release() has 
	 *		been called on it, since the object may have been destroyed.
	 *	\return	Returns the new reference count on the object. This may not be
	 *		accurate and should only be used for debugging purposes.
	 */
	STDMETHOD_(ULONG, Release)(THIS) PURE;
};

DEFINE_GUID(IID_IUnknown, 0, 0, 0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

HRESULT IUnknown_QueryInterface(void* ptr, REFIID iid, void** obj);
ULONG IUnknown_AddRef(void* ptr);
ULONG IUnknown_Release(void* ptr);

#ifdef __cplusplus

#if 0

extern "C" int wprintf(const wchar_t*, ...);

#define IMPLEMENT_IUNKNOWN(cls) \
public: \
	dword m_refs; \
\
	STDMETHOD_(ULONG, AddRef)() \
	{ \
		wprintf(L"[" L#cls L"::AddRef %d] ", m_refs); \
		return ++m_refs; \
	} \
\
	STDMETHOD_(ULONG, Release)() \
	{ \
		wprintf(L"[" L#cls L"::Release %u] ", m_refs - 1); \
		if (m_refs == 0) \
		{ \
			wprintf(L"[delete " L#cls L"] "); \
			delete this; \
			return 0; \
		} \
		else \
			return m_refs--; \
	}

#else
#define IMPLEMENT_IUNKNOWN(cls) \
public: \
	dword m_refs; \
\
	STDMETHOD_(ULONG, AddRef)() \
	{ \
		return ++m_refs; \
	} \
\
	STDMETHOD_(ULONG, Release)() \
	{ \
		if (m_refs == 0) \
		{ \
			delete this; \
			return 0; \
		} \
		else \
			return m_refs--; \
	}

#endif

#endif

#ifdef __cplusplus
}
#endif

#endif