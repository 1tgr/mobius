#ifndef __GUI_FRAMEWINDOW_H
#define __GUI_FRAMEWINDOW_H

#include "Window.h"

namespace OS
{

//! Window with a frame
class GUI_EXPORT FrameWindow : public WindowImpl
{
public:
	FrameWindow();
	void OnPaint();
	rect_t GetClientArea();
};

};

#endif
