/* $Id: desktop.h,v 1.1 2002/04/03 23:26:44 pavlovskii Exp $ */

#ifndef DESKTOP_H__
#define DESKTOP_H__

#include <gui/window.h>

class AltTabWindow;

class Desktop : public os::Window
{
public:
    AltTabWindow *m_altTabWindow;

    Desktop();
    void OnPaint();
    void OnKeyDown(uint32_t key);
    void OnCommand(unsigned id);

    void PowerOff();
};

#endif
