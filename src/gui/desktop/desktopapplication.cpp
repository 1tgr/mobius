/* $Id: desktopapplication.cpp,v 1.2 2002/09/13 23:26:02 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/keyboard.h>
#include <os/mouse.h>
#include <gui/application.h>
#include <gui/messagebox.h>
#include <wchar.h>
#include <errno.h>

#include "desktop.h"

class DesktopApplication : public os::Application
{
public:
    Desktop m_desktop;

    DesktopApplication(const wchar_t *name);
    static void KeyboardThread();
    static void MouseThread();
    bool HookKeyboardInput(uint32_t key);
};

DesktopApplication::DesktopApplication(const wchar_t *name) : 
    Application(name)
{
    ThrCreateThread(KeyboardThread, NULL, 10);
    ThrCreateThread(MouseThread, NULL, 8);
    ProcSpawnProcess(SYS_BOOT L"/guitest.exe", ProcGetProcessInfo());
}

void DesktopApplication::KeyboardThread()
{
    DesktopApplication *app;
    wndinput_t key;

    while (true)
    {
        key.data.key = ConReadKey();

        if (key.data.key & KBD_BUCKY_RELEASE)
            key.type = WND_INPUT_KEY_UP;
        else
            key.type = WND_INPUT_KEY_DOWN;

        app = static_cast<DesktopApplication*>(Application::GetApplication());
        if (!app->HookKeyboardInput(key.data.key))
            WndQueueInput(&key);
    }

    ThrExitThread(0);
}

void DesktopApplication::MouseThread()
{
    handle_t mouse;
    mouse_packet_t pkt;
    size_t bytes;
    wchar_t str[50] = SYS_DEVICES L"/Classes/mouse0";
    int err;
    MGLpoint pt;
    MGLrect dims;
    wndinput_t input;
    uint32_t old_buttons;
    mgl::Rc *rc;

    mouse = FsOpen(str, FILE_READ);
    if (mouse == NULL)
    {
        err = errno;
        DbgWrite(str, wcslen(str));
        /*os::MessageBox box(SYS_DEVICES L"/ps2mouse", _wcserror(err));
        box.DoModal();*/
        ThrExitThread(err);
    }

    rc = &os::Application::GetApplication()->m_rc;
    dims = rc->GetDimensions();
    pt.x = (dims.left + dims.right) / 2;
    pt.y = (dims.top + dims.bottom) / 2;
    old_buttons = 0;
    while (true)
    {
        // xxx -- reimplement this
        //mglMoveCursor(pt);
        rc->SetFillColour(0xffffff);
        rc->FillRect(MGLrect(pt.x - 10, pt.y - 10, pt.x + 10, pt.y + 10));
        DbgWrite(str, 
            swprintf(str, L"%d, %d\n", pt.x, pt.y));

        if (!FsReadSync(mouse, &pkt, sizeof(pkt), &bytes))
        {
            err = errno;
            break;
        }

        if (pkt.dx || pkt.dy)
        {
            pt.x += pkt.dx;
            if (pt.x < dims.left)
                pt.x = dims.left;
            if (pt.x >= dims.right)
                pt.x = dims.right - 1;

            pt.y += pkt.dy;
            if (pt.y < dims.top)
                pt.y = dims.top;
            if (pt.y >= dims.bottom)
                pt.y = dims.bottom - 1;

            input.type = WND_INPUT_MOUSE_MOVE;
            input.data.mouse_move.x = pt.x;
            input.data.mouse_move.y = pt.y;
            WndQueueInput(&input);
        }

        input.data.mouse_move.x = pt.x;
        input.data.mouse_move.y = pt.y;
        if (pkt.buttons != old_buttons)
        {
            if (pkt.buttons & ~old_buttons)
            {
                input.type = WND_INPUT_MOUSE_DOWN;
                input.data.mouse_down.buttons = pkt.buttons & ~old_buttons;
                WndQueueInput(&input);
            }

            if (~pkt.buttons & old_buttons)
            {
                input.type = WND_INPUT_MOUSE_UP;
                input.data.mouse_up.buttons = ~pkt.buttons & old_buttons;
                WndQueueInput(&input);
            }

            old_buttons = pkt.buttons;
        }

        if (pkt.dwheel)
        {
            input.type = WND_INPUT_MOUSE_WHEEL;
            input.data.mouse_wheel.dwheel = pkt.dwheel;
            WndQueueInput(&input);
        }
    }

    DbgWrite(str, 
        swprintf(str, L"Finished: %s\n", _wcserror(err)));
    HndClose(mouse);
    ThrExitThread(0);
}

bool DesktopApplication::HookKeyboardInput(uint32_t key)
{
    if (key & KBD_BUCKY_RELEASE)
    {

    }
    else
    {
        if (key == (KBD_BUCKY_ALT | 9) && 
            m_desktop.m_altTabWindow == NULL)
        {
            m_desktop.PostMessage(MSG_KEYDOWN, key);
            return true;
        }

        if (key == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
        {
            m_desktop.PostMessage(MSG_KEYDOWN, key);
            return true;
        }
    }

    return false;
}

int main()
{
    DesktopApplication app(L"application/x-vnd.desktop");
    return app.Run();
}
