#ifndef __GUI_DIALOGLAYOUT_H
#define __GUI_DIALOGLAYOUT_H

#include "Window.h"
#include <os/List.h>
#include <os/Vector.h>

namespace OS
{

struct LayoutContext
{
	rect_t rect;
};


class LayoutElement
{
public:
	virtual ~LayoutElement() { }
	virtual void DoLayout(const LayoutContext& ctx) = 0;
};


class GUI_EXPORT LayoutContainer : public LayoutElement
{
protected:
	List<LayoutElement*> m_children;

public:
	~LayoutContainer();
	void AddLayout(LayoutElement *child);
};


class GUI_EXPORT LayoutControl : public LayoutElement
{
protected:
	WindowImpl *m_wnd;

public:
	LayoutControl(WindowImpl *wnd);
	void DoLayout(const LayoutContext& ctx);
};


class GUI_EXPORT LayoutRow : public LayoutContainer
{
protected:
	Vector<int> m_widths;

public:
	LayoutRow(const wchar_t *widths);
	void DoLayout(const LayoutContext& ctx);
};


class GUI_EXPORT LayoutColumn : public LayoutContainer
{
protected:
	Vector<int> m_heights;

public:
	LayoutColumn(const wchar_t *heights);
	void DoLayout(const LayoutContext& ctx);
};

};

#endif
