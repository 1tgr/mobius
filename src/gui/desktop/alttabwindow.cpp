/* $Id: alttabwindow.cpp,v 1.2 2002/04/10 12:25:45 pavlovskii Exp $ */

#include "alttabwindow.h"
#include "desktop.h"
#include <stdlib.h>
#include <gl/mgl.h>
#include <os/keyboard.h>

AltTabWindow::AltTabWindow(os::Window *parent)
{
    MGLrect rect;
    MGLreal width, height;
    mglGetDimensions(NULL, &rect);
    width = rect.Width();
    height = rect.Height();
    rect.left = (rect.left * 2 + rect.right) / 3;
    rect.top = (rect.top * 2 + rect.bottom) / 3;
    rect.right = rect.right - width / 3;
    rect.bottom = rect.bottom - height / 3;
    Create(parent, NULL, rect);
}

void AltTabWindow::OnPaint()
{
    static const wchar_t text[] = L"Alt+Tab Window";
    MGLrect rect;
    MGLpoint size;

    GetPosition(&rect);
    glBevel(&rect, 0x808080, 2, 0x40, false);
    RectInflate(&rect, -2, -2);
    glSetColour(0x808080);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);

    glGetTextSize(text, -1, &size);
    glSetColour(0x000000);
    rect.left = (rect.left + rect.right - size.x) / 2;
    rect.top = (rect.top + rect.bottom - size.y) / 2;
    glDrawText(&rect, text, -1);
}

void AltTabWindow::OnKeyUp(uint32_t key)
{
    if (key == (KBD_BUCKY_ALT | KBD_BUCKY_RELEASE))
    {
        static_cast<Desktop*>(m_parent)->m_altTabWindow = NULL;
        delete this;
    }
    else
        Window::OnKeyUp(key);
}
