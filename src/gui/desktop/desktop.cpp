/* $Id: desktop.cpp,v 1.3 2002/04/11 00:31:01 pavlovskii Exp $ */

#include "desktop.h"
#include "alttabwindow.h"
#include "iconview.h"
#include "taskbar.h"

#include <os/keyboard.h>
#include <os/syscall.h>

#include <gl/mgl.h>

Desktop::Desktop() : m_altTabWindow(NULL)
{
    MGLrect rect;

    m_handle = WndOwnRoot();
    mglGetDimensions(NULL, &rect);
    WndSetAttribute(m_handle, WND_ATTR_POSITION, WND_TYPE_RECT, &rect, sizeof(rect));
    WndSetAttribute(m_handle, WND_ATTR_USERDATA, WND_TYPE_VOID, this, sizeof(this));
    WndInvalidate(m_handle, NULL);

    m_taskbar = new Taskbar(this);
    m_icons = new IconView(this);

    /*
     * xxx - I shouldn't need to do this...
     * Seems like the first child created doesn't get invalidated. Wonder why.
     */
    m_taskbar->Invalidate();
}

void Desktop::OnPaint()
{
}

void Desktop::OnKeyDown(uint32_t key)
{
    if (key == (KBD_BUCKY_ALT | 9) && m_altTabWindow == NULL)
        m_altTabWindow = new AltTabWindow(this);
    else if (key == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
        OnCommand(2);
    else
        Window::OnKeyDown(key);
}

void Desktop::OnCommand(unsigned id)
{
    /*
     * We get various commands sent by the input server (e.g. Ctrl+Alt+Del).
     * The task bar handles these.
     */
    m_taskbar->OnCommand(id);
}
