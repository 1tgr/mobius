/* $Id: frame.cpp,v 1.3 2002/04/20 12:47:28 pavlovskii Exp $ */

#include <stdio.h>
#include <gui/frame.h>
#include <gl/mgl.h>
#include <stdlib.h>
#include <algorithm>
#include <stdarg.h>
#include <wchar.h>
#include <os/syscall.h>

using namespace os;

#define SIZE_CORNER         m_margins.top
#define SIZE_BUTTON         (m_margins.top - m_margins.bottom)
#define NUM_BUTTONS_RIGHT   3
#define NUM_BUTTONS_LEFT    1

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
    Container(0, text, pos)
{
    m_margins = MGLrect(8, 38, 8, 8);
    m_mouseRegion = rgnNone;
}

void Frame::OnPaint()
{
    static const MGLcolour frameColours[2] = { 0xAAAAAA, 0xFFA000 };
    static const wchar_t l_buttons[NUM_BUTTONS_LEFT][2] = 
        { { 0x263a, 0 } };
    static const wchar_t r_buttons[NUM_BUTTONS_RIGHT][2] = 
        { { 0x263c, 0 }, { 0x25ca, 0 }, { 0x25a1, 0 } };
    MGLrect rect, r;
    wchar_t text[256];
    unsigned i;
    MGLpoint size;

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

    rect.left += m_margins.left;
    rect.top += m_margins.bottom;
    rect.right -= m_margins.right;
    rect.bottom = rect.top + m_margins.top;
    glSetColour(0xA0A0A0);
    glFillRect(rect.left, rect.top, 
        rect.left + (SIZE_BUTTON * NUM_BUTTONS_LEFT), rect.bottom);
    glFillRect(rect.right - (SIZE_BUTTON * NUM_BUTTONS_RIGHT), rect.top, 
        rect.right, rect.bottom);

    rect.top -= m_margins.bottom;
    glSetColour(0x000000);
    for (i = 0; i < NUM_BUTTONS_LEFT; i++)
    {
        r = MGLrect(rect.left, rect.top, rect.left + SIZE_BUTTON, rect.bottom);
        glGetTextSize(l_buttons[i], -1, &size);
        r.left = (r.left + r.right - size.x) / 2;
        r.top = (r.top + r.bottom - size.y) / 2;
        r.right = r.left + size.x;
        r.bottom = r.top + size.y;
        glDrawText(&r, l_buttons[i], -1);
        rect.left += SIZE_BUTTON;
    }

    for (i = 0; i < NUM_BUTTONS_RIGHT; i++)
    {
        r = MGLrect(rect.right - SIZE_BUTTON, rect.top, rect.right, rect.bottom);
        glGetTextSize(r_buttons[i], -1, &size);
        r.left = (r.left + r.right - size.x) / 2;
        r.top = (r.top + r.bottom - size.y) / 2;
        r.right = r.left + size.x;
        r.bottom = r.top + size.y;
        glDrawText(&r, r_buttons[i], -1);
        rect.right -= SIZE_BUTTON;
    }

    if (GetTitle(text, _countof(text)))
    {
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

    m_mouseRegion = HitTest(x, y);
    if (m_mouseRegion != rgnNone)
    {
        m_lastMovePoint = MGLpoint(x, y);
        OnHitBegin(m_mouseRegion, x, y);
        SetCapture();
    }
}

void Frame::OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y)
{
    if (m_mouseRegion != rgnNone)
    {
        FrameRegion rgn = m_mouseRegion;
        OnHitMiddle(rgn, x, y);
        m_mouseRegion = rgnNone;
        ReleaseCapture();
        OnHitEnd(rgn, x, y);
    }
}

void Frame::OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y)
{
    if (m_mouseRegion != rgnNone)
    {
        OnHitMiddle(m_mouseRegion, x, y);
        m_lastMovePoint = MGLpoint(x, y);
    }
}

void Frame::OnSize(const MGLrect &rect)
{
    RecalcLayout();
}

