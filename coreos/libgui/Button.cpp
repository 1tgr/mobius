// $Id$
#include <gui/Button.h>
#include <gui/Graphics.h>

using namespace OS;

/*void Button::OnPaint()
{
	static const int BORDER_WIDTH = 2;
	static const int SHADOW_WIDTH = 2;
	Painter p(this);
	point_t org, size;
	rect_t middle;

	if (m_is_down)
	{
		p.SetFillColour(GetColour(ColourDialog));

		// Top
		p.FillRect(rect_t(m_position.left, m_position.top, 
			m_position.right, m_position.top + SHADOW_WIDTH));
		// Left
		p.FillRect(rect_t(m_position.left, m_position.top + SHADOW_WIDTH,
			m_position.left + SHADOW_WIDTH, m_position.bottom));

		p.SetFillColour(GetColour(ColourDialogText));

		// Top border
		p.FillRect(rect_t(m_position.left + SHADOW_WIDTH, m_position.top + SHADOW_WIDTH,
			m_position.right, m_position.top + SHADOW_WIDTH + BORDER_WIDTH));

		// Left border
		p.FillRect(rect_t(m_position.left + SHADOW_WIDTH, m_position.top + SHADOW_WIDTH + BORDER_WIDTH,
			m_position.left + SHADOW_WIDTH + BORDER_WIDTH, m_position.bottom));

		// Right border
		p.FillRect(rect_t(m_position.right - BORDER_WIDTH, m_position.top + SHADOW_WIDTH + BORDER_WIDTH,
			m_position.right, m_position.bottom));

		// Bottom border
		p.FillRect(rect_t(m_position.left + SHADOW_WIDTH + BORDER_WIDTH, m_position.bottom - BORDER_WIDTH,
			m_position.right - BORDER_WIDTH, m_position.bottom));

		// Middle
		middle = rect_t(m_position.left + SHADOW_WIDTH + BORDER_WIDTH, m_position.top + SHADOW_WIDTH + BORDER_WIDTH,
			m_position.right - BORDER_WIDTH, m_position.bottom - BORDER_WIDTH);
	}
	else
	{
		p.SetFillColour(GetColour(ColourDialogText));

		// Top
		p.FillRect(rect_t(m_position.left, m_position.top, 
			m_position.right - BORDER_WIDTH, m_position.top + BORDER_WIDTH));

		// Left
		p.FillRect(rect_t(m_position.left, m_position.top + BORDER_WIDTH, 
			m_position.left + BORDER_WIDTH, m_position.bottom - BORDER_WIDTH));

		// Right
		p.FillRect(rect_t(m_position.right - SHADOW_WIDTH - BORDER_WIDTH, m_position.top + BORDER_WIDTH, 
			m_position.right, m_position.bottom));

		// Bottom
		p.FillRect(rect_t(m_position.left + BORDER_WIDTH, m_position.bottom - SHADOW_WIDTH - BORDER_WIDTH, 
			m_position.right - SHADOW_WIDTH - BORDER_WIDTH, m_position.bottom));

		p.SetFillColour(GetColour(ColourDialog));

		// Top right
		p.FillRect(rect_t(m_position.right - SHADOW_WIDTH, m_position.top, 
			m_position.right, m_position.top + BORDER_WIDTH));

		// Bottom left
		p.FillRect(rect_t(m_position.left, m_position.bottom - SHADOW_WIDTH,
			m_position.left + BORDER_WIDTH, m_position.bottom));

		// Middle
		middle = rect_t(m_position.left + BORDER_WIDTH, m_position.top + BORDER_WIDTH, 
			m_position.right - SHADOW_WIDTH - BORDER_WIDTH, m_position.bottom - SHADOW_WIDTH - BORDER_WIDTH);
	}

	p.SetFillColour(8);
	p.FillRect(middle);

	p.SetPenColour(GetColour(ColourDialogText));
	size = p.GetTextSize(m_title);
	org.x = (middle.left + middle.right - size.x) / 2;
	org.y = (middle.top + middle.bottom - size.y) / 2;
	p.DrawText(rect_t(org.x, org.y, org.x + size.x, org.y + size.y), m_title);
}*/


static void draw_3d_rect(Graphics *g, const rect_t& rect, colour_t top_left, colour_t bottom_right)
{
	g->SetPenColour(bottom_right);
	g->Line(rect.right - 1, rect.top + 1, rect.right - 1, rect.bottom - 1);
	g->Line(rect.left, rect.bottom - 1, rect.right, rect.bottom - 1);

	g->SetPenColour(top_left);
	g->Line(rect.left, rect.top, rect.right, rect.top);
	g->Line(rect.left, rect.top, rect.left, rect.bottom - 1);
}

/*
 *    1--------1--------+
 *    | 2------2------+ |
 *    | |             | |
 *    2 2      3      4 5
 *    | |             | |
 *    | +------4------4 |
 *    +--------5--------5
 */

void Button::OnPaint()
{
	static const StandardColour colours[5][2] =
	{
		{  ColourButton1, ColourButton5 },		// outer top/left
		{  ColourButton2, ColourButton4 },		// inner top/left
		{  ColourButton3, ColourHighlight },	// middle
		{  ColourButton4, ColourButton2 },		// inner bottom/right
		{  ColourButton5, ColourButton1 },		// outer bottom/right
	};

	Painter p(this);
	Rect rect;
	point_t size, org;

	rect = m_position;
	draw_3d_rect(&p, 
		rect, 
		GetColour(colours[0][m_is_down]), 
		GetColour(colours[4][m_is_down]));

	rect.Inflate(-1, -1);
	draw_3d_rect(&p, 
		rect, 
		GetColour(colours[1][m_is_down]), 
		GetColour(colours[3][m_is_down]));

	rect.Inflate(-1, -1);
	p.SetFillColour(GetColour(colours[2][m_is_down]));
	p.FillRect(rect);

	p.SetPenColour(GetColour(ColourDialogText));
	size = p.GetTextSize(m_title);
	org.x = (rect.left + rect.right - size.x) / 2;
	org.y = (rect.top + rect.bottom - size.y) / 2;

	if (m_is_down)
	{
		org.x += 2;
		org.y += 2;
	}

	p.DrawText(Rect(org.x, org.y, org.x + size.x, org.y + size.y), m_title);
}


void Button::OnMouseDown(point_t pt)
{
	m_is_mouse_down = m_is_down = true;
	SetCapture(true);
	Invalidate(m_position);
}


void Button::OnMouseMove(point_t pt)
{
	if (m_is_mouse_down)
	{
		bool was_down;

		was_down = m_is_down;
		m_is_down = m_position.IncludesPoint(pt.x, pt.y);
		if (was_down != m_is_down)
			Invalidate(m_position);
	}
}


void Button::OnMouseUp(point_t pt)
{
	m_is_mouse_down = m_is_down = false;
	SetCapture(false);
	Invalidate(m_position);
}


Button::Button()
{
	m_is_down = m_is_mouse_down = false;
}
