#ifndef __GUI_CONTROL_H
#define __GUI_CONTROL_H

#include "Window.h"

namespace OS
{

//! Base class for controls
class GUI_EXPORT Control : public WindowImpl
{
};

enum StandardColour
{
	ColourView,
	ColourViewText,
	ColourDialog,
	ColourDialogText,
	ColourMenu,
	ColourMenuText,
	ColourHighlight,
	ColourButton1,
	ColourButton2,
	ColourButton3,
	ColourButton4,
	ColourButton5,
};

//! Retrieves the value of a standard colour
colour_t GUI_EXPORT GetColour(StandardColour c);

}

#endif
