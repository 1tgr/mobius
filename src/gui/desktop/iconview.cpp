/* $Id: iconview.cpp,v 1.1 2002/04/10 12:26:31 pavlovskii Exp $ */

#include "iconview.h"
#include <gl/mgl.h>
#include <wchar.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>

IconView::IconView(os::Container *parent) : os::View(parent)
{
    m_dir = _wcsdup(L"*");
}

IconView::~IconView()
{
    free(m_dir);
}

void IconView::OnPaint()
{
    MGLrect rect;
    MGLpoint size;
    dirent_t di;
    handle_t search;

    GetPosition(&rect);
    glSetColour(0x0040C0);
    glFillRect(rect.left, rect.top, rect.right, rect.bottom);

    glSetColour(0x000000);
    glGetTextSize(m_dir, -1, &size);

    search = FsOpenSearch(m_dir);
    if (search != NULL)
    {
        rect.bottom = rect.top + size.y;
        while (FsReadSync(search, &di, sizeof(di), NULL))
        {
            glDrawText(&rect, di.name, -1);
            RectOffset(&rect, 0, rect.Height());
        }
    }
}
