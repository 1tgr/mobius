/* $Id: control.cpp,v 1.2 2002/04/10 12:27:44 pavlovskii Exp $ */

#include <gui/control.h>
#include <os/keyboard.h>

using namespace os;

Control::Control(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id) : 
    Window(parent, text, pos),
    m_id(id)
{
}

void Control::OnKeyDown(uint32_t key)
{
    switch (key & ~KBD_BUCKY_ANY)
    {
    case '\t':
        m_parent->GetNextChild(this)->SetFocus();
        break;
    default:
        Window::OnKeyDown(key);
        break;
    }
}

void Control::OnFocus()
{
    Invalidate();
}

void Control::OnBlur()
{
    Invalidate();
}
