/* $Id: messagebox.h,v 1.1 2002/04/03 23:29:31 pavlovskii Exp $ */

#ifndef __GUI_MESSAGEBOX_H
#define __GUI_MESSAGEBOX_H

#include <gui/dialog.h>

namespace os
{
    //! Message box class
    /*! \ingroup    gui */
    class MessageBox : public Dialog
    {
    public:
        enum
        {
            //! OK button (Enter)
            btnOk = 1,
            //! Cancel button (Esc)
            btnCancel = 2,
            //! Yes button (Y)
            btnYes = 4,
            //! No button (N)
            btnNo = 8,
            //! Help button (F1)
            btnHelp = 16,

            //! First custom button
            btnCustom1 = 0x10000,
            //! Second custom button
            btnCustom2 = 0x20000,
            //! Third custom button
            btnCustom3 = 0x40000,

            //! OK and Cancel buttons
            btnOkCancel = btnOk | btnCancel,
            //! Yes and No buttons
            btnYesNo = btnYes | btnNo,
            //! Yes, No and Cancel buttons
            btnYesNoCancel = btnYes | btnNo | btnCancel,
        };

        //! Constructs the message box
        /*!
         *  \param  title   Message box title
         *  \param  text    Text to appear above the buttons
         *  \param  buttons Bitmask specifying which buttons appear in the 
         *      message box
         *  \param  ...     Titles of any custom buttons
         *
         *  Standard buttons are assigned titles automatically; the titles of 
         *  any custom buttons (\p btnCustom1, \p btnCustom2, etc.) are 
         *  specified as \p const \p wchar_t* parameters after the \p buttons 
         *  parameter.
         */
        MessageBox(const wchar_t *title, const wchar_t *text, unsigned buttons = btnOk, ...);
        ~MessageBox();
        void OnKeyDown(uint32_t key);
    };
};

#endif
