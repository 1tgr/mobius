// $Id$
#include <gui/Window.h>
#include <gui/Graphics.h>
#include <gui/FrameWindow.h>
#include <gui/DialogBox.h>
#include <gui/Label.h>
#include <gui/Edit.h>
#include <gui/Button.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <stdlib.h>

class MainView : public OS::WindowImpl
{
protected:
	void OnPaint()
	{
		OS::Painter p(this);
		p.SetFillColour(MAKE_COLOUR(rand() % 255, rand() % 255, rand() % 255));
		p.FillRect(m_position);
	}

	void OnMouseMove(point_t pt)
	{
		Invalidate(m_position);
	}
};

class CustomDialogBox : public OS::DialogBox
{
protected:
	OS::Label m_label;
	OS::Button m_button_browse, m_button_run, m_button_cancel;
	OS::Edit m_edit;

	bool OnCreate()
	{
		OS::LayoutColumn *column;
		OS::LayoutRow *row;

		if (!OS::DialogBox::OnCreate())
			return false;

		m_label.Create(&m_container, 
			L"Program:",
			OS::Label::AlignLeft | OS::Label::AlignMiddle);
		m_edit.Create(&m_container, L"");
		m_button_browse.Create(&m_container, L"Browse");
		m_button_run.Create(&m_container, L"Run");
		m_button_cancel.Create(&m_container, L"Cancel");

		column = new OS::LayoutColumn(L"");
			row = new OS::LayoutRow(L"30,70");
				row->AddLayout(new OS::LayoutControl(&m_label));
				row->AddLayout(new OS::LayoutControl(&m_edit));
			column->AddLayout(row);

			row = new OS::LayoutRow(L"60,40");
				row->AddLayout(NULL);
				row->AddLayout(new OS::LayoutControl(&m_button_browse));
			column->AddLayout(row);

			row = new OS::LayoutRow(L"20,40,40");
				row->AddLayout(NULL);
				row->AddLayout(new OS::LayoutControl(&m_button_run));
				row->AddLayout(new OS::LayoutControl(&m_button_cancel));
			column->AddLayout(row);
		m_container.AddLayout(column);

		m_container.UpdateLayout();
		return true;
	}
};

class MainWindow : public OS::FrameWindow
{
protected:
	MainView m_view;

	bool OnCreate()
	{
		return m_view.Create(this, NULL, GetClientArea());
	}

public:
	MainWindow()
	{
	}

	void OnKeyDown(uint32_t key)
	{
		if (key == 27)
			OS::WndPostQuitMessage(0);
		else
		{
			OS::WindowImpl *dlg;
			dlg = new CustomDialogBox;
			dlg->Create(0, L"Dialog Box", OS::Rect(75, 150, 395, 275));
		}
	}
};


int main()
{
	OS::wnd_message_t msg;
	OS::Rect pos(100, 100, 500, 350);
	MainWindow wnd;

	wnd.Create(0, L"Hello, world!", pos);

	while (OS::WndGetMessage(&msg) &&
		msg.code != MSG_QUIT)
		OS::MessageHandler::HandleMessage(&msg);

	wnd.Close();
	return 0;
}
