/* $Id: container.h,v 1.1 2002/04/03 23:29:31 pavlovskii Exp $ */

#ifndef __GUI_CONTAINER_H
#define __GUI_CONTAINER_H

#include <gui/window.h>
#include <list>

namespace os
{
    class View;

    //! Container class
    /*! \ingroup    gui */
    class Container : public Window
    {
    protected:
        std::list<Window*> m_views;

    public:
        Container(Window *parent, const wchar_t *text, const MGLrect &pos);
        void AddView(Window *view);
        void RemoveView(Window *view);
    };
};

#endif
