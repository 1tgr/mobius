/* $Id: frame.h,v 1.3 2002/04/10 12:32:37 pavlovskii Exp $ */

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
        bool m_isMoving;
        MGLpoint m_lastMovePoint;
        MGLrect m_dragRect;

    public:
        Frame(const wchar_t *text, const MGLrect &pos);
        void OnPaint();
        void OnFocus();
        void OnBlur();
        void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
        void OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y);
        void OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y);

    protected:
        enum FrameRegion
        {
            rgnTitle,
            rgnClient,
        };

        virtual FrameRegion HitTest(MGLreal x, MGLreal y);
    };
};

#endif
