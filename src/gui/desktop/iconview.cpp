/* $Id: iconview.cpp,v 1.2 2002/04/11 00:31:01 pavlovskii Exp $ */

#define __THROW_BAD_ALLOC printf("out of memory\n"); exit(1)
#include <stdio.h>

#include "iconview.h"
#include <gl/mgl.h>
#include <wchar.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <gui/messagebox.h>

#define SIZE_ICON   100

IconView::IconView(os::Container *parent) : os::View(parent, os::alClient, 0)
{
    m_mouseOver = NULL;
    m_dir = _wcsdup(L"/*");
    m_icon = WmfOpen(L"banner.wmf");
    Refresh();
}

IconView::~IconView()
{
    WmfClose(m_icon);
    free(m_dir);
}

void IconView::OnPaint()
{
    MGLrect rect, icon;
    MGLpoint size;
    std::vector<Item>::iterator it;

    GetPosition(&rect);
    glSetColour(0x0040C0);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);

    if (m_items.size() > 0)
    {
        for (it = m_items.begin(); it != m_items.end(); ++it)
        {
            rect = GetItemRect(*it);
            glGetTextSize(it->text, -1, &size);

            icon.left = (rect.left + rect.right - SIZE_ICON) / 2;
            icon.top = (rect.top + rect.bottom - size.y - SIZE_ICON) / 2;
            icon.right = icon.left + SIZE_ICON;
            icon.bottom = icon.top + SIZE_ICON;
            WmfDraw(m_icon, &icon);

            rect.left = (rect.left + rect.right - size.x) / 2 - 4;
            rect.top = rect.bottom - size.y;
            rect.right = rect.left + size.x + 4;

            if (m_mouseOver == it)
                glSetColour(0xFFA000);
            else
                glSetColour(0xffffff);

            glFillRect(rect.left, rect.top, rect.right, rect.bottom);

            glSetColour(0x000000);
            glRectangle(rect.left, rect.top, rect.right, rect.bottom);
            rect.left += 2;
            glDrawText(&rect, it->text, -1);
        }
    }
}

void IconView::Refresh()
{
    dirent_t di;
    handle_t search;

    search = FsOpenSearch(m_dir);
    if (search != NULL)
        while (FsReadSync(search, &di, sizeof(di), NULL))
            AddItem(di.name);
    FsClose(search);
}

IconView::Item& IconView::AddItem(const wchar_t *text)
{
    MGLrect rect;
    MGLpoint size, origin;

    GetPosition(&rect);
    if (m_items.empty())
        origin = MGLpoint(rect.left, rect.top);
    else
    {
        Item &back = m_items.back();
        MGLrect backrect = GetItemRect(back);
        origin = MGLpoint(backrect.left, backrect.bottom + 8);
        if (origin.y + backrect.Height() > rect.bottom)
            origin = MGLpoint(backrect.right + size.x, rect.top);
    }

    m_items.push_back(Item());

    Item &ret = m_items.back();
    ret.text = _wcsdup(text);
    ret.origin = origin;

    rect = GetItemRect(ret);
    Invalidate(&rect);
    return ret;
}

MGLrect IconView::GetItemRect(const Item &item) const
{
    MGLpoint size = MGLpoint(160, 150);
    return MGLrect(item.origin.x, item.origin.y,
        item.origin.x + size.x, item.origin.y + size.y);
}

void IconView::OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y)
{
    std::vector<Item>::iterator it;
    MGLrect rect;

    for (it = m_items.begin(); it != m_items.end(); ++it)
    {
        rect = GetItemRect(*it);
        if (RectIncludesPoint(&rect, x, y))
            break;
    }

    if (it != m_items.end())
    {
        wchar_t str[100];
        swprintf(str, L"You clicked on: %s", it->text);

        os::MessageBox box(L"Desktop", str);
        box.DoModal();
    }
}

void IconView::OnMouseMove(uint32_t buttons, MGLreal x, MGLreal y)
{
    std::vector<Item>::iterator it;
    Item *mouseOver;
    MGLrect rect;

    mouseOver = NULL;
    for (it = m_items.begin(); it != m_items.end(); ++it)
    {
        rect = GetItemRect(*it);
        if (RectIncludesPoint(&rect, x, y))
        {
            mouseOver = it;
            break;
        }
    }

    if (mouseOver != m_mouseOver)
    {
        if (m_mouseOver != NULL)
        {
            rect = GetItemRect(*m_mouseOver);
            Invalidate(&rect);
        }

        m_mouseOver = mouseOver;
        if (mouseOver != NULL)
        {
            rect = GetItemRect(*mouseOver);
            Invalidate(&rect);
        }
    }
}
