// MsgQueue.cpp: implementation of the CMsgQueue class.
//
//////////////////////////////////////////////////////////////////////

#include <os/os.h>
#include <gui/messages.h>
#include <stdio.h>
#include <conio.h>
#include "MsgQueue.h"
#include <kernel/proc.h>
#include <kernel/thread.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMsgQueue::CMsgQueue()
{
	_cputws(L"CMsgQueue::CMsgQueue\n");
	m_refs = 0;
	m_head = m_tail = NULL;
}

CMsgQueue::~CMsgQueue()
{
	_cputws(L"CMsgQueue::~CMsgQueue\n");
}

HRESULT CMsgQueue::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IMsgQueue))
	{
		AddRef();
		*ppvObject = (IMsgQueue*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CMsgQueue::PeekMessage(msg_t* msg, bool remove)
{
	mqent_t* next;

	if (m_head)
	{
		*msg = m_head->msg;

		if (remove)
		{
			next = m_head->next;
			free(m_head);
			m_head = next;

			if (m_head == NULL)
				m_tail = NULL;
		}

		return msg->message == WM_QUIT ? S_FALSE : S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CMsgQueue::GetMessage(msg_t* msg)
{
	HRESULT hr;

	//_cputws(L"GetMessage...");
	while ((hr = PeekMessage(msg, true)) == E_FAIL)
		;

	//_cputws(L"done\n");
	return hr;
}

HRESULT CMsgQueue::DispatchMessage(const msg_t* msg)
{
	void *wndproc;
	dword params[3];

	if (msg->wnd)
	{
		if (msg->wnd->GetAttrib(ATTR_PROCESS) == (dword) procCurrent())
		{
			wndproc = (void*) msg->wnd->GetAttrib(ATTR_WNDPROC);
			if (wndproc)
			{
				//wprintf(L"wndproc = %p\n", wndproc);
				params[0] = (dword) msg->wnd;
				params[1] = msg->message;
				params[2] = msg->params;
				thrCall(thrCurrent(), wndproc, params, sizeof(params));
			}
			else
				msg->wnd->DefWndProc(msg->message, msg->params);

			return S_OK;
		}
	}
	
	return E_FAIL;
}

HRESULT CMsgQueue::PostMessage(IWindow* wnd, dword message, dword params)
{
	mqent_t* ent = (mqent_t*) malloc(sizeof(mqent_t));

	ent->msg.wnd = wnd;
	ent->msg.message = message;
	ent->msg.params = params;
	ent->next = NULL;

	if (m_tail)
		m_tail->next = ent;
	m_tail = ent;
	if (m_head == NULL)
		m_head = ent;

	return S_OK;
}