/* $Id: alttabwindow.cpp,v 1.3 2002/09/13 23:26:02 pavlovskii Exp $ */

#include "alttabwindow.h"
#include "desktop.h"
#include <stdlib.h>
//#include <gl/mgl.h>
#include <os/keyboard.h>
#include <mgl/fontmanager.h>
#include <gui/application.h>

AltTabWindow::AltTabWindow(os::Window *parent)
{
    MGLrect rect;
    MGLreal width, height;
    rect = os::Application::GetApplication()->m_rc.GetDimensions();
    width = rect.Width();
    height = rect.Height();
    rect.left = (rect.left * 2 + rect.right) / 3;
    rect.top = (rect.top * 2 + rect.bottom) / 3;
    rect.right = rect.right - width / 3;
    rect.bottom = rect.bottom - height / 3;
    Create(parent, NULL, rect);
}

void AltTabWindow::OnPaint(mgl::Rc *rc)
{
    static const wchar_t text[] = L"Alt+Tab Window";
    MGLrect rect;
    MGLpoint size;

    GetPosition(&rect);
    rc->SetPenColour(0x808080);
    rc->Bevel(rect, 2, 0x40, false);
    rect.Inflate(-2, -2);
    rc->SetFillColour(0x808080);
    rc->FillRect(rect);

    mgl::Font *font = mgl::FontManager::GetDefault(0);
    size = font->GetTextSize(rc, text, -1);
    rc->SetPenColour(0x000000);
    rect.left = (rect.left + rect.right - size.x) / 2;
    rect.top = (rect.top + rect.bottom - size.y) / 2;
    font->DrawText(rc, rect, text, -1);
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
