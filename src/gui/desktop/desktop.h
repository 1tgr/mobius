/* $Id: desktop.h,v 1.2 2002/04/10 12:25:45 pavlovskii Exp $ */

#ifndef DESKTOP_H__
#define DESKTOP_H__

#include <gui/container.h>

class AltTabWindow;
class IconView;

class Desktop : public os::Container
{
public:
    AltTabWindow *m_altTabWindow;
    IconView *m_icons;

    Desktop();
    void OnPaint();
    void OnKeyDown(uint32_t key);
    void OnCommand(unsigned id);

    void PowerOff();
};

#endif
