/* $Id: label.cpp,v 1.2 2002/09/13 23:26:02 pavlovskii Exp $ */

#include <gui/label.h>
//#include <gl/mgl.h>
#include <mgl/fontmanager.h>

using namespace os;

Label::Label(Window *parent, const wchar_t *text, const MGLrect &pos) :
    Control(parent, text, pos, 0)
{
}

void Label::OnPaint(mgl::Rc *rc)
{
    MGLrect rect;
    wchar_t text[256];

    GetPosition(&rect);

    rc->SetFillColour(0xa0a0a0);
    rc->FillRect(MGLrect(rect.left, rect.top, rect.right, rect.bottom));

    rc->SetPenColour(0x000000);
    GetTitle(text, _countof(text));
    mgl::FontManager::GetDefault(0)->DrawText(rc, rect, text, -1);
}
