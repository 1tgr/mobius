// $Id$
#include <gui/Menu.h>
#include <gui/Graphics.h>
#include <gui/Control.h>
#include <stdlib.h>

using namespace OS;

bool Menu::Create()
{
	return WindowImpl::Create(NULL, NULL, Rect(0, 0, 0, 0));
}


void Menu::OnPaint()
{
	Painter p(this);
	List<MenuItem>::Iterator it;
	Rect rect, rect_text;
	int item_height, i;

	p.SetPenColour(GetColour(ColourMenuText));
	p.SetFillColour(GetColour(ColourMenu));
	
	rect = m_position;
	p.Rectangle(rect);

	rect.Inflate(-1, -1);
	p.FillRect(Rect(rect.left, rect.top, rect.right, rect.top + 3));
	p.FillRect(Rect(rect.left, rect.bottom - 3, rect.right, rect.bottom));

	rect_text = rect;
	rect_text.Inflate(-3, -3);
	item_height = rect_text.Height() / m_items.Size();
	rect_text.bottom = rect_text.top + item_height;

	i = 0;
	for (it = m_items.Begin(); it != m_items.End(); ++it)
	{
		p.SetFillColour(GetColour(m_selected_item == i ? ColourHighlight : ColourMenu));
		p.FillRect(Rect(rect.left, rect_text.top, rect.right, rect_text.bottom));
		p.DrawText(rect_text, it->text);
		rect_text.Offset(0, item_height);
		i++;
	}
}


void Menu::OnMouseMove(point_t pt)
{
	int new_item;

	new_item = PointToItem(pt);
	if (new_item != m_selected_item)
	{
		m_selected_item = new_item;
		Invalidate(m_position);
	}
}


void Menu::OnMouseUp(point_t pt)
{
	m_result = m_selected_item = PointToItem(pt);
}


int Menu::PointToItem(const point_t& pt)
{
	Rect rect = m_position;

	rect.Inflate(0, -4);
	if (rect.IncludesPoint(pt.x, pt.y))
		return ((pt.y - rect.top) * m_items.Size()) / rect.Height();
	else
		return -1;
}


int Menu::Show(const point_t& pt)
{
	wnd_message_t msg;

	if (m_handle != NULL)
		Close();

	if (m_items.Size() == 0 ||
		!Create())
		return -1;

	Painter p(this);
	Point size, size_max;
	Rect rect;
	List<MenuItem>::Iterator it;

	size_max.x = 0;
	size_max.y = 0;

	for (it = m_items.Begin(); it != m_items.End(); ++it)
	{
		size = p.GetTextSize(it->text);
		if (size.x > size_max.x)
			size_max.x = size.x;
		if (size.y > size_max.y)
			size_max.y = size.y;
	}

	rect.left = pt.x;
	rect.top = pt.y;
	rect.right = pt.x + size_max.x + 8;
	rect.bottom = pt.y + size_max.y * m_items.Size() + 8;
	SetPosition(rect);

	m_result = -1;
	while (m_result == -1 &&
		WndGetMessage(&msg))
		MessageHandler::HandleMessage(&msg);

	Close();
	return m_result;
}


MenuItem *Menu::AddItem(const wchar_t *text)
{
	List<MenuItem>::Iterator it = m_items.Add(MenuItem());
	it->text = _wcsdup(text);
	return &(*it);
}
