/* $Id: edit.h,v 1.2 2002/04/04 00:08:42 pavlovskii Exp $ */

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
        void OnPaint();
        void OnKeyDown(uint32_t key);
        void OnMouseDown(uint32_t button, MGLreal x, MGLreal y);
    };
};

#endif