/* $Id: container.cpp,v 1.2 2002/04/10 12:27:44 pavlovskii Exp $ */

#define __THROW_BAD_ALLOC printf("out of memory\n"); exit(1)
#include <stdio.h>
#include <gui/container.h>

using namespace os;

Container::Container()
{
}

Container::Container(Window *parent, const wchar_t *text, const MGLrect &pos) : 
    Window(parent, text, pos)
{
}

void Container::AddView(Window *view)
{
    m_views.push_back(view);
    RecalcLayout();
}

void Container::RemoveView(Window *view)
{
    m_views.remove(view);
    RecalcLayout();
}

void Container::RecalcLayout()
{
    std::list<Window*>::iterator it;
    int width, i;
    MGLrect pos;

    GetPosition(&pos);
    width = pos.Width() / m_views.size();
    for (it = m_views.begin(), i = 0; it != m_views.end(); ++it, i++)
    {
        pos.left = i * width;
        pos.right = (i + 1) * width;
        (*it)->SetPosition(pos);
    }
}
