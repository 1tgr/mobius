// $Id$
#include <gui/Label.h>
#include <gui/Graphics.h>

using namespace OS;

Label::Label()
{
	m_flags = AlignLeft | AlignTop;
}


bool Label::Create(WindowImpl *parent, 
				   const wchar_t *title, 
				   unsigned flags,
				   const rect_t& position)
{
	m_flags = flags;
	return WindowImpl::Create(parent, title, position);
}


void Label::OnPaint()
{
	Painter p(this);
	Point size, org;
	Rect rect = m_position;

	p.SetFillColour(GetColour(ColourDialog));
	p.SetPenColour(GetColour(ColourDialogText));

	size = p.GetTextSize(m_title);

	switch (m_flags & MaskAlignHorz)
	{
	case AlignLeft:
	default:
		org.x = 0;
		break;

	case AlignRight:
		org.x = m_position.Width() - size.x;
		break;

	case AlignCentre:
		org.x = (m_position.Width() - size.x) / 2;
		break;
	}

	switch (m_flags & MaskAlignVert)
	{
	case AlignTop:
	default:
		org.y = 0;
		break;

	case AlignBottom:
		org.y = m_position.Height() - size.y;
		break;

	case AlignMiddle:
		org.y = (m_position.Height() - size.y) / 2;
		break;
	}

	p.FillRect(m_position);
	rect.Offset(org.x, org.y);
	p.DrawText(rect, m_title);
}
