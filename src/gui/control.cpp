/* $Id: control.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

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
