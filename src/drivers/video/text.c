#include <stdio.h>
#include <os/syscall.h>
#include <kernel/fs.h>

#include "vidfuncs.h"

#include <ft2build.h>
#include FT_FREETYPE_H

FT_Library ft_library;
FT_Face ft_face;
void *font_data;

static void draw_bitmap(video_t *vid, const clip_t *clip, FT_Bitmap *bmp, 
                        int sx, int sy, colour_t fg, colour_t bg)
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
                if (fg != -1 && ptr[x / 8] & (0x80 >> (x & 7)))
                    vid->vidPutPixel(vid, clip, sx + x, sy + y, fg);
                if (bg != -1 && (ptr[x / 8] & (0x80 >> (x & 7))) == 0)
                    vid->vidPutPixel(vid, clip, sx + x, sy + y, bg);
                break;

            case ft_pixel_mode_grays:
                if (bg == -1)
                    clr = vid->vidGetPixel(vid, sx + x, sy + y);
                else
                    clr = bg;
                
                if (fg != -1 && ptr[x] != 0)
                {
                    int p, rp;

                    p = ptr[x] / 16;
                    rp = 15 - p;
                    r = COLOUR_RED(clr);
                    g = COLOUR_GREEN(clr);
                    b = COLOUR_BLUE(clr);
                    ar = COLOUR_RED(fg);
                    ag = COLOUR_GREEN(fg);
                    ab = COLOUR_BLUE(fg);
                    r = (r * rp + ar * p) / 16;
                    g = (g * rp + ag * p) / 16;
                    b = (b * rp + ab * p) / 16;
                    vid->vidPutPixel(vid, clip, sx + x, sy + y, MAKE_COLOUR(r, g, b));
                }

                if (bg != -1 && ptr[x] == 0)
                    vid->vidPutPixel(vid, clip, sx + x, sy + y, bg);
                break;
            }
        }

        ptr += bmp->pitch;
    }
}

void vidTextOut(video_t *vid, const clip_t *clip, rect_t *rect, 
                const wchar_t *str, size_t len, colour_t fg, colour_t bg)
{
    int error, x, y, maxx, maxy;

    //wprintf(L"height = 0x%x = %d\n", 
        //ft_face->size->metrics.height, ft_face->size->metrics.height >> 6);
    maxx = x = rect->left;
    maxy = y = rect->top + (ft_face->size->metrics.height >> 7);
    
    while (len > 0)
    {
        if (*str != '\n')
        {
            error = FT_Load_Char(ft_face, *str, FT_LOAD_RENDER);
            if (error)
                wprintf(L"FT_Load_Char: %x\n", error);
        }
        else
            error = 1;

        if (*str == '\n' ||
            x + (ft_face->glyph->advance.x >> 6) > rect->right)
        {
            x = rect->left;
            y += ft_face->size->metrics.height >> 6;
            if (y > maxy)
                maxy = y;

            if (y > rect->bottom)
                break;
        }

        if (error == 0)
        {
            //wprintf(L"%c(%d)", *str, ft_face->glyph->bitmap_top >> 6);
            //vid->vidHLine(vid, x + ft_face->glyph->bitmap_left,
                //x + ft_face->glyph->bitmap_left + ft_face->glyph->bitmap.width,
                //y - ft_face->glyph->bitmap_top,
                //7);
            if (fg != -1 || bg != -1)
                draw_bitmap(vid, 
                    clip, 
                    &ft_face->glyph->bitmap,
                    x + ft_face->glyph->bitmap_left, 
                    y - ft_face->glyph->bitmap_top,
                    fg, bg);

            x += ft_face->glyph->advance.x >> 6;
            if (x > maxx)
                maxx = x;
        }

        str++;
        len--;
    }

    rect->right = maxx;
    rect->bottom = maxy;
}

bool vidInitText(void)
{
    int error, pts;
    const wchar_t *font;
    dirent_t di;
    size_t size;

    FT_Init_FreeType(&ft_library);

    font = L"/System/Boot/amrtypen.ttf";

    if (FsQueryFile(font, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
        handle_t file;
        FT_Open_Args args;

        font_data = malloc(di.length);
        file = FsOpen(font, FILE_READ);
        FsReadSync(file, font_data, di.length, &size);
        FsClose(file);

        args.flags = ft_open_memory;
        args.memory_base = font_data;
        args.memory_size = size;
        error = FT_Open_Face(ft_library,
	    &args,
	    0,
	    &ft_face);
        if (error != 0)
            wprintf(L"FT_Open_Face: %x\n", error);
        else
        {
            pts = 10;
            do
            {
                wprintf(L"FT_Set_Char_Size: trying %d pt\n", pts);
                error = FT_Set_Char_Size(ft_face, 0, pts * 64, 96, 96);
                pts++;
            } while (error != 0 && pts <= 72);

            if (error != 0)
                wprintf(L"FT_Set_Char_Size: %x\n", error);
        }
    }

    return true;
}