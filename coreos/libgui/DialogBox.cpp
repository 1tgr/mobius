// $Id$
#include <gui/DialogBox.h>
#include <gui/Control.h>
#include <gui/Graphics.h>

using namespace OS;

DialogContainer::DialogContainer() : LayoutRow(L"")
{
}


void DialogContainer::OnPaint()
{
	Painter p(this);

	p.SetFillColour(GetColour(ColourDialog));
	p.FillRect(p.m_invalid_rect);
}


void DialogContainer::UpdateLayout()
{
	LayoutContext ctx;
	ctx.rect = m_position;
	ctx.rect.left += 5;
	ctx.rect.top += 5;
	DoLayout(ctx);
}


DialogBox::DialogBox()
{
}


bool DialogBox::OnCreate()
{
	return m_container.Create(this, NULL, GetClientArea());
}


void DialogBox::OnClose(wnd_h handle)
{
	delete this;
}
