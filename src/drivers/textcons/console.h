#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <kernel/driver.h>

class CConsole : public IUnknown, public IDevice
{
protected:
	dword m_refs;
	unsigned short m_con_x, m_con_y, m_width, m_height;
	byte m_esc, m_esc1, m_esc2, m_esc3;
	word m_attrib;

public:
	CConsole();

	virtual void UpdateCursor() = 0;
	virtual void Output(int x, int y, wchar_t c, word attrib) = 0;
	void WriteCharacter(dword mode, wchar_t c);
	void SetAttrib(byte attrib);
	virtual void Clear() = 0;
	virtual void Scroll(int dx, int dy) = 0;

	// IUnknown methods
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();

	// IDevice method
	STDMETHOD(GetInfo)(device_t* buf);
	STDMETHOD(DeviceOpen)();

	class CStream : public IUnknown, public IStream
	{
	protected:
		CConsole *m_con;
		dword m_mode, m_refs;

	public:
		CStream(CConsole* con);
		~CStream();

		// IUnknown methods
		STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
		STDMETHOD_(ULONG, AddRef)();
		STDMETHOD_(ULONG, Release)();

		// IStream methods
		STDMETHOD_(size_t, Read)(void* buffer, size_t length);
		STDMETHOD_(size_t, Write)(const void* buffer, size_t length);
		STDMETHOD(SetIoMode)(dword mode);
		STDMETHOD(IsReady)();
		STDMETHOD(Stat)(folderitem_t* buf);
		STDMETHOD(Seek)(long offset, int origin);
	};

	friend class CStream;
};

#endif