// MsgQueue.h: interface for the CMsgQueue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MSGQUEUE_H__76B07981_7DE1_4513_8FA9_B4269A40986A__INCLUDED_)
#define AFX_MSGQUEUE_H__76B07981_7DE1_4513_8FA9_B4269A40986A__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <gui/winserve.h>

struct mqent_t
{
	mqent_t *next, *prev;
	msg_t msg;
};

class CMsgQueue : 
	public IUnknown, 
	public IMsgQueue  
{
public:
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CMsgQueue);

	STDMETHOD(PeekMessage)(msg_t* msg, bool remove);
	STDMETHOD(GetMessage)(msg_t* msg);
	STDMETHOD(DispatchMessage)(const msg_t* msg);
	STDMETHOD(PostMessage)(IWindow* wnd, dword message, dword params);

public:
	CMsgQueue();
	virtual ~CMsgQueue();

	mqent_t *m_head, *m_tail;
};

#endif // !defined(AFX_MSGQUEUE_H__76B07981_7DE1_4513_8FA9_B4269A40986A__INCLUDED_)
