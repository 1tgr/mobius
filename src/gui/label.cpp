/* $Id: label.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

#include <gui/label.h>
#include <gl/mgl.h>

using namespace os;

Label::Label(Window *parent, const wchar_t *text, const MGLrect &pos) :
    Control(parent, text, pos, 0)
{
}

void Label::OnPaint()
{
    MGLrect rect;
    wchar_t text[256];

    GetPosition(&rect);

    glSetColour(0xa0a0a0);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);

    glSetColour(0x000000);
    GetTitle(text, _countof(text));
    glDrawText(&rect, text, -1);
}
