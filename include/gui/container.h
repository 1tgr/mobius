/* $Id: container.h,v 1.2 2002/04/10 12:32:37 pavlovskii Exp $ */

#ifndef __GUI_CONTAINER_H
#define __GUI_CONTAINER_H

#include <gui/window.h>
#include <list>

namespace os
{
    //! Container class
    /*! \ingroup    gui */
    class Container : public Window
    {
    protected:
        std::list<Window*> m_views;

    public:
        Container();
        Container(Window *parent, const wchar_t *text, const MGLrect &pos);
        void AddView(Window *view);
        void RemoveView(Window *view);
        void RecalcLayout();
    };
};

#endif
