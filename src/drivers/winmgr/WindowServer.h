// WindowServer.h: interface for the CWindowServer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINDOWSERVER_H__1726670A_0953_4E42_BB4B_C09886089C16__INCLUDED_)
#define AFX_WINDOWSERVER_H__1726670A_0953_4E42_BB4B_C09886089C16__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <kernel/driver.h>
#include <gui/winserve.h>
#include <gui/surface.h>
#include <gui/font.h>

class CWindowServer : 
	public IUnknown, 
	public IWindowServer,
	public IDevice
{
public:
	CWindowServer();
	virtual ~CWindowServer();

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CWindowServer);

	STDMETHOD(GetInfo)(device_t* buf);
	STDMETHOD(DeviceOpen)();

	STDMETHOD_(IWindow*, CreateWindow)(const windowdef_t* def);
	STDMETHOD_(ISurface*, GetScreen)();
	STDMETHOD_(IFont*, GetFont)(int index);

protected:
	ISurface* m_gfx;
	IFont *m_fonts[16];
};

#endif // !defined(AFX_WINDOWSERVER_H__1726670A_0953_4E42_BB4B_C09886089C16__INCLUDED_)
