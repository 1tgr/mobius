// $Id$
#include <gui/Window.h>
#include <stdlib.h>

using namespace OS;

void MessageHandler::HandleMessage(const wnd_message_t *msg)
{
	MessageHandler *This;
	wnd_data_t data;

	This = (MessageHandler*) msg->client_data;
	if (This == 0 && 
		WndGetAttribute(msg->wnd, WND_ATTR_CLIENT_DATA, &data))
		This = (MessageHandler*) data.client_data;

	if (This != 0)
		This->DoHandleMessage(msg);
}

WindowImpl::WindowImpl()
{
	m_title = NULL;
	m_parent = NULL;
	m_message = NULL;
}

WindowImpl::~WindowImpl()
{
	if (m_handle != 0)
	{
		if (m_parent != 0)
			m_parent->RemoveChild(this);

		Close();
	}
}

bool WindowImpl::Create(WindowImpl *parent, const wchar_t *title, const rect_t& position)
{
	m_handle = DoCreateWindow(parent, title, position);

	if (m_handle == 0)
		return false;

	m_parent = parent;
	m_title = _wcsdup(title);
	m_position = position;
	
	if (m_parent != NULL)
		m_parent->AddChild(this);

	if (!OnCreate())
	{
		Close();
		return false;
	}

	return true;
}

void WindowImpl::AddChild(WindowImpl *child)
{
	OnAddChild(child);
}

void WindowImpl::RemoveChild(WindowImpl *child)
{
	OnRemoveChild(child);
}

WindowImpl *WindowImpl::GetParentFrame()
{
	if (m_parent == NULL)
		return this;
	else
		return m_parent->GetParentFrame();
}


void WindowImpl::SetPosition(const rect_t& pos)
{
	wnd_data_t data;
	m_position = pos;
	data.position = pos;
	SetAttribute(WND_ATTR_POSITION, &data);
	OnSize(pos.right - pos.left, pos.bottom - pos.top);
}


wnd_h WindowImpl::DoCreateWindow(WindowImpl *parent, const wchar_t *title, const rect_t& position)
{
	wnd_attr_t attribs[2];

	attribs[0].code = WND_ATTR_POSITION;
	attribs[0].data.position = position;

	attribs[1].code = WND_ATTR_CLIENT_DATA;
	attribs[1].data.client_data = this;

	return WndCreate(parent == 0 ? 0 : parent->m_handle, attribs, _countof(attribs));
}

void WindowImpl::DoHandleMessage(const wnd_message_t *msg)
{
	m_message = msg;
	if (msg->code == MSG_QUIT)
		OnQuit();
	if (msg->code == MSG_PAINT)
		OnPaint();
	if (msg->code == MSG_KEYDOWN)
		OnKeyDown(msg->params.key_down.key);
	if (msg->code == MSG_KEYUP)
		OnKeyUp(msg->params.key_up.key);
	if (msg->code == MSG_MOUSEMOVE)
		OnMouseMove(Point(msg->params.mouse_move.x, msg->params.mouse_move.y));

	if (msg->code == MSG_MOUSEDOWN)
	{
		WindowImpl *frame;
		frame = GetParentFrame();
		if (frame != NULL)
			frame->BringToFront();
		OnMouseDown(Point(msg->params.mouse_move.x, msg->params.mouse_move.y));
	}

	if (msg->code == MSG_MOUSEUP)
		OnMouseUp(Point(msg->params.mouse_move.x, msg->params.mouse_move.y));

	if (msg->code == MSG_CLOSE)
	{
		wnd_h handle;
		handle = m_handle;
		m_handle = 0;
		OnClose(m_handle);
		// might have deleted this inside OnClose
		return;
	}

	if (msg->code == MSG_FOCUS)
		OnFocus();
	if (msg->code == MSG_BLUR)
		OnBlur();

	m_message = 0;
}
