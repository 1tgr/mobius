// $Id$
#include <gui/DialogLayout.h>

using namespace OS;

LayoutContainer::~LayoutContainer()
{
	List<LayoutElement*>::Iterator it;

	for (it = m_children.Begin(); it != m_children.End(); ++it)
		if (*it != NULL)
			delete *it;
}


void LayoutContainer::AddLayout(LayoutElement *child)
{
	m_children.Add(child);
}


LayoutControl::LayoutControl(WindowImpl *wnd)
{
	m_wnd = wnd;
}


void LayoutControl::DoLayout(const LayoutContext& ctx)
{
	if (m_wnd != NULL)
	{
		Rect rect = ctx.rect;
		rect.right -= 5;
		rect.bottom -= 5;
		m_wnd->SetPosition(rect);
	}
}


static void parse_numbers(Vector<int>& vec, const wchar_t *str)
{
	wchar_t *temp, *width, *ch;

	temp = _wcsdup(str);

	width = ch = temp;
	while (*ch != '\0')
	{
		if (*ch == ',')
		{
			*ch = '\0';
			vec.Add(wcstoul(width, NULL, 0));
			width = ch + 1;
		}

		ch++;
	}

	if (*width != '\0')
		vec.Add(wcstoul(width, NULL, 0));

	free(temp);
}


LayoutRow::LayoutRow(const wchar_t *widths)
{
	parse_numbers(m_widths, widths);
}


void LayoutRow::DoLayout(const LayoutContext& ctx)
{
	List<LayoutElement*>::Iterator it;
	LayoutContext childctx;
	unsigned i, width;

	childctx = ctx;
	i = 0;
	for (it = m_children.Begin(); it != m_children.End(); ++it)
	{
		if (i < m_widths.Size())
			width = ((ctx.rect.right - ctx.rect.left) * m_widths[i]) / 100;
		else
			width = (ctx.rect.right - ctx.rect.left) / m_children.Size();

		childctx.rect.right = childctx.rect.left + width;
		if (*it != NULL)
			(*it)->DoLayout(childctx);
		childctx.rect.left = childctx.rect.right;
		i++;
	}
}


LayoutColumn::LayoutColumn(const wchar_t *heights)
{
	parse_numbers(m_heights, heights);
}


void LayoutColumn::DoLayout(const LayoutContext& ctx)
{
	List<LayoutElement*>::Iterator it;
	LayoutContext childctx;
	unsigned i, height;

	childctx = ctx;
	i = 0;
	for (it = m_children.Begin(); it != m_children.End(); ++it)
	{
		if (i < m_heights.Size())
			height = ((ctx.rect.bottom - ctx.rect.top) * m_heights[i]) / 100;
		else
			height = (ctx.rect.bottom - ctx.rect.top) / m_children.Size();

		childctx.rect.bottom = childctx.rect.top + height;
		if (*it != NULL)
			(*it)->DoLayout(childctx);
		childctx.rect.top = childctx.rect.bottom;
		i++;
	}
}
