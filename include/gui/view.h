/* $Id: view.h,v 1.1 2002/04/03 23:29:31 pavlovskii Exp $ */

#ifndef __GUI_VIEW_H
#define __GUI_VIEW_H

#include <gui/window.h>

namespace os
{
    class Container;

    //! View class
    /*! \ingroup    gui */
    class View : public Window
    {
    public:
        View(Container *parent);
        ~View();
    };
};

#endif
