/* $Id: messagebox.cpp,v 1.3 2002/09/13 23:26:02 pavlovskii Exp $ */

#include <gui/messagebox.h>
#include <gui/button.h>
#include <gui/label.h>
//#include <gl/mgl.h>
#include <os/keyboard.h>
#include <wchar.h>
#include <stdarg.h>

using namespace os;

static const struct
{
    const wchar_t *title;
    uint32_t key;
} btnInfo[] =
{
    { L"OK",     '\n' },
    { L"Cancel", 27 },
    { L"Yes",    'y' },
    { L"No",     'n' },
    { L"Help",   KEY_F1 },
};

MessageBox::MessageBox(const wchar_t *title, const wchar_t *text, unsigned buttons, ...) :
    Dialog(title, MGLrect(300, 400, 900, 600))
{
    unsigned i;
    MGLrect button = MGLrect(750, 550, 900, 600), 
        label = MGLrect(300, 400, 900, 450);
    va_list ptr;

    if (text != NULL)
    {
        label.left += m_margins.left * 2;
        label.right -= m_margins.right * 2;
        label.Offset(0, m_margins.top + m_margins.bottom * 2);
        new Label(this, text, label);
    }

    va_start(ptr, buttons);
    button.Offset(-m_margins.right * 2, -m_margins.bottom * 2);
    for (i = 0; i < 32; i++)
    {
        if (buttons & (1 << i))
        {
            const wchar_t *title;

            if (i < 16)
                title = btnInfo[i].title;
            else
                title = va_arg(ptr, const wchar_t*);

            new Button(this, title, button, 1 << i);
            button.Offset(button.left - button.right - m_margins.right, 0);
        }
    }

    va_end(ptr);
}

MessageBox::~MessageBox()
{
    while (!m_children.empty())
        delete m_children.front();
}

void MessageBox::OnKeyDown(uint32_t key)
{
    unsigned i;

    if (key & ~KBD_BUCKY_ANY)
    {
        key = key & ~KBD_BUCKY_ANY;
        if (key < 0x10000)
            key = towlower(key);
        for (i = 0; i < _countof(btnInfo); i++)
            if (key == btnInfo[i].key)
            {
                OnCommand(1 << i);
                break;
            }
    }
    else
        Dialog::OnKeyDown(key);
}

void MessageBox::OnClose()
{
    /* Don't call Window::OnClose because that will delete this */
    OnCommand(btnCancel);
}
