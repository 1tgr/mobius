/* $Id: taskbar.h,v 1.1 2002/04/11 00:32:30 pavlovskii Exp $ */

#ifndef DESKTOP_TASKBAR_H__
#define DESKTOP_TASKBAR_H__

#include <gui/view.h>

class Taskbar : public os::Container
{
public:
    Taskbar(os::Container *parent);
    ~Taskbar();
    void OnPaint();
    void OnCommand(unsigned id);

    static void PowerOff();
};

#endif
