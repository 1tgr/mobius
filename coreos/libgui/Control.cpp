// $Id$
#include <gui/Control.h>

using namespace OS;

static colour_t colours[] =
{
	0xffffff,	// ColourView
	0x000000,	// ColourViewText
	0x808080,	// ColourDialog
	0x000000,	// ColourDialogText
	0xffffff,	// ColourMenu
	0x000000,	// ColourMenuText
	0x00ffff,	// ColourHighlight
	0x808080,	// ColourButton1
	0xffffff,	// ColourButton2
	0xc0c0c0,	// ColourButton3
	0x808080,	// ColourButton4
	0x000000,	// ColourButton5
};

colour_t OS::GetColour(StandardColour c)
{
	if (c < ColourView || c > ColourButton5)
		return 0;
	else
		return colours[c];
}