Frame::FrameRegion Frame::HitTest(MGLreal x, MGLreal y)
{
    MGLrect rect, client;
    unsigned i;
    
    GetPosition(&client);
    rect = client;
    client.left += m_margins.left;
    client.top += m_margins.top;
    client.right -= m_margins.right;
    client.bottom -= m_margins.bottom;

    if (RectIncludesPoint(&client, x, y))
        return rgnClient;
    else
    {
        client.left = rect.left + m_margins.left;
        client.top = rect.top + m_margins.bottom;
        client.right = rect.right - m_margins.right;
        client.bottom = rect.bottom - m_margins.bottom;
        if (RectIncludesPoint(&client, x, y))
        {
            MGLreal ax;

            ax = client.left;
            for (i = 0; i < NUM_BUTTONS_LEFT; i++)
            {
                ax += SIZE_BUTTON;
                if (x <= ax)
                    return (FrameRegion) (rgnButtonA + i);
            }

            ax = client.right;
            for (i = 0; i < NUM_BUTTONS_RIGHT; i++)
            {
                ax -= SIZE_BUTTON;
                if (x >= ax)
                    return (FrameRegion) (rgnButton1 + i);
            }

            return rgnTitle;
        }
        else if (x <= rect.left + SIZE_CORNER)
        {
            if (y <= rect.top + SIZE_CORNER)
                return rgnSizeTopLeft;
            else if (y >= rect.bottom - SIZE_CORNER)
                return rgnSizeBottomLeft;
            else
                return rgnSizeLeft;
        }
        else if (x >= rect.right - SIZE_CORNER)
        {
            if (y <= rect.top + SIZE_CORNER)
                return rgnSizeTopRight;
            else if (y >= rect.bottom - SIZE_CORNER)
                return rgnSizeBottomRight;
            else
                return rgnSizeRight;
        }
        else if (y <= rect.top + SIZE_CORNER)
        {
            if (x <= rect.left + SIZE_CORNER)
                return rgnSizeTopLeft;
            else if (x >= rect.right - SIZE_CORNER)
                return rgnSizeTopRight;
            else
                return rgnSizeTop;
        }
        else if (y >= rect.bottom - SIZE_CORNER)
        {
            if (x <= rect.left + SIZE_CORNER)
                return rgnSizeBottomLeft;
            else if (x >= rect.right - SIZE_CORNER)
                return rgnSizeBottomRight;
            else
                return rgnSizeBottom;
        }
        else
            return rgnTitle;
    }
}

void Frame::OnHitBegin(FrameRegion rgn, MGLreal x, MGLreal y)
{
    switch (rgn)
    {
    case rgnTitle:
    case rgnSizeTop:
    case rgnSizeBottom:
    case rgnSizeLeft:
    case rgnSizeRight:
    case rgnSizeTopLeft:
    case rgnSizeTopRight:
    case rgnSizeBottomRight:
    case rgnSizeBottomLeft:
        GetPosition(&m_dragRect);
        glSetColour(0xffffffff);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        break;

    default:
        break;
    }
}

void Frame::OnHitMiddle(FrameRegion rgn, MGLreal x, MGLreal y)
{
    MGLrect offset = MGLrect(0, 0, 0, 0);

    switch (rgn)
    {
    case rgnTitle:
        offset.left = offset.right = x - m_lastMovePoint.x;
        offset.top = offset.bottom = y - m_lastMovePoint.y;
        break;

    case rgnSizeTop:
        offset.top = y - m_lastMovePoint.y;
        break;

    case rgnSizeBottom:
        offset.bottom = y - m_lastMovePoint.y;
        break;

    case rgnSizeLeft:
        offset.left = x - m_lastMovePoint.x;
        break;

    case rgnSizeRight:
        offset.right = x - m_lastMovePoint.x;
        break;

    case rgnSizeTopLeft:
        offset.left = x - m_lastMovePoint.x;
        offset.top = y - m_lastMovePoint.y;
        break;

    case rgnSizeTopRight:
        offset.right = x - m_lastMovePoint.x;
        offset.top = y - m_lastMovePoint.y;
        break;

    case rgnSizeBottomRight:
        offset.right = x - m_lastMovePoint.x;
        offset.bottom = y - m_lastMovePoint.y;
        break;

    case rgnSizeBottomLeft:
        offset.left = x - m_lastMovePoint.x;
        offset.bottom = y - m_lastMovePoint.y;
        break;

    default:
        break;
    }

    if (offset.left != 0 || offset.top != 0 || 
        offset.right != 0 || offset.bottom != 0)
    {
        glSetColour(0xffffffff);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);

        m_dragRect.left += offset.left;
        m_dragRect.top += offset.top;
        m_dragRect.right += offset.right;
        m_dragRect.bottom += offset.bottom;

        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        glFlush();
    }
}

void Frame::OnHitEnd(FrameRegion rgn, MGLreal x, MGLreal y)
{
    MGLrect dims;

    switch (rgn)
    {
    case rgnTitle:
    case rgnSizeTop:
    case rgnSizeBottom:
    case rgnSizeLeft:
    case rgnSizeRight:
    case rgnSizeTopLeft:
    case rgnSizeTopRight:
    case rgnSizeBottomRight:
    case rgnSizeBottomLeft:
        dwprintf(L"OnHitEnd: %d (%d,%d), (%d,%d)\n", 
            rgn, m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        glSetColour(0xffffffff);
        glRectangle(m_dragRect.left, m_dragRect.top, 
            m_dragRect.right, m_dragRect.bottom);
        SetPosition(m_dragRect);
        break;

    case rgnButtonClose:
        PostMessage(MSG_CLOSE);
        break;

    case rgnButton2:
        mglGetDimensions(NULL, &dims);
        SetPosition(dims);
        break;

    default:
        break;
    }
}
