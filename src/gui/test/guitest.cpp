/* $Id: guitest.cpp,v 1.4 2002/12/18 23:16:56 pavlovskii Exp $ */

#include <gui/application.h>
#include <gui/dialog.h>
#include <gui/button.h>
#include <gui/edit.h>
#include <gui/messagebox.h>

#include <stdlib.h>
#include <stdio.h>
#include <os/syscall.h>
#include <os/rtl.h>

class TestDialog : public os::Dialog
{
public:
    os::Button m_button;
    os::Edit m_edit;

    TestDialog();
    ~TestDialog();
    void OnCommand(unsigned id);
};

TestDialog::TestDialog() : 
    os::Dialog(L"A Dialog", MGLrect(100, 100, 400, 400)),
    m_button(this, L"Button", MGLrect(150, 150, 350, 200), 1),
    m_edit(this, L"Edit", MGLrect(150, 225, 350, 275), 1)
{
    __asm__("int3");
}

TestDialog::~TestDialog()
{
    PostMessage(MSG_QUIT);
}

void TestDialog::OnCommand(unsigned id)
{
    PostMessage(MSG_CLOSE);
}

class TestApplication : public os::Application
{
public:
    TestDialog *m_dialog;
    
    TestApplication(const wchar_t *name);
};

TestApplication::TestApplication(const wchar_t *name) : Application(name)
{
    m_dialog = new TestDialog;
}

int main()
{
    TestApplication app(L"application/x-vnd.guitest");
    return app.Run();
}
