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
    };
};

#endif