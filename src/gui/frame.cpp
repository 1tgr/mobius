/* $Id: frame.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

#include <stdio.h>
#include <gui/frame.h>
#include <gl/mgl.h>
#include <stdlib.h>
#include <algorithm>

using namespace os;

Frame::Frame(const wchar_t *text, const MGLrect &pos) : 
    Container(0, text, pos),
    m_margins(5, 35, 5, 5)
{
}

void Frame::OnPaint()
{
    static const MGLcolour frameColours[2] = { 0xAAAAAA, 0xFFA000 };
    MGLrect rect;
    wchar_t text[256];

    GetPosition(&rect);

    glSetColour(frameColours[HasFocus()]);
    glFillRect(rect.left, rect.top, 
        rect.left + m_margins.left, rect.bottom);
    glFillRect(rect.left + m_margins.left, rect.top, 
        rect.right - m_margins.right, rect.top + m_margins.top);
    glFillRect(rect.right - m_margins.right, rect.top, 
        rect.right, rect.bottom);
    glFillRect(rect.left + m_margins.left, rect.bottom - m_margins.bottom, 
        rect.right - m_margins.right, rect.bottom);
    glBevel(&rect, 0xFFA000, 1, 0x40, true);

    if (GetTitle(text, _countof(text)))
    {
        MGLpoint size;
        glSetColour(0x000000);
        rect = MGLrect(rect.left + m_margins.left, rect.top, 
            rect.right - m_margins.right, rect.top + m_margins.top);
        glGetTextSize(text, -1, &size);
        rect.left = (rect.left + rect.right - size.x) / 2;
        rect.top = (rect.top + rect.bottom - size.y) / 2;
        rect.right = rect.left + size.x;
        rect.bottom = rect.top + size.y;
        glDrawText(&rect, text, -1);
    }
}

void Frame::OnFocus()
{
    Invalidate();
}

void Frame::OnBlur()
{
    Invalidate();
}

void Frame::OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y)
{
    SetFocus();
}
