/* $Id: application.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

#include <gui/application.h>
#include <gui/window.h>

#include <stdlib.h>

#include <os/rtl.h>
#include <os/syscall.h>

using namespace os;

/*!
 *  \defgroup   gui Graphical User Interface
 *
 *  The GUI classes implement the user-side part of the Möbius graphical user 
 *  interface; the other (smaller) part runs in kernel mode and is accessed 
 *  using the \p WndXXX syscalls. The \p Window and \p Application classes 
 *  encapsulate the user-kernel interface.
 *
 *  All of the GUI classes are accessed using the \p os C++ namespace.
 */

Application *Application::g_theApp;

Application::Application(const wchar_t *name)
{
    g_theApp = this;
    ThrGetThreadInfo()->msgqueue_event = EvtAlloc();
    m_rc = mglCreateRc(NULL);
}

Application::~Application()
{
    mglDeleteRc(m_rc);
    g_theApp = NULL;
}

Application *Application::GetApplication()
{
    return g_theApp;
}

bool Application::GetMessage(msg_t *msg)
{
    do
    {
        ThrWaitHandle(ThrGetThreadInfo()->msgqueue_event);
    } while (!WndGetMessage(msg));
    return msg->code != MSG_QUIT;
}

void Application::HandleMessage(const msg_t *msg)
{
    Window *wnd;
    size_t size;
    size = sizeof(wnd);
    wnd = NULL;
    if (WndGetAttribute(msg->window, WND_ATTR_USERDATA, WND_TYPE_VOID, &wnd, &size) &&
        wnd != NULL)
        wnd->HandleMessage(msg);
}

int Application::Run()
{
    msg_t msg;
    while (GetMessage(&msg))
        HandleMessage(&msg);
    return 0;
}
