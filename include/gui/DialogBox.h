#ifndef __GUI_DIALOGBOX_H
#define __GUI_DIALOGBOX_H

#include "FrameWindow.h"
#include "DialogLayout.h"

namespace OS
{

//! Container for controls in a dialog box
class GUI_EXPORT DialogContainer : public WindowImpl, public LayoutRow
{
public:
	DialogContainer();
	void OnPaint();
	void UpdateLayout();
};


//! Frame for dialog box
class GUI_EXPORT DialogBox : public FrameWindow
{
protected:
	DialogContainer m_container;

	bool OnCreate();
	void OnClose(wnd_h handle);

public:
	DialogBox();
};

};

#endif
