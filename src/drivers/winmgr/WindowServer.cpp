// WindowServer.cpp: implementation of the CWindowServer class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <os/os.h>

#include "WindowServer.h"
#include "Window.h"
#include "winframe.h"
#include "MsgQueue.h"
#include "ResFont.h"
#include "DesktopWnd.h"
#include "S3Graphics.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern "C" IWindowServer* OpenServer();

extern "C" const byte font1[], font2[];
extern ISurface* CreateGraphics(int nMode);
extern IFont* CreateFont(const byte* pFontData);

CWindow* desktop;

void __cdecl idle()
{
#if 0
	msg_t msg;
	IMsgQueue* queue;
	HRESULT (*wndproc) (IWindow*, dword, dword);

	queue = (IMsgQueue*) thrGetTls();
	if (queue)
	{
		while (true)
		{
			if (SUCCEEDED(queue->GetMessage(&msg)))
			{
				/* Can't currently use IMsgQueue::DispatchMessage in kernel mode */
				if (msg.wnd)
				{
					wndproc = (HRESULT (*) (IWindow*, dword, dword)) 
						msg.wnd->GetAttrib(ATTR_WNDPROC);

					if (wndproc)
						wndproc(msg.wnd, msg.message, msg.params);
					else
						msg.wnd->DefWndProc(msg.message, msg.params);
				}
			}
		}
	}
#endif
}

CWindowServer::CWindowServer()
{
	const word *font_dir, *ord;
	const FontDirEntry *header;
	windowdef_t def;
	int i;
	surface_t desc;
	
	m_refs = 0;
	m_gfx = CreateGraphics(0x13);
	//m_gfx = new CS3Graphics;
	m_gfx->GetSurfaceDesc(&desc);
	
	memset(m_fonts, 0, sizeof(m_fonts));
	m_fonts[0] = CreateFont(font2);

	font_dir = (const word*) resFind(_info.base, RT_FONTDIR, 0, 0);
	if (font_dir)
	{
		ord = font_dir + 1;
		for (i = 0; i < *font_dir; i++)
		{
			header = (const FontDirEntry*) (ord + 1);
			m_fonts[i + 1] = new CResFont(_info.base, *ord);

			if (header->Version == 0x200)
				ord = (const word*) ((const byte*) header + SIZEOF_FONTDIRENTRY20);
			else
				ord = (const word*) ((const byte*) header + sizeof(FontDirEntry));
		}
	}

	DeviceOpen();

	def.size = sizeof(def);
	def.flags = WIN_X | WIN_Y | WIN_WIDTH | WIN_HEIGHT | WIN_PARENT;
	def.x = def.y = 0;
	def.width = desc.nWidth;
	def.height = desc.nHeight;
	def.parent = NULL;
	AddRef();
	desktop = new CDesktopWnd(&def, this);

	desktop->UpdateWindow();
}

CWindowServer::~CWindowServer()
{
	int i;

	desktop->Release();
	desktop = NULL;

	for (i = 0; i < countof(m_fonts); i++)
		if (m_fonts[i])
			m_fonts[i]->Release();

	m_gfx->Release();
}

HRESULT CWindowServer::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IDevice))
	{
		AddRef();
		*ppvObject = (IDevice*) this;
		return S_OK;
	}
	else if (InlineIsEqualGUID(iid, IID_IWindowServer))
	{
		AddRef();
		*ppvObject = (IWindowServer*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CWindowServer::GetInfo(device_t* buf)
{
	if (buf->size < sizeof(device_t))
		return E_FAIL;

	wcscpy(buf->name, L"Window Server");
	return S_OK;
}

HRESULT CWindowServer::DeviceOpen()
{
	if (thrGetInfo()->queue == NULL)
	{
		CMsgQueue* queue = new CMsgQueue;
		//thrSetTls((dword) (IMsgQueue*) queue);
		thrGetInfo()->queue = (dword) (IMsgQueue*) queue;
	}

	return S_OK;
}

IWindow* CWindowServer::CreateWindow(const windowdef_t* def)
{
	CWindowFrame* frame;
	CWindow* wnd;
	windowdef_t deftemp = *def, def2 = *def;
	rectangle_t rect;
	
	AddRef();
	deftemp.flags &= ~WIN_WNDPROC;
	deftemp.wndproc = NULL;
	frame = new CWindowFrame(&deftemp, this);

	rect = *frame;
	frame->AdjustForFrame(&rect);
	def2.flags |= WIN_PARENT | WIN_X | WIN_Y | WIN_WIDTH | WIN_HEIGHT;
	def2.parent = frame;
	def2.x = rect.left;
	def2.y = rect.top;
	def2.width = rect.Width();
	def2.height = rect.Height();

	wnd = new CWindow(&def2, this);
	frame->UpdateWindow();
	return wnd;
}

ISurface* CWindowServer::GetScreen()
{
	m_gfx->AddRef();
	return m_gfx;
}

IFont* CWindowServer::GetFont(int index)
{
	if (m_fonts[index])
	{
		m_fonts[index]->AddRef();
		return m_fonts[index];
	}
	else
		return NULL;
}