/* $Id: edit.h,v 1.3 2002/08/04 17:22:39 pavlovskii Exp $ */

#ifndef __GUI_EDIT_H
#define __GUI_EDIT_H

#include <gui/control.h>

namespace os
{
    //! Edit control
    /*! \ingroup    gui */
    class Edit : public Control
    {
    public:
        Edit(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id);
        void OnPaint(mgl::Rc *rc);
        void OnKeyDown(uint32_t key);
        void OnMouseDown(uint32_t button, MGLreal x, MGLreal y);
    };
};

#endif