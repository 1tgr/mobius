/* $Id: edit.cpp,v 1.4 2002/09/13 23:26:02 pavlovskii Exp $ */

#include <gui/edit.h>
//#include <gl/mgl.h>
#include <wchar.h>
#include <stdlib.h>
#include <os/keyboard.h>
#include <mgl/fontmanager.h>

using namespace os;

Edit::Edit(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id) : 
    Control(parent, text, pos, id)
{
}

void Edit::OnPaint(mgl::Rc *rc)
{
    MGLrect rect;
    wchar_t text[256];

    GetPosition(&rect);

    rc->SetPenColour(0x808080);
    rc->Bevel(rect, 4, 0x40, false);
    rect.Inflate(-4, -4);

    rc->SetFillColour(0xffffff);
    rc->FillRect(MGLrect(rect.left, rect.top, rect.right, rect.bottom));

    rc->SetPenColour(0x000000);
    GetTitle(text, _countof(text));
    mgl::FontManager::GetDefault(0)->DrawText(rc, rect, text, -1);
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
    
    case '\n':
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

void Edit::OnMouseDown(uint32_t button, MGLreal x, MGLreal y)
{
    SetFocus();
}
