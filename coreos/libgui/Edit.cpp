// $Id$
#include <gui/Edit.h>
#include <gui/Graphics.h>
#include <stdlib.h>

using namespace OS;

void Edit::OnPaint()
{
	Painter p(this);
	Rect rect = m_position;
	Point size;

	p.SetPenColour(GetColour(ColourViewText));
	p.SetFillColour(GetColour(ColourView));
	p.FillRect(m_position);
	p.Rectangle(m_position);

	rect.Inflate(-4, -4);
	p.DrawText(rect, m_title);

	if (HasFocus())
	{
		size = p.GetTextSize(m_title);
		p.Line(rect.left + size.x, rect.top, rect.left + size.x, rect.bottom);
	}
}


void Edit::OnKeyDown(uint32_t key)
{
	key = (wchar_t) key;
	if (key != 0)
	{
		int length;
		Rect rect = m_position;

		switch (key)
		{
		case '\b':
			if (m_title[0] != '\0')
				m_title[wcslen(m_title) - 1] = '\0';
			break;

		default:
			length = wcslen(m_title);
			m_title = (wchar_t*) realloc(m_title, sizeof(wchar_t) * (length + 2));
			if (m_title == NULL)
				m_title = L"";
			else
			{
				m_title[length] = (wchar_t) key;
				m_title[length + 1] = '\0';
			}
			break;
		}

		rect.Inflate(-4, -4);
		Invalidate(m_position);
	}
}


void Edit::OnMouseDown(point_t pt)
{
	SetFocus();
}
