/* $Id: iconview.cpp,v 1.5 2002/09/13 23:26:02 pavlovskii Exp $ */

#define __THROW_BAD_ALLOC printf("out of memory\n"); exit(1)
#include <stdio.h>

#include "iconview.h"
//#include <gl/mgl.h>
#include <wchar.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <gui/messagebox.h>
#include <mgl/fontmanager.h>

#define SIZE_ICON   100

IconView::IconView(os::Container *parent, const wchar_t *dir) : 
    os::View(parent, os::alClient, 0)
{
    m_colour = 0x0040C0;
    m_mouseOver = NULL;
    m_dir = _wcsdup(dir);
    m_icon.Open(L"banner.wmf");
    Refresh();
    m_needErase = true;
}

IconView::~IconView()
{
    free(m_dir);
}

void IconView::OnPaint(mgl::Rc *rc)
{
    MGLrect rect, icon;
    MGLpoint size;
    std::vector<Item>::iterator it;
    mgl::Font *font;

    font = mgl::FontManager::GetDefault(0);
    if (m_needErase)
    {
        GetPosition(&rect);
        rc->SetFillColour(m_colour);
        rc->FillRect(rect);
    }
    else
        m_needErase = true;

    if (m_items.size() > 0)
    {
        for (it = m_items.begin(); it != m_items.end(); ++it)
        {
            rect = GetItemRect(*it);
            size = font->GetTextSize(rc, it->text, -1);

            icon.left = (rect.left + rect.right - SIZE_ICON) / 2;
            icon.top = (rect.top + rect.bottom - size.y - SIZE_ICON) / 2;
            icon.right = icon.left + SIZE_ICON;
            icon.bottom = icon.top + SIZE_ICON;
            m_icon.Draw(rc, icon);

            rect.left = (rect.left + rect.right - size.x) / 2 - 4;
            rect.top = rect.bottom - size.y;
            rect.right = rect.left + size.x + 4;

            if (m_mouseOver == it)
                rc->SetFillColour(0xFFA000);
            else
                rc->SetFillColour(0xffffff);

            rc->FillRect(rect);

            rc->SetPenColour(0x000000);
            rc->Rectangle(rect);
            rect.left += 2;
            font->DrawText(rc, rect, it->text);
        }
    }
}

void IconView::Refresh()
{
    dirent_t di;
    handle_t search;

    search = FsOpenDir(m_dir);
    if (search != NULL)
        while (FsReadDir(search, &di, sizeof(di)))
            AddItem(di.name);

    HndClose(search);
}

IconView::Item& IconView::AddItem(const wchar_t *text)
{
    MGLrect rect;
    MGLpoint size, origin;

    GetPosition(&rect);
    if (m_items.empty())
        origin = MGLpoint(0, 0);
    else
    {
        Item &back = m_items.back();
        MGLrect backrect = GetItemRect(back);
        origin = MGLpoint(backrect.left, backrect.bottom + 8);
        if (origin.y + backrect.Height() > rect.Height())
            origin = MGLpoint(backrect.right + size.x, rect.top);
        origin.x -= rect.left;
        origin.y -= rect.top;
    }

    m_items.push_back(Item());

    Item &ret = m_items.back();
    ret.text = _wcsdup(text);
    ret.origin = origin;

    rect = GetItemRect(ret);
    m_needErase = false;
    Invalidate(&rect);
    return ret;
}

MGLrect IconView::GetItemRect(const Item &item) const
{
    MGLrect pos;
    MGLpoint size = MGLpoint(160, 150);
    GetPosition(&pos);
    return MGLrect(item.origin.x + pos.left, item.origin.y + pos.top,
        item.origin.x + pos.left + size.x, item.origin.y + pos.top + size.y);
}

void IconView::OnMouseDown(uint32_t buttons, MGLreal x, MGLreal y)
{
    std::vector<Item>::iterator it;
    MGLrect rect;

    for (it = m_items.begin(); it != m_items.end(); ++it)
    {
        rect = GetItemRect(*it);
        if (rect.IncludesPoint(x, y))
            break;
    }

    if (it != m_items.end())
    {
        wchar_t newdir[256];
        os::Frame *frame;
        IconView *view;

        wcscpy(newdir, m_dir);
        wcscat(newdir, it->text);
        wcscat(newdir, L"/");
        frame = new os::Frame(newdir, MGLrect(300, 300, 900, 900));
        view = new IconView(frame, newdir);
        view->m_colour = 0xffffff;
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
        if (rect.IncludesPoint(x, y))
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
            m_needErase = false;
            Invalidate(&rect);
        }

        m_mouseOver = mouseOver;
        if (mouseOver != NULL)
        {
            rect = GetItemRect(*mouseOver);
            m_needErase = false;
            Invalidate(&rect);
        }
    }
}
