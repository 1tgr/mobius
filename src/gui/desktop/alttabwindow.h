/* $Id: alttabwindow.h,v 1.1 2002/04/03 23:26:44 pavlovskii Exp $ */

#ifndef ALTTABWINDOW_H__
#define ALTTABWINDOW_H__

#include <gui/window.h>

class AltTabWindow : public os::Window
{
public:
    AltTabWindow(os::Window *parent);
    void OnPaint();
    void OnKeyUp(uint32_t key);
};

#endif
