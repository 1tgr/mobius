// $Id$
#include <gui/Window.h>
#include <gui/Graphics.h>
#include <gui/Menu.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <stdlib.h>

using namespace OS;

class Desktop : public WindowImpl
{
protected:
	void OnPaint()
	{
		Painter p(this);
        p.SetFillColour(0x3822C2);
		p.FillRect(m_position);
	}

	void OnMouseDown(point_t pos)
	{
		Menu menu;
		handle_t c;

		menu.AddItem(L"Run wincli.exe");
		menu.AddItem(L"Run gconsole.exe");
		
		switch (menu.Show(Point(pos.x + 1, pos.y + 1)))
		{
		case 0:
			c = ProcSpawnProcess(L"/mobius/wincli.exe", ProcGetProcessInfo());
			HndClose(c);
			break;

		case 1:
			c = ProcSpawnProcess(L"/mobius/gconsole.exe", ProcGetProcessInfo());
			HndClose(c);
			break;
		}
	}

public:
	wnd_h OwnRoot()
	{
		wnd_attr_t attribs[1];
		wnd_data_t data;

		attribs[0].code = WND_ATTR_CLIENT_DATA;
		attribs[0].data.client_data = this;
		m_handle = WndOwnRoot(attribs, _countof(attribs));
		if (GetAttribute(WND_ATTR_POSITION, &data))
			m_position = data.position;
		return m_handle;
	}
};


int SetupThread(void *);

int main(void)
{
    wnd_message_t msg;
    Desktop desktop;
    handle_t c;

    desktop.OwnRoot();

	//c = ThrCreateThread(SetupThread, NULL, 16, L"SetupThread");
	//HndClose(c);

	OS::WndAttachInputDevice(0, SYS_DEVICES L"/ps2mouse");

    while (OS::WndGetMessage(&msg))
		OS::MessageHandler::HandleMessage(&msg);

    desktop.Close();
    return 0;
}
