/* $Id: container.cpp,v 1.3 2002/04/11 00:31:01 pavlovskii Exp $ */

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

void Container::AddView(Window *view, Alignment align, MGLreal size)
{
    m_views.push_back(ViewTuple(view, align, size));
    RecalcLayout();
}

void Container::RemoveView(Window *view)
{
    std::list<ViewTuple>::iterator it;

    for (it = m_views.begin(); it != m_views.end(); ++it)
        if (it->window == view)
            it = m_views.erase(it);

    RecalcLayout();
}

void Container::RecalcLayout()
{
    std::list<ViewTuple>::iterator it;
    MGLrect pos, child;

    GetPosition(&pos);
    for (it = m_views.begin(); it != m_views.end(); ++it)
    {
        switch (it->align)
        {
        case alLeft:
            child = MGLrect(pos.left, pos.top, pos.left + it->size, pos.bottom);
            pos.left += it->size;
            break;

        case alTop:
            child = MGLrect(pos.left, pos.top, pos.right, pos.top + it->size);
            pos.top += it->size;
            break;

        case alRight:
            child = MGLrect(pos.right - it->size, pos.top, pos.right, pos.bottom);
            pos.right -= it->size;
            break;

        case alBottom:
            child = MGLrect(pos.left, pos.bottom - it->size, pos.right, pos.bottom);
            pos.bottom -= it->size;
            break;

        case alClient:
            continue;
        }

        it->window->SetPosition(child);
    }

    /* Multiple alClient views probably won't work very well */
    for (it = m_views.begin(); it != m_views.end(); ++it)
        if (it->align == alClient)
            it->window->SetPosition(pos);
}
