/* $Id: view.cpp,v 1.2 2002/04/11 00:31:01 pavlovskii Exp $ */

#include <gui/view.h>
#include <gui/container.h>
#include <stdlib.h>

using namespace os;

View::View(Container *parent, Alignment align, MGLreal size) : 
    Window(parent, NULL, MGLrect(0, 0, 0, 0))
{
    parent->AddView(this, align, size);
}

View::~View()
{
    static_cast<Container*>(m_parent)->RemoveView(this);
}
