/* $Id: control.h,v 1.1 2002/04/03 23:29:31 pavlovskii Exp $ */

#ifndef __GUI_CONTROL_H
#define __GUI_CONTROL_H

#include <gui/window.h>

namespace os
{
    //! Control class
    /*! \ingroup    gui */
    class Control : public Window
    {
    protected:
        unsigned m_id;

    public:
        Control(Window *parent, const wchar_t *text, const MGLrect &pos, unsigned id);
        void OnKeyDown(uint32_t key);
        void OnFocus();
        void OnBlur();
    };
};

#endif
