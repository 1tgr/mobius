#include <stdlib.h>
#include <os/rtl.h>
#include <os/syscall.h>
#include <os/port.h>
#include <os/device.h>
#include <errno.h>
#include <string.h>

#include <gui/Window.h>
#include <gui/FrameWindow.h>
#include <gui/Graphics.h>

#include "console.h"
#include "multibyte.h"

using namespace OS;

class ConsoleFrame : public FrameWindow
{
protected:
    Console m_console;
    handle_t m_client;
    static unsigned s_num_clients;

	enum
	{
		NEW_STRING = 0x00010001
	};

    static int Thread(void *param)
    {
        return ((ConsoleFrame*) param)->Thread();
    }

    int Thread();

    bool OnCreate()
    {
        return m_console.Create(this, NULL, GetClientArea());
    }

    void OnClose(wnd_h wnd)
    {
        delete this;
    }

	void DoHandleMessage(const wnd_message_t *msg);

public:
	ConsoleFrame(handle_t client) : m_console(client)
    {
        m_client = client;
		ThrCreateThread(Thread, this, 16, L"ConsoleFrame::Thread");
    }

	~ConsoleFrame()
	{
		HndClose(m_client);
	}

	bool Create();
};


unsigned ConsoleFrame::s_num_clients;


bool ConsoleFrame::Create()
{
	point_t size;

	WndGetScreenSize(&size);
	if (!WindowImpl::Create(NULL, 
		L"Console", 
		Rect(s_num_clients * 50, s_num_clients * 50, 
			s_num_clients * 50 + (size.x * 3) / 4, s_num_clients * 50 + (size.y * 3) / 4)))
        return false;

    s_num_clients++;
	return true;
}


int ConsoleFrame::Thread()
{
	char buf[160];
	wchar_t wbuf[80], *str;
	con_mbstate_t state;
	size_t bytes, length;
	wnd_message_t msg;

	ConInitMbState(&state);
    while (FsRead(m_client, buf, 0, sizeof(buf), &bytes))
    {
		length = ConMultiByteToWideChar(wbuf, buf, bytes, &state);
		if (length > 0)
		{
			str = (wchar_t*) malloc(length * sizeof(wchar_t));
			memcpy(str, wbuf, length * sizeof(wchar_t));

			msg.wnd = m_handle;
			msg.code = NEW_STRING;
			msg.params.generic[0] = (uint32_t) str;
			msg.params.generic[1] = length;
			WndPostMessage(&msg);
		}
    }

	return 0;
}


void ConsoleFrame::DoHandleMessage(const wnd_message_t *msg)
{
	wchar_t *str;
	size_t length;

	switch (msg->code)
	{
	case NEW_STRING:
		str = (wchar_t*) msg->params.generic[0];
		length = msg->params.generic[1];
		m_console.WriteString(str, length);
		free(str);
		break;

	default:
		WindowImpl::DoHandleMessage(msg);
		break;
	}
}


class AcceptorWindow : public WindowImpl
{
protected:
	handle_t m_port; 

	enum
	{
		NEW_CLIENT = 0x00010000
	};

	static int Thread(void *param)
	{
		return ((AcceptorWindow*) param)->Thread();
	}

	int Thread()
	{
		handle_t client;
		wnd_message_t msg;

		while ((client = PortAccept(m_port, FILE_READ | FILE_WRITE)) != 0)
		{
			msg.wnd = m_handle;
			msg.code = NEW_CLIENT;
			msg.params.generic[0] = client;
			WndPostMessage(&msg);
		}

		return errno;
	}

	bool OnCreate()
	{
		handle_t thread;
		thread = ThrCreateThread(Thread, this, 16, L"AcceptorWindow::Thread");
		HndClose(thread);
		return true;
	}

	void DoHandleMessage(const wnd_message_t *msg)
	{
		ConsoleFrame *frame;

		switch (msg->code)
		{
		case NEW_CLIENT:
			//_wdprintf(L"AcceptorWindow: got new client %u\n", msg->params.generic[0]);
			frame = new ConsoleFrame(msg->params.generic[0]);
			if (!frame->Create())
				delete frame;
			break;

		default:	
			WindowImpl::DoHandleMessage(msg);
			break;
		}
	}

public:
	AcceptorWindow(handle_t port)
	{
		m_port = port;
	}
};


int main(void)
{
    wnd_message_t msg;
    const wchar_t *shell_name = L"/mobius/shell.exe",
        *server_name = SYS_PORTS L"/console";
    handle_t client, server;
    unsigned i;
    process_info_t defaults = { 0 };

    server = FsCreate(server_name, 0);
    i = 0;
    for (i = 0; i < 1; i++)
    {
        client = FsOpen(server_name, FILE_READ | FILE_WRITE);
        defaults.std_in = defaults.std_out = client;
        wcscpy(defaults.cwd, L"/");
        if (ProcSpawnProcess(shell_name, &defaults) == NULL)
        {
            char *str;
            str = strerror(errno);
            FsWrite(client, str, 0, strlen(str), NULL);
        }
    }

    AcceptorWindow acceptor(server);
	acceptor.Create(NULL, NULL);

	while (WndGetMessage(&msg))
		MessageHandler::HandleMessage(&msg);

    return 0;
}
