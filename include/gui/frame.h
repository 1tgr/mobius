/* $Id: frame.h,v 1.5 2002/08/04 17:22:39 pavlovskii Exp $ */

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
        enum FrameRegion
        {
            rgnNone,
            rgnTitle,
            rgnClient,
            rgnButton1,
            rgnButton2,
            rgnButton3,
            rgnButtonA,
            rgnButtonB,
            rgnButtonC,
            rgnSizeTop,
            rgnSizeBottom,
            rgnSizeLeft,
            rgnSizeRight,
            rgnSizeTopLeft,
            rgnSizeTopRight,
            rgnSizeBottomRight,
            rgnSizeBottomLeft,
            rgnButtonClose = rgnButton1,
        };

        FrameRegion m_mouseRegion;
        MGLpoint m_lastMovePoint;
        MGLrect m_dragRect;

    public:
        Frame(const wchar_t *text, const MGLrect &pos);
        void OnPaint(Rc *rc);
        void OnFocus();
        void OnBlur();
        void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
        void OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y);
        void OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y);
        void OnSize(const MGLrect &rect);

    protected:
        virtual FrameRegion HitTest(MGLreal x, MGLreal y);
        virtual void OnHitBegin(FrameRegion rgn, MGLreal x, MGLreal y);
        virtual void OnHitMiddle(FrameRegion rgn, MGLreal x, MGLreal y);
        virtual void OnHitEnd(FrameRegion rgn, MGLreal x, MGLreal y);
    };
};

#endif
