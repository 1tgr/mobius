/* $Id: guitest.cpp,v 1.1 2002/04/03 23:26:02 pavlovskii Exp $ */

#include <gui/application.h>
#include <gui/dialog.h>
#include <gui/button.h>
#include <gui/edit.h>
#include <gui/messagebox.h>

#include <stdlib.h>
#include <stdio.h>
#include <os/syscall.h>
#include <os/rtl.h>

class TestApplication : public os::Application
{
public:
    os::Dialog m_dialog;
    os::Button m_button;
    os::Edit m_edit;

    TestApplication(const wchar_t *name);
};

TestApplication::TestApplication(const wchar_t *name) : 
    Application(name),
    m_dialog(L"A Dialog", MGLrect(100, 100, 400, 400)),
    m_button(&m_dialog, L"Button", MGLrect(150, 150, 350, 200), 1),
    m_edit(&m_dialog, L"Edit", MGLrect(150, 225, 350, 275), 1)
{
}

int main()
{
    TestApplication app(L"application/x-vnd.guitest");
    return app.Run();
}
