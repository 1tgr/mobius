/* $Id: frame.cpp,v 1.2 2002/04/10 12:27:44 pavlovskii Exp $ */

#include <stdio.h>
#include <gui/frame.h>
#include <gl/mgl.h>
#include <stdlib.h>
#include <algorithm>
#include <stdarg.h>
#include <wchar.h>
#include <os/syscall.h>

using namespace os;

int dwprintf(const wchar_t *fmt, ...)
{
    va_list ptr;
    int chars;
    wchar_t buf[512];
    va_start(ptr, fmt);
    chars = vswprintf(buf, fmt, ptr);
    DbgWrite(buf, chars);
    va_end(ptr);
    return chars;
}

Frame::Frame(const wchar_t *text, const MGLrect &pos) : 
    Container(0, text, pos),
    m_margins(5, 35, 5, 5),
    m_isMoving(false)
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
    glBevel(&rect, 0xFFA000, 2, 0x40, true);

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

    if (HitTest(x, y) == rgnTitle)
    {
        m_isMoving = true;
        m_lastMovePoint = MGLpoint(x, y);
        GetPosition(&m_dragRect);
        glSetColour(0xffffffff);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        SetCapture();
    }
}

void Frame::OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y)
{
    if (m_isMoving)
    {
        m_isMoving = false;
        ReleaseCapture();
        glSetColour(0xffffffff);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        SetPosition(m_dragRect);
    }
}

void Frame::OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y)
{
    if (m_isMoving)
    {
        glSetColour(0xffffffff);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        RectOffset(&m_dragRect, x - m_lastMovePoint.x, y - m_lastMovePoint.y);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        glFlush();
        m_lastMovePoint = MGLpoint(x, y);
    }
}

Frame::FrameRegion Frame::HitTest(MGLreal x, MGLreal y)
{
    MGLrect client;
    
    GetPosition(&client);
    client.left += m_margins.left;
    client.top += m_margins.top;
    client.right -= m_margins.right;
    client.bottom -= m_margins.bottom;

    if (RectIncludesPoint(&client, x, y))
        return rgnClient;
    else
        return rgnTitle;
}
