/* $Id: view.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

#include <gui/view.h>
#include <gui/container.h>
#include <stdlib.h>

using namespace os;

View::View(Container *parent) : 
    Window(parent, NULL, MGLrect(0, 0, 0, 0))
{
    parent->AddView(this);
}

View::~View()
{
    static_cast<Container*>(m_parent)->RemoveView(this);
}
