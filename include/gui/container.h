/* $Id: container.h,v 1.3 2002/04/20 12:34:38 pavlovskii Exp $ */

#ifndef __GUI_CONTAINER_H
#define __GUI_CONTAINER_H

#include <gui/window.h>
#include <list>

namespace os
{
    enum Alignment
    {
        alLeft,
        alTop,
        alRight,
        alBottom,
        alClient,
    };

    //! Container class
    /*! \ingroup    gui */
    class Container : public Window
    {
    protected:
        struct ViewTuple
        {
            Window *window;
            Alignment align;
            MGLreal size;

            ViewTuple(Window *_window, Alignment _align, MGLreal _size)
            {
                window = _window;
                align = _align;
                size = _size;
            }
        };

        std::list<ViewTuple> m_views;
        MGLrect m_margins;

    public:
        Container();
        Container(Window *parent, const wchar_t *text, const MGLrect &pos);
        void AddView(Window *view, Alignment align, MGLreal size);
        void RemoveView(Window *view);
        void RecalcLayout();
    };
};

#endif
