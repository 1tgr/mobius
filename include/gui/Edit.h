#ifndef __GUI_EDIT_H
#define __GUI_EDIT_H

#include "Control.h"

namespace OS
{

//! Edit control
class GUI_EXPORT Edit : public Control
{
protected:
	void OnPaint();
	void OnKeyDown(uint32_t key);
	void OnMouseDown(point_t pt);
};

};

#endif
