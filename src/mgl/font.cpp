/* $Id: font.cpp,v 1.2 2002/12/18 23:06:10 pavlovskii Exp $ */

#include <mgl/font.h>
#include <mgl/fontmanager.h>
#include <mgl/rc.h>

#include <ft2build.h>
#include <freetype/freetype.h>

#include <os/defs.h>
#include <os/syscall.h>
#include <os/rtl.h>
#include <stdlib.h>
#include <wchar.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

using namespace mgl;

Font::Font(const wchar_t *filename, unsigned size_twips)
{
    int error;
    dirent_standard_t di;
    FT_Open_Args args;

    if (!FsQueryFile(filename, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
        m_font_data = NULL;
        m_ft_face = NULL;
        return;
    }

    wprintf(L"Font::Font: mapping file (%u bytes)...", di.length);
    m_file = FsOpen(filename, FILE_READ);
    m_font_data = VmmMapFile(m_file, 
        NULL, 
        PAGE_ALIGN_UP(di.length) / PAGE_SIZE, 
        VM_MEM_USER | VM_MEM_READ);
    wprintf(L"done (%p)\n", m_font_data);

    wprintf(L"Loading face...");
    args.flags = ft_open_memory;
    args.memory_base = (const FT_Byte*) m_font_data;
    args.memory_size = di.length;
    error = FT_Open_Face(FontManager::GetInstance()->m_ft_library,
	&args,
	0,
	&m_ft_face);
    if (error == 0)
    {
        do
        {
            error = FT_Set_Char_Size(m_ft_face, 0, (size_twips * 64) / 20, 96, 96);
            size_twips += 20;
        } while (error != 0 && size_twips <= 6000);

        wprintf(L"done, size_twips = %u, error = %u\n", size_twips, error);
    }
    else
        wprintf(L"failed: error = %u\n", error);
}

Font::~Font()
{
    FT_Done_Face(m_ft_face);
    VmmFree(m_font_data);
    HndClose(m_file);
}

static void draw_bitmap(Rc* rc, FT_Bitmap *bmp, int sx, int sy, 
                        MGLcolour fg, MGLcolour bg)
{
    colour_t clr;
    int x, y, r, g, b, ar, ag, ab;
    uint8_t *ptr;

    ptr = bmp->buffer;
    for (y = 0; y < bmp->rows; y++)
    {
        for (x = 0; x < bmp->width; x++)
        {
            switch (bmp->pixel_mode)
            {
            case ft_pixel_mode_mono:
                if (fg != (MGLcolour) -1 && ptr[x / 8] & (0x80 >> (x & 7)))
                    (rc->*(rc->DoPutPixel))(sx + x, sy + y, fg);
                if (bg != (MGLcolour) -1 && (ptr[x / 8] & (0x80 >> (x & 7))) == 0)
                    (rc->*(rc->DoPutPixel))(sx + x, sy + y, bg);
                break;

            case ft_pixel_mode_grays:
                if (bg == (MGLcolour) -1)
                    clr = (rc->*(rc->DoGetPixel))(sx + x, sy + y);
                else
                    clr = bg;

                if (fg != (MGLcolour) -1 && ptr[x] != 0)
                {
                    int p, rp;

                    p = ptr[x] / 16;
                    rp = 15 - p;
                    r = MGL_RED(clr);
                    g = MGL_GREEN(clr);
                    b = MGL_BLUE(clr);
                    ar = MGL_RED(fg);
                    ag = MGL_GREEN(fg);
                    ab = MGL_BLUE(fg);
                    r = (r * rp + ar * p) / 16;
                    g = (g * rp + ag * p) / 16;
                    b = (b * rp + ab * p) / 16;
                    (rc->*(rc->DoPutPixel))(sx + x, sy + y, MGL_COLOUR(r, g, b));
                }

                if (bg != (MGLcolour) -1 && ptr[x] == 0)
                    (rc->*(rc->DoPutPixel))(sx + x, sy + y, bg);
                break;
            }
        }

        ptr += bmp->pitch;
    }
}

void Font::IterateString(Rc *rc, const MGLrect& rect, const wchar_t *str, 
                         int len, point_t *size,
                         void (*fn)(Rc *, FT_Bitmap *, int, int, 
                            MGLcolour, MGLcolour))
{
    int error, x, y, maxx, maxy, hgt;
    rect_t trect;

    if (m_ft_face == NULL)
        return;

    if (len == -1)
        len = wcslen(str);

    rc->Transform(rect, trect);
    hgt = m_ft_face->size->metrics.height >> 7;
    maxx = x = trect.left;
    maxy = y = trect.top + (m_ft_face->size->metrics.ascender >> 6);//+ hgt;
    //gap = hgt / 4;

    //_wdprintf(L"Font::DrawText: num_glyphs = %u ", m_ft_face->num_glyphs);
    while (len > 0)
    {
        if (*str != '\n')
            error = FT_Load_Char(m_ft_face, *str, FT_LOAD_RENDER);
        else
            error = 1;

        if (*str == '\n' ||
            x + (m_ft_face->glyph->advance.x >> 6) > trect.right + 1)
        {
            x = trect.left;
            y += hgt;
            if (y > maxy)
                maxy = y;

            if (y > trect.bottom + 1)
                break;
        }

        if (error == 0)
        {
            //_wdprintf(L"%c(%d)", *str, m_ft_face->glyph->bitmap_top >> 6);
            //vid->vidHLine(vid, x + m_ft_face->glyph->bitmap_left,
                //x + m_ft_face->glyph->bitmap_left + m_ft_face->glyph->bitmap.width,
                //y - m_ft_face->glyph->bitmap_top,
                //7);
            if (fn != NULL &&
                (rc->m_state.colour_fill != (MGLcolour) -1 || 
                rc->m_state.colour_pen != (MGLcolour) -1))
                fn(rc,
                    &m_ft_face->glyph->bitmap,
                    x + m_ft_face->glyph->bitmap_left, 
                    y - m_ft_face->glyph->bitmap_top /*- gap*/,
                    rc->m_state.colour_pen, 
                    rc->m_state.colour_fill);

            x += m_ft_face->glyph->advance.x >> 6;
            if (x > maxx)
                maxx = x;
        }
        //else
            //_wdprintf(L"%c[%x]", *str, error);

        str++;
        len--;
    }

    //_wdprintf(L"\n");
    if (size != NULL)
    {
        size->x = maxx - trect.left;
        size->y = maxy - trect.top - (m_ft_face->size->metrics.descender >> 6);
    }
}

MGLpoint Font::GetTextSize(Rc *rc, const wchar_t *str, int len)
{
    point_t size;
    MGLpoint ret;
    MGLrect rect(0, 0, 10000, 10000);

    size.x = size.y = 0;
    IterateString(rc, rect, str, len, &size, NULL);
    rc->InverseTransform(size.x, size.y, ret);
    wprintf(L"Font::GetTextSize(%s): size = (%d,%d) ret = (%d,%d)\n",
        str, size.x, size.y, ret.x, ret.y);
    return ret;
}

void Font::DrawText(Rc *rc, const MGLrect& rect, const wchar_t *str, int len)
{
    IterateString(rc, rect, str, len, NULL, draw_bitmap);
}

void Font::GetMetrics(Rc *rc, FontMetrics *m)
{
    rect_t r;
    MGLrect ret;
    //MGLpoint pt;
    FT_Size_Metrics *metrics;

    //r.left = m_ft_face->size->metrics.x_ppem;
    r.left = m_ft_face->max_advance_width;
    r.right = 0;
    r.top = m_ft_face->ascender;
    r.bottom = -m_ft_face->descender / 2;
    wprintf(L"width = %d ascender = %d descender = %d height = %d\n",
        r.left, r.top, r.bottom, m_ft_face->height);

    rc->InverseTransform(r, ret);
    /*ret.left = r.left;
    ret.top = r.top;
    ret.right = r.right;
    ret.bottom = r.bottom;*/
    //rc->InverseTransform(0, m_ft_face->height / 2, pt);

    metrics = &m_ft_face->size->metrics;
    m->char_width_max = (ret.left * metrics->x_ppem) / (m_ft_face->units_per_EM);
    m->char_width_min = 0;
    m->ascent = (ret.top * metrics->y_ppem) / (m_ft_face->units_per_EM);
    m->descent = (ret.bottom * metrics->y_ppem) / (m_ft_face->units_per_EM);
    //m->height = (pt.y * metrics->y_ppem) / (m_ft_face->units_per_EM);
    m->height = m->ascent + m->descent;
    wprintf(L"width = %d ascender = %d descender = %d height = %d\n",
        m->char_width_max, m->ascent, m->descent, m->height);
}
