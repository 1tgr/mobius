/* $Id: iconview.h,v 1.2 2002/04/11 00:31:01 pavlovskii Exp $ */

#ifndef DESKTOP_ICONVIEW_H__
#define DESKTOP_ICONVIEW_H__

#include <gui/view.h>
#include <vector>
#include <gl/wmf.h>

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
    wmf_t *m_icon;
    Item *m_mouseOver;

public:
    IconView(os::Container *parent);
    ~IconView();
    void OnPaint();
    void OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y);
    void OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y);

    void Refresh();
    Item& AddItem(const wchar_t *text);
    MGLrect GetItemRect(const Item &item) const;
};

#endif
