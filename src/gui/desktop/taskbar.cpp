/* $Id: taskbar.cpp,v 1.2 2002/05/05 13:52:28 pavlovskii Exp $ */

#include "taskbar.h"
#include <gl/mgl.h>
#include <gui/messagebox.h>
#include <gui/edit.h>
#include <gui/button.h>
#include <os/rtl.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <stdlib.h>
#include "v86.h"

class RunDialog : public os::MessageBox
{
public:
    os::Edit m_edit;
    RunDialog();
};

RunDialog::RunDialog() : 
    os::MessageBox(L"Run", L"Enter the name of the program to run:", btnOkCancel),
    m_edit(this, L"", MGLrect(310, 490, 890, 530), 0)
{
}

Taskbar::Taskbar(os::Container *parent) : 
    os::Container(parent, NULL, MGLrect(0, 0, 0, 0))
{
    parent->AddView(this, os::alTop, 50);
    AddView(new os::Button(this, L"Run...", MGLrect(0, 0, 0, 0), 1),
        os::alLeft,
        200);
    AddView(new os::Button(this, L"Shut Down...", MGLrect(0, 0, 0, 0), 2),
        os::alLeft,
        200);
}

Taskbar::~Taskbar()
{
    static_cast<Container*>(m_parent)->RemoveView(this);
}

void Taskbar::OnPaint()
{
    MGLrect pos;
    GetPosition(&pos);
    glSetColour(0x000080);
    glFillRect(pos.left, pos.top, pos.right, pos.bottom);
}

void Taskbar::PowerOff()
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

    code = (char*) VmmAlloc(PAGE_ALIGN_UP((size + 0x10000) & 0xffff) / PAGE_SIZE,
        NULL,
        MEM_READ | MEM_WRITE);
    *(uint16_t*) code = 0x20cd;

    memcpy(code + 0x100, rsrc, size);
    
    fp_code = i386LinearToFp(code);
    fp_code = MK_FP(FP_SEG(fp_code), FP_OFF(fp_code) + 0x100);
        
    if (sh_v86stack == NULL)
        sh_v86stack = (char*) VmmAlloc(PAGE_ALIGN_UP(65536) / PAGE_SIZE,
            NULL,
            MEM_READ | MEM_WRITE);

    fp_stackend = i386LinearToFp(sh_v86stack);
    memset(sh_v86stack, 0, 65536);
    
    thr = ThrCreateV86Thread(fp_code, fp_stackend, 15, ShV86Handler);
    ThrWaitHandle(thr);

    /* xxx - need to clean up HndClose() implementation before we can use this */
    /*HndClose(thr);*/
}

void Taskbar::OnCommand(unsigned id)
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
