// $Id$
#include <gui/DialogBox.h>
#include <gui/Button.h>
#include <gui/Label.h>
#include <os/defs.h>
#include <os/syscall.h>
#include <stdlib.h>


class MouseChooseDialog : public OS::DialogBox
{
protected:
	OS::Button m_buttons[4];
	OS::Label m_label;
	wchar_t m_names[4][20];
	unsigned m_num_buttons;

	bool OnCreate()
	{
		static const wchar_t mouse_prefix[] = L"mouse";
		handle_t dir;
		dirent_t di;
		wchar_t name[30];
		OS::LayoutColumn *column;

		if (!OS::DialogBox::OnCreate())
			return false;

		column = new OS::LayoutColumn(L"20");
		column->AddLayout(new OS::LayoutControl(&m_label));

		OS::Rect rect = GetClientArea();
		rect.Inflate(-5, -5);
		rect.bottom = rect.top + rect.Height() / _countof(m_buttons);

		m_num_buttons = 0;
		dir = FsOpenDir(SYS_DEVICES L"/Classes");
		while (FsReadDir(dir, &di, sizeof(di)) &&
			m_num_buttons < _countof(m_buttons))
		{
			if (wcslen(di.name) >= (_countof(mouse_prefix) - 1) &&
				_wcsnicmp(di.name, mouse_prefix, _countof(mouse_prefix) - 1) == 0)
			{
				swprintf(name, L"Press %c: %s", m_num_buttons + 'A', di.name);
				wcscpy(m_names[m_num_buttons], di.name);
				//m_buttons[m_num_buttons].Create(&m_container, name, rect);

				m_buttons[m_num_buttons].Create(&m_container, name, OS::Rect(0, 0, 0, 0));
				column->AddLayout(new OS::LayoutControl(m_buttons + m_num_buttons));

				rect.Offset(0, rect.Height());
				m_num_buttons++;
			}
		}

		HndClose(dir);

		if (m_num_buttons == 0)
		{
			m_label.Create(&m_container,
				L"No mice found\n"
				L"Check " SYS_BOOT L"/system.pro\n"
				L"\n"
				L"Press Esc to continue",
				OS::Label::AlignLeft | OS::Label::AlignTop);
		}
		else
		{
			m_label.Create(&m_container,
				L"Choose the input device to\n"
				L"use with The Möbius:",
				OS::Label::AlignLeft | OS::Label::AlignTop);
		}

		m_container.AddLayout(column);
		m_container.UpdateLayout();

		if (m_num_buttons == 1)
			OnKeyDown('A');

		return true;
	}

	void OnKeyDown(uint32_t key)
	{
		wchar_t name[MAX_PATH];
		unsigned index;

		if (key == 27)
			m_result = -1;
		else if (key < 0x10000)
		{
			key = towupper((wchar_t) key);

			if (key >= 'A' && key < 'A' + m_num_buttons)
			{
				index = key - 'A';
				wcscpy(name, SYS_DEVICES L"/Classes/");
				wcscat(name, m_names[index]);
				_wdprintf(L"key = %u (%c), device = %s\n", key, key, name);
				OS::WndAttachInputDevice(0, name);
				m_result = index + 1;
			}
		}
	}

	void OnClose(OS::wnd_h handle)
	{
		m_result = -1;
	}

public:
	int m_result;

	MouseChooseDialog()
	{
		m_result = 0;
	}
};


int SetupThread(void *param)
{
	MouseChooseDialog *dlg;
	OS::wnd_message_t msg;

	dlg = new MouseChooseDialog;
	dlg->Create(0, L"Input Device", OS::Rect(75, 150, 395, 400));

	while (dlg->m_result == 0 &&
		OS::WndGetMessage(&msg))
		OS::MessageHandler::HandleMessage(&msg);

	dlg->Close();
	delete dlg;

	return 0;
}
