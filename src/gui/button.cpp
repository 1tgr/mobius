/* $Id: button.cpp,v 1.4 2002/09/13 23:26:02 pavlovskii Exp $ */

#include <gui/button.h>
//#include <gl/mgl.h>
#include <stdlib.h>
#include <stddef.h>
#include <os/keyboard.h>
#include <mgl/fontmanager.h>

using namespace os;

Button::Button(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id) :
    Control(parent, text, pos, id),
    m_isDown(false),
    m_mouseDown(false)
{
}

void Button::OnPaint(mgl::Rc *rc)
{
    MGLrect rect;
    wchar_t text[256];

    GetPosition(&rect);
    rc->SetFillColour(0x808080);
    rc->Bevel(rect, 2, 0x80, !m_isDown);
    rect.Inflate(-2, -2);
    rc->Bevel(rect, 2, 0x40, !m_isDown);
    rect.Inflate(-2, -2);

    rc->FillRect(rect);

    if (GetTitle(text, _countof(text)))
    {
        MGLpoint size;
        size = mgl::FontManager::GetDefault(0)->GetTextSize(rc, text, -1);
        rc->SetPenColour(0x000000);
        rect.left = (rect.left + rect.right - size.x) / 2;
        rect.top = (rect.top + rect.bottom - size.y) / 2;

        if (m_isDown)
        {
            rect.left += 2;
            rect.top += 2;
        }

        mgl::FontManager::GetDefault(0)->DrawText(rc, rect, text, -1);
    }
}

#include <os/syscall.h>
#include <wchar.h>

void Button::OnKeyDown(uint32_t key)
{
    if ((key & ~KBD_BUCKY_ANY) == ' ')
    {
        if (!m_isDown)
        {
            Invalidate();
            m_isDown = true;
        }
    }
    else
        Control::OnKeyDown(key);
}

void Button::OnKeyUp(uint32_t key)
{
    if ((key & ~(KBD_BUCKY_RELEASE | KBD_BUCKY_ANY)) == ' ')
    {
        if (m_isDown)
        {
            m_isDown = false;
            Invalidate();
            OnClick();
        }
    }
    else
        Control::OnKeyUp(key);
}

void Button::OnClick()
{
    m_parent->OnCommand(m_id);
}

void Button::OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y)
{
    m_isDown = true;
    m_mouseDown = true;
    Invalidate();
    SetCapture();
}

void Button::OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y)
{
    if (m_mouseDown)
    {
        ReleaseCapture();
        m_mouseDown = false;
    }

    if (m_isDown)
    {
        m_isDown = false;
        Invalidate();
        OnClick();
    }
}

void Button::OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y)
{
    if (m_mouseDown)
    {
        bool wasDown;
        MGLrect pos;
        GetPosition(&pos);
        wasDown = m_isDown;
        m_isDown = pos.IncludesPoint(x, y);
        if (m_isDown != wasDown)
            Invalidate();
    }
}
