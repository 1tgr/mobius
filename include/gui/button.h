/* $Id: button.h,v 1.2 2002/04/04 00:08:42 pavlovskii Exp $ */

#ifndef __GUI_BUTTON_H
#define __GUI_BUTTON_H

#include <gui/control.h>

namespace os
{
    //! Button control
    /*! \ingroup    gui */
    class Button : public Control
    {
    protected:
        bool m_isDown;
        bool m_mouseDown;

    public:
        Button(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id);
        void OnPaint();
        void OnKeyDown(uint32_t key);
        void OnKeyUp(uint32_t key);
        void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
        void OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y);
        void OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y);

        virtual void OnClick();
    };
};

#endif
