/* $Id: application.h,v 1.3 2002/08/04 17:22:39 pavlovskii Exp $ */

#ifndef __GUI_APPLICATION_H
#define __GUI_APPLICATION_H

#include <mgl/rc.h>
#include <os/gui.h>

namespace os
{
    //! Application class
    /*!
     *  \ingroup    gui
     *  Each process requires one application object before any of the GUI 
     *  classes can be used. Usually, an instance of \p os::Application is 
     *  constructed in \p main , from where the message loop runs.
     */
    class Application
    {
    protected:
        static Application *g_theApp;

    public:
        //! MGL rendering context used by the application
        //mglrc_t *m_rc;
        mgl::RcDevice m_rc;

        //! Retrieves a pointer to the process's application object
        static Application *GetApplication();

        //! Application constructor
        /*!
         *  \param  name    Application name in MIME format: 
         *      \p application/x-vnd.string , where \p string is a unique
         *      application identifier.
         */
        Application(const wchar_t *name);
        //! Application destructor
        virtual ~Application();

        //! Runs the central message loop for the thread
        /*!
         *  \return The thread's exit code
         */
        int Run();

        //! Waits for and obtains a message from the thread's message queue
        bool GetMessage(msg_t *msg);

        //! Handles a message obtained from \p GetMessage
        void HandleMessage(const msg_t *msg);
    };
};

#endif