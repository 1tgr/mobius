/* $Id: edit.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

#include <gui/edit.h>
#include <gl/mgl.h>
#include <wchar.h>
#include <stdlib.h>
#include <os/keyboard.h>

using namespace os;

Edit::Edit(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id) : 
    Control(parent, text, pos, id)
{
}

void Edit::OnPaint()
{
    MGLrect rect;
    wchar_t text[256];

    GetPosition(&rect);
    
    glBevel(&rect, 0x808080, 4, 0x40, false);
    RectInflate(&rect, -4, -4);

    glSetColour(0xffffff);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);
    
    glSetColour(0x000000);
    GetTitle(text, _countof(text));
    glDrawText(&rect, text, -1);
}

void Edit::OnKeyDown(uint32_t key)
{
    wchar_t text[256], str[2];
    bool changed;

    text[0] = '\0';
    GetTitle(text, _countof(text));
    changed = false;
    key = key & ~KBD_BUCKY_ANY;

    switch (key)
    {
    case '\b':
        if (text[0] != '\0')
        {
            text[wcslen(text) - 1] = '\0';
            changed = true;
        }
        break;
    
    case '\t':
        Control::OnKeyDown(key);
        break;

    default:
        if (key != 0 && key != '\t')
        {
            str[0] = key;
            str[1] = '\0';
            wcscat(text, str);
            changed = true;
        }
    }

    if (changed)
    {
        SetTitle(text);
        Invalidate();
    }
}
