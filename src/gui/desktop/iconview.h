/* $Id: iconview.h,v 1.1 2002/04/10 12:26:31 pavlovskii Exp $ */

#ifndef DESKTOP_ICONVIEW_H__
#define DESKTOP_ICONVIEW_H__

#include <gui/view.h>

class IconView : public os::View
{
protected:
    wchar_t *m_dir;

public:
    IconView(os::Container *parent);
    ~IconView();
    void OnPaint();
};

#endif
