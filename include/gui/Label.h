#ifndef __GUI_LABEL_H
#define __GUI_LABEL_H

#include "Control.h"

namespace OS
{

//! Label control
class GUI_EXPORT Label : public Control
{
public:
	enum Flags
	{
		AlignLeft = 0x00,
		AlignCentre = 0x01,
		AlignRight = 0x02,
		MaskAlignHorz = 0x03,
		AlignTop = 0x00,
		AlignMiddle = 0x04,
		AlignBottom = 0x05,
		MaskAlignVert = 0x07,
	};

	Label();
	bool Create(WindowImpl *parent, 
		const wchar_t *title, 
		unsigned flags,
		const rect_t& position = Rect(0, 0, 0, 0));

protected:
	unsigned m_flags;

	void OnPaint();
};

};

#endif
