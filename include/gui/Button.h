#ifndef __GUI_BUTTON_H
#define __GUI_BUTTON_H

#include "Control.h"

namespace OS
{

//! Button control
class GUI_EXPORT Button : public Control
{
protected:
	bool m_is_down;
	bool m_is_mouse_down;

	void OnPaint();
	void OnMouseDown(point_t pt);
	void OnMouseMove(point_t pt);
	void OnMouseUp(point_t pt);

public:
	Button();
};

};

#endif
