/* $Id: application.cpp,v 1.3 2002/12/18 23:16:56 pavlovskii Exp $ */

#include <gui/application.h>
#include <gui/window.h>
#include <gui/messagebox.h>

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <os/rtl.h>
#include <os/syscall.h>

using namespace os;

extern "C"
bool DbgLookupLineNumber(addr_t addr, char **path, char **file, unsigned *line);

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

Application::Application(const wchar_t *name) : m_rc(SYS_DEVICES L"/Classes/video0")
{
    thread_info_t *thread;
    g_theApp = this;
    IpcConnect(SYS_PORTS L"/winmgr");
    thread = ThrGetThreadInfo();
    //thread->msgqueue_event = EvtCreate();
    thread->exception_handler = ExceptionHandler;
    //m_rc = mglCreateRc(NULL);
}

Application::~Application()
{
    //mglDeleteRc(m_rc);
    g_theApp = NULL;
}

Application *Application::GetApplication()
{
    return g_theApp;
}

extern "C" handle_t ipc_pipe;

bool Application::GetMessage(msg_t *msg)
{
    /*do
    {
        ThrWaitHandle(ThrGetThreadInfo()->msgqueue_event);
    } while (!WndGetMessage(msg));*/

    ipc_packet_t *packet;

    msg->code = 0;
    while ((packet = IpcReceivePacket(ipc_pipe)) != NULL)
    {
        if (packet->code == (uint32_t) ~SYS_WndGetMessage)
        {
            _wdprintf(L"Application::GetMessage: got message\n");
            return true;
        }

        free(packet);
    }

    if (msg->code == MSG_QUIT)
        __asm__("int3");
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
    __asm__("int3");
    return 0;
}

void Application::ExceptionHandler()
{
    static bool nested;
    bool handled;
    context_t *ctx;

    if (nested)
        ProcExitProcess(0);

    nested = true;
    ctx = &ThrGetThreadInfo()->exception_info;
    handled = Application::GetApplication()->HandleException(ctx);
    nested = false;

    if (handled)
        __asm__("mov %%ebp,%%esp\n"
            "pop %%ebp\n"
            "jmp *%0" : : "m" (ctx->eip));
    else
        exit(0);
}

bool Application::HandleException(context_t *ctx)
{
    wchar_t str[256], *ch;
    char *path, *file;
    unsigned line;

    ch = str;
    ch += swprintf(ch, L"Exception %ld at %08lx\n",
        ctx->intr, ctx->eip);

    if (DbgLookupLineNumber(ctx->eip, &path, &file, &line))
        ch += swprintf(ch, L"%S%S(%hu)\n", path, file, line);

    MessageBox box(ProcGetProcessInfo()->module_first->name, str);
    box.DoModal();

    return ctx->intr == 3;
}
