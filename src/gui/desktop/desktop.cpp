/* $Id: desktop.cpp,v 1.1 2002/04/03 23:26:44 pavlovskii Exp $ */

#include "desktop.h"
#include "alttabwindow.h"
#include <os/syscall.h>
#include <os/defs.h>
#include <os/keyboard.h>
#include <os/rtl.h>
#include <unistd.h>
#include <stdlib.h>
#include <gl/mgl.h>
#include <gui/messagebox.h>
#include <gui/button.h>
#include <gui/edit.h>

#include "v86.h"

class RunDialog : public os::MessageBox
{
public:
    os::Edit m_edit;
    RunDialog();
};

RunDialog::RunDialog() : 
    os::MessageBox(L"Run", L"Enter the name of the program to run:", btnOkCancel),
    m_edit(this, L"", MGLrect(310, 500, 890, 530), 0)
{
}

Desktop::Desktop() : m_altTabWindow(NULL)
{
    MGLrect rect;
    m_handle = WndOwnRoot();
    mglGetDimensions(NULL, &rect);
    WndSetAttribute(m_handle, WND_ATTR_POSITION, WND_TYPE_RECT, &rect, sizeof(rect));
    WndSetAttribute(m_handle, WND_ATTR_USERDATA, WND_TYPE_VOID, this, sizeof(this));
    WndInvalidate(m_handle, NULL);

    new os::Button(this, L"Run...",       MGLrect(25, 25, 225, 75), 1);
    new os::Button(this, L"Shut Down...", MGLrect(25, 100, 225, 150), 2);
}

void Desktop::OnPaint()
{
    static const wchar_t str[] = L"The Möbius";
    MGLrect rect;
    MGLpoint size;

    GetPosition(&rect);
    glSetColour(0x0040C0);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);

    glSetColour(0x000000);
    glGetTextSize(str, -1, &size);
    rect.left = rect.right - size.x;
    rect.top = rect.bottom - size.y;
    glDrawText(&rect, str, -1);
}

void Desktop::PowerOff()
{
    static char *sh_v86stack;
    char *code;
    FARPTR fp_code, fp_stackend;
    handle_t thr;
    const void *rsrc;
    size_t size;
    
    rsrc = ResFindResource(NULL, 256, 1, 0);
    if (rsrc == NULL)
        return;
    
    size = ResSizeOfResource(NULL, 256, 1, 0);

    code = sbrk((size + 0x10000) & 0xffff);
    *(uint16_t*) code = 0x20cd;

    memcpy(code + 0x100, rsrc, size);
    
    fp_code = i386LinearToFp(code);
    fp_code = MK_FP(FP_SEG(fp_code), FP_OFF(fp_code) + 0x100);
        
    if (sh_v86stack == NULL)
        sh_v86stack = sbrk(65536);

    fp_stackend = i386LinearToFp(sh_v86stack);
    memset(sh_v86stack, 0, 65536);
    
    thr = ThrCreateV86Thread(fp_code, fp_stackend, 15, ShV86Handler);
    ThrWaitHandle(thr);

    /* xxx - need to clean up HndClose() implementation before we can use this */
    /*HndClose(thr);*/
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
    switch (id)
    {
    case 1:
    {
        RunDialog dlg;
        if (dlg.DoModal() == RunDialog::btnOk)
        {
            wchar_t *str;
            size_t size;
            handle_t proc;

            size = dlg.m_edit.GetTitleLength();
            str = new wchar_t[size + 1];
            dlg.m_edit.GetTitle(str, size + 1);

            proc = ProcSpawnProcess(str, ProcGetProcessInfo());
            /* xxx - need to close this */
            /*HndClose(proc);*/

            delete[] str;
        }

        break;
    }
        
    case 2:
    {
        os::MessageBox box(L"Shut Down The Möbius", 
            L"What do you want the computer to do?", 
            os::MessageBox::btnCustom1 | os::MessageBox::btnCustom2 | os::MessageBox::btnCancel,
            L"Reboot", L"Power Off");

        switch (box.DoModal())
        {
        case os::MessageBox::btnCustom1:
            SysShutdown(SHUTDOWN_REBOOT);
            break;

        case os::MessageBox::btnCustom2:
            PowerOff();
            break;
        }
    }
    };
}
