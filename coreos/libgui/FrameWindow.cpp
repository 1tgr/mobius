// $Id$

#include <gui/FrameWindow.h>
#include <gui/Graphics.h>
#include <gui/Control.h>

using namespace OS;

FrameWindow::FrameWindow()
{
}

void FrameWindow::OnPaint()
{
	Painter p(this);
	Rect rect = m_position;
	Point size;
	int x;

	p.SetFillColour(0x000000);
	p.SetPenColour(0xffffff);

	size = p.GetTextSize(m_title);
	x = (m_position.left + m_position.right - size.x) / 2;
	rect = Rect(x, m_position.top + 2, x + size.x, m_position.top + size.y + 2);
	p.DrawText(rect, m_title);

	// Title bar LHS
	rect.left = m_position.left;
	rect.right = x;
	p.FillRect(rect);

	// Title bar RHS
	rect.left = x + size.x;
	rect.right = m_position.right;
	p.FillRect(rect);

	// Title bar top
	rect.left = m_position.left;
	rect.top = m_position.top;
	rect.right = m_position.right;
	rect.bottom = rect.top + 2;
	p.FillRect(rect);

	// Title bar bottom
	rect.top = m_position.top + size.y + 2;
	rect.bottom = rect.top + 1;
	p.FillRect(rect);

	// Left border
	rect.left = m_position.left;
	rect.top = rect.bottom;
	rect.right = rect.left + 4;
	rect.bottom = m_position.bottom;
	p.FillRect(rect);

	// Right border
	rect.left = m_position.right - 4;
	rect.right = m_position.right;
	p.FillRect(rect);

	// Bottom border
	rect.left = m_position.left + 4;
	rect.top = m_position.bottom - 4;
	rect.right = m_position.right - 4;
	rect.bottom = m_position.bottom;
	p.FillRect(rect);

	p.SetPenColour(GetColour(ColourDialog));
	rect.left = m_position.left + 4;
	rect.top = m_position.top + size.y + 3;
	rect.right = m_position.right - 4;
	rect.bottom = m_position.bottom - 4;
	p.Rectangle(rect);
}

rect_t FrameWindow::GetClientArea()
{
	Rect ret = m_position;
	Painter p(this);

	ret.top += p.GetTextSize(m_title).y + 4;
	ret.left += 5;
	ret.right -= 5;
	ret.bottom -= 5;

	return ret;
}
