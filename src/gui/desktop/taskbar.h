/* $Id: taskbar.h,v 1.2 2002/09/13 23:26:02 pavlovskii Exp $ */

#ifndef DESKTOP_TASKBAR_H__
#define DESKTOP_TASKBAR_H__

#include <gui/view.h>

class Taskbar : public os::Container
{
public:
    Taskbar(os::Container *parent);
    ~Taskbar();
    void OnPaint(mgl::Rc *rc);
    void OnCommand(unsigned id);

    static void PowerOff();
};

#endif
