/* $Id: dialog.h,v 1.1 2002/04/03 23:29:31 pavlovskii Exp $ */

#ifndef __GUI_DIALOG_H
#define __GUI_DIALOG_H

#include <gui/frame.h>

namespace os
{
    //! Dialog class
    /*! \ingroup    gui */
    class Dialog : public Frame
    {
    protected:
        unsigned m_modalResult;

    public:
        Dialog(const wchar_t *text, const MGLrect &pos);
        void OnPaint();
        void OnCommand(unsigned id);

        //! Shows the dialog modally
        /*!
         *  \p DoModal runs its own message loop which terminates when the user
         *  clicks a button in the dialog.
         *  \return The ID of the button which ended the dialog.
         */
        unsigned DoModal();
    };
};

#endif