// $Id$
#ifndef __GUI_MENU_H
#define __GUI_MENU_H

#include "Window.h"
#include <os/List.h>

namespace OS
{

struct MenuItem
{
	wchar_t *text;
};


//! Pop-up menu
class GUI_EXPORT Menu : public WindowImpl
{
protected:
	List<MenuItem> m_items;
	int m_result;
	int m_selected_item;

	void OnPaint();
	void OnMouseMove(point_t pt);
	void OnMouseUp(point_t pt);
	int PointToItem(const point_t& pt);

public:
	bool Create();
	int Show(const point_t& pt);
	MenuItem *AddItem(const wchar_t *text);
};

};

#endif
