/* $Id: window.h,v 1.3 2002/04/10 12:32:37 pavlovskii Exp $ */

#ifndef __GUI_WINDOW_H
#define __GUI_WINDOW_H

#include <gl/types.h>
#include <os/gui.h>
#include <list>

namespace os
{
    //! Window class
    /*!
     *  \ingroup    gui
     *  This is the base class for all windows (including frames, views and 
     *  controls). It is the interface to the kernel-mode window manager, and 
     *  maintains some user-mode data as well.
     *
     *  \p Window objects are not used directly but as the base of some more 
     *  useful class, such as \p Frame, \p Container or \p Control.
     */
    class Window
    {
    protected:
        handle_t m_handle;
        Window *m_parent;
        std::list<Window*> m_children;

        //! Window constructor
        /*!
         *  Constructs a \p Window object without creating a window
         */
        Window();

        //! Window constructor
        /*!
         *  Creates a window
         */
        Window(Window *parent, const wchar_t *title, const MGLrect &pos);

        //! Window constructor
        /*!
         *  Binds a Window object to a window handle
         */
        Window(handle_t hnd);

        //! Creates the underlying window
        /*!
         *  \param  parent  Parent for this window, or \p NULL for a top-level 
         *      window.
         *  \param  title   Window title, or \p NULL to create a window 
         *      without a title.
         *  \param  pos     Position of the window, in screen units.
         */
        void Create(Window *parent, const wchar_t *title, const MGLrect &pos);
        void AddChild(Window *child);
        void RemoveChild(Window *child);

        handle_t GetHandle()
        {
            if (this)
                return m_handle;
            else
                return 0;
        }

    public:
        virtual ~Window();

        operator bool()
        {
            return m_handle != 0;
        }

        //! Gets the on-screen position of the window
        bool GetPosition(MGLrect *rect) const;
        //! Sets the on-screen position of the window
        void SetPosition(const MGLrect &rect);

        //! Gets the window's title
        bool GetTitle(wchar_t *title, size_t max) const;

        //! Gets the length of the window's title
        size_t GetTitleLength() const;
        
        //! Sets the window's title
        void SetTitle(wchar_t *title);

        //! Invalidates a portion of the window
        /*!
         *  The area is added to any previous invalid areas, and the total 
         *  invalid area will be redrawn (via \p OnPaint) next time the 
         *  application processes messages.
         *  \param  rect    The rectangle to invalidate
         */
        void Invalidate(MGLrect *rect = 0);

        //! Sets the input focus to this window
        void SetFocus();

        //! Posts a message in the message queue of the window's owner thread
        void PostMessage(uint32_t code, uint32_t param1 = 0, 
            uint32_t param2 = 0, uint32_t param3 = 0);

        //! Returns \p true if the window, or one of its children, has the 
        //!     input focus; returns \p false otherwise.
        bool HasFocus() const;

        bool SetCapture();
        bool ReleaseCapture();

        //! Returns a pointer to the window's next child
        Window *GetNextChild(Window *child);

        virtual void HandleMessage(const msg_t *msg);
        virtual void OnPaint();
        virtual void OnKeyDown(uint32_t key);
        virtual void OnKeyUp(uint32_t key);
        virtual void OnCommand(unsigned id);
        virtual void OnFocus();
        virtual void OnBlur();
        virtual void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
        virtual void OnMouseUp(uint32_t buttons, MGLreal x, MGLreal y);
        virtual void OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y);
        virtual void OnMouseWheel(int delta, MGLreal x, MGLreal y);
    };
};

#endif
