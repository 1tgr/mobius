/* $Id: desktop.h,v 1.3 2002/04/11 00:31:01 pavlovskii Exp $ */

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
    void OnPaint();
    void OnKeyDown(uint32_t key);
    void OnCommand(unsigned id);
};

#endif
