/* $Id: iconview.h,v 1.4 2002/09/13 23:26:02 pavlovskii Exp $ */

#ifndef DESKTOP_ICONVIEW_H__
#define DESKTOP_ICONVIEW_H__

#include <gui/view.h>
#include <vector>
//#include <gl/wmf.h>
#include <mgl/metafile.h>

class IconView : public os::View
{
public:
    struct Item
    {
        MGLpoint origin;
        wchar_t *text;
    };

protected:
    wchar_t *m_dir;
    std::vector<Item> m_items;
    mgl::Metafile m_icon;
    Item *m_mouseOver;
    bool m_needErase;

public:
    MGLcolour m_colour;

    IconView(os::Container *parent, const wchar_t *dir);
    ~IconView();
    void OnPaint(mgl::Rc *rc);
    void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
    void OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y);

    void Refresh();
    Item& AddItem(const wchar_t *text);
    MGLrect GetItemRect(const Item &item) const;
};

#endif
