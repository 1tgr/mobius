/* $Id: alttabwindow.h,v 1.2 2002/09/13 23:26:02 pavlovskii Exp $ */

#ifndef ALTTABWINDOW_H__
#define ALTTABWINDOW_H__

#include <gui/window.h>

class AltTabWindow : public os::Window
{
public:
    AltTabWindow(os::Window *parent);
    void OnPaint(mgl::Rc *rc);
    void OnKeyUp(uint32_t key);
};

#endif
