/* $Id: container.cpp,v 1.1 2002/04/03 23:28:05 pavlovskii Exp $ */

#define __THROW_BAD_ALLOC printf("out of memory\n"); exit(1)
#include <stdio.h>
#include <gui/container.h>

using namespace os;

Container::Container(Window *parent, const wchar_t *text, const MGLrect &pos) : 
    Window(parent, text, pos)
{
}

void Container::AddView(Window *view)
{
    m_views.push_back(view);
}

void Container::RemoveView(Window *view)
{
    m_views.remove(view);
}
