/* $Id: desktop.h,v 1.4 2002/09/13 23:26:02 pavlovskii Exp $ */

#ifndef DESKTOP_H__
#define DESKTOP_H__

#include <gui/container.h>

class AltTabWindow;
class IconView;
class Taskbar;

class Desktop : public os::Container
{
public:
    AltTabWindow *m_altTabWindow;
    IconView *m_icons;
    Taskbar *m_taskbar;

    Desktop();
    void OnPaint(mgl::Rc *rc);
    void OnKeyDown(uint32_t key);
    void OnCommand(unsigned id);
};

#endif
