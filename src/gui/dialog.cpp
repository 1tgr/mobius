/* $Id: dialog.cpp,v 1.2 2002/09/13 23:26:02 pavlovskii Exp $ */

#include <gui/dialog.h>
//#include <gl/mgl.h>
#include <gui/application.h>

using namespace os;

Dialog::Dialog(const wchar_t *text, const MGLrect &pos) : Frame(text, pos)
{
}

void Dialog::OnPaint(mgl::Rc *rc)
{
    MGLrect rect;

    Frame::OnPaint(rc);

    GetPosition(&rect);
    rect.left += m_margins.left;
    rect.top += m_margins.top;
    rect.right -= m_margins.right;
    rect.bottom -= m_margins.bottom;

    rc->SetFillColour(0xa0a0a0);
    rc->FillRect(MGLrect(rect.left, rect.top, rect.right, rect.bottom));
}

unsigned Dialog::DoModal()
{
    Application *app;
    msg_t msg;

    app = Application::GetApplication();
    m_modalResult = 0;
    do
    {
        if (!app->GetMessage(&msg))
            return 0;

        app->HandleMessage(&msg);
    } while (m_modalResult == 0);

    return m_modalResult;
}

void Dialog::OnCommand(unsigned id)
{
    m_modalResult = id;
}
