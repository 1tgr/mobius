/* $Id: frame.h,v 1.2 2002/04/04 00:08:42 pavlovskii Exp $ */

#ifndef __GUI_FRAME_H
#define __GUI_FRAME_H

#include <gui/container.h>

namespace os
{
    //! Frame class
    /*! \ingroup    gui */
    class Frame : public Container
    {
    protected:
        MGLrect m_margins;

    public:
        Frame(const wchar_t *text, const MGLrect &pos);
        void OnPaint();
        void OnFocus();
        void OnBlur();
        void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
    };
};

#endif