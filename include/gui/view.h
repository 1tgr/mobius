/* $Id: view.h,v 1.2 2002/04/20 12:34:38 pavlovskii Exp $ */

#ifndef __GUI_VIEW_H
#define __GUI_VIEW_H

#include <gui/window.h>
#include <gui/container.h>

namespace os
{
    //! View class
    /*! \ingroup    gui */
    class View : public Window
    {
    public:
        View(Container *parent, Alignment align, MGLreal size);
        ~View();
    };
};

#endif
