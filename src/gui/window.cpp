/* $Id: window.cpp,v 1.2 2002/04/10 12:27:44 pavlovskii Exp $ */

#define __THROW_BAD_ALLOC printf("out of memory\n"); exit(1)
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>

#include <os/gui.h>
#include <os/syscall.h>
#include <gui/window.h>
#include <gui/application.h>
#include <algorithm>

using namespace os;

int dwprintf(const wchar_t *fmt, ...);

Window::Window()
{
    m_handle = NULL;
    m_parent = NULL;
}

Window::Window(Window *parent, const wchar_t *title, const MGLrect &pos)
{
    Create(parent, title, pos);
}

Window::Window(handle_t hnd)
{
    m_handle = hnd;
    m_parent = NULL;
}

Window::~Window()
{
    if (m_parent != NULL)
        m_parent->RemoveChild(this);

    WndClose(m_handle);
}

void Window::Create(Window *parent, const wchar_t *title, const MGLrect &pos)
{
    wndattr_t attribs[3];
    
    attribs[0].id = WND_ATTR_POSITION;
    attribs[0].type = WND_TYPE_RECT;
    attribs[0].size = sizeof(MGLrect);
    attribs[0].data.rect = (MGLrect*) &pos;
    attribs[1].id = WND_ATTR_USERDATA;
    attribs[1].type = WND_TYPE_VOID;
    attribs[1].size = sizeof(this);
    attribs[1].data.ptr = this;

    if (title != NULL)
    {
        attribs[2].id = WND_ATTR_TITLE;
        attribs[2].type = WND_TYPE_STRING;
        attribs[2].size = (wcslen(title) + 1) * sizeof(wchar_t);
        attribs[2].data.str = (wchar_t*) title;
    }

    m_handle = WndCreate(parent->GetHandle(), attribs, 
        _countof(attribs) - (title == NULL ? 1 : 0));

    m_parent = parent;
    if (parent != NULL)
        parent->AddChild(this);
}

void Window::AddChild(Window *child)
{
    m_children.push_back(child);
}

void Window::RemoveChild(Window *child)
{
    m_children.remove(child);
}

bool Window::GetPosition(MGLrect *rect) const
{
    size_t size;
    size = sizeof(MGLrect);
    return WndGetAttribute(m_handle, WND_ATTR_POSITION, WND_TYPE_RECT, rect, &size);
}

void Window::SetPosition(const MGLrect &rect)
{
    WndSetAttribute(m_handle, WND_ATTR_POSITION, WND_TYPE_RECT, &rect, sizeof(rect));
}

bool Window::GetTitle(wchar_t *title, size_t max) const
{
    max *= sizeof(wchar_t);
    return WndGetAttribute(m_handle, WND_ATTR_TITLE, WND_TYPE_STRING, title, &max);
}

size_t Window::GetTitleLength() const
{
    size_t size;

    size = 0;
    if (!WndGetAttribute(m_handle, WND_ATTR_TITLE, WND_TYPE_STRING, NULL, &size) &&
        errno == ENOMEM)
        return (size / sizeof(wchar_t)) - 1;
    else
        return 0;
}

void Window::SetTitle(wchar_t *title)
{
    WndSetAttribute(m_handle, WND_ATTR_TITLE, WND_TYPE_STRING, 
        title, (wcslen(title) + 1) * sizeof(wchar_t));
}

void Window::Invalidate(MGLrect *rect)
{
    WndInvalidate(m_handle, rect);
}

void Window::SetFocus()
{
    WndSetFocus(m_handle);
}

void Window::PostMessage(uint32_t code, uint32_t param1, uint32_t param2, uint32_t param3)
{
    msg_t msg;
    msg.code = code;
    msg.p.params[0] = param1;
    msg.p.params[1] = param2;
    msg.p.params[2] = param3;
    WndPostMessage(m_handle, &msg);
}

bool Window::HasFocus() const
{
    return WndHasFocus(m_handle);
}

bool Window::SetCapture()
{
    return WndSetCapture(m_handle, true);
}

bool Window::ReleaseCapture()
{
    return WndSetCapture(m_handle, false);
}

Window *Window::GetNextChild(Window *child)
{
    std::list<Window*>::iterator it;

    it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
        ++it;

    if (it == m_children.end())
    {
        if (m_children.empty())
            return NULL;
        else
            return m_children.front();
    }
    else
        return *it;
}

void Window::HandleMessage(const msg_t *msg)
{
    MGLclip clip;
    size_t size;

    switch (msg->code)
    {
    case MSG_PAINT:
        mglUseRc(Application::GetApplication()->m_rc);

        clip.num_rects = msg->p.params[0];
        clip.rects = new MGLrect[clip.num_rects];
        size = clip.num_rects * sizeof(MGLrect);
        WndGetClip(m_handle, clip.rects, &size);
        mglSetClip(NULL, &clip);
        
        OnPaint();

        glFlush();
        clip.num_rects = 0;
        delete[] clip.rects;
        clip.rects = NULL;
        mglSetClip(NULL, &clip);
        break;

    case MSG_KEYDOWN:
        OnKeyDown(msg->p.key);
        break;

    case MSG_KEYUP:
        OnKeyUp(msg->p.key);
        break;

    case MSG_FOCUS:
        OnFocus();
        break;

    case MSG_BLUR:
        OnBlur();
        break;

    case MSG_MOUSEDOWN:
        OnMouseDown(msg->p.mouse.buttons, msg->p.mouse.x, msg->p.mouse.y);
        break;

    case MSG_MOUSEUP:
        OnMouseUp(msg->p.mouse.buttons, msg->p.mouse.x, msg->p.mouse.y);
        break;

    case MSG_MOUSEMOVE:
        OnMouseMove(msg->p.mouse.buttons, msg->p.mouse.x, msg->p.mouse.y);
        break;
    }
}

void Window::OnPaint()
{
    MGLrect rect;
    GetPosition(&rect);
    glSetColour(0x123456);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);
}

void Window::OnKeyDown(uint32_t key)
{
    if (m_parent != NULL)
        m_parent->OnKeyDown(key);
}

void Window::OnKeyUp(uint32_t key)
{
    if (m_parent != NULL)
        m_parent->OnKeyUp(key);
}

void Window::OnCommand(unsigned id)
{
}

void Window::OnFocus()
{
}

void Window::OnBlur()
{
}

void Window::OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y)
{
}

void Window::OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y)
{
}

void Window::OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y)
{
}

void Window::OnMouseWheel(int delta, MGLreal x, MGLreal y)
{
}
