/*
 * $History: font.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:32
 * Updated in $/coreos/framebuf
 * Improved size measurement in FontDrawText
 * Added history block
 */

#include <os/framebuf.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>

#include <ft2build.h>
#include <freetype/freetype.h>

static FT_Library ft_library;

struct font_t
{
    lmutex_t lmux;
    FT_Face face;
    void *data;
    uint32_t options;

    colour_t last_fg;
    colour_t last_bg;
    rgb_t palette2[2];
    rgb_t palette256[256];
};


font_t *FontLoad(const wchar_t *filename, unsigned size_points_64, uint32_t options)
{
    int error;
    dirent_standard_t di;
    FT_Face ft_face;
    handle_t file;
    void *font_data;
    font_t *font;

    if (ft_library == NULL &&
        !FontInit())
        return NULL;

    if (!FsQueryFile(filename, FILE_QUERY_STANDARD, &di, sizeof(di)))
        return NULL;

    file = FsOpen(filename, FILE_READ);
    font_data = VmmMapFile(file, 
        NULL, 
        PAGE_ALIGN_UP(di.length) / PAGE_SIZE, 
        VM_MEM_USER | VM_MEM_READ);
    HndClose(file);

    error = FT_New_Memory_Face(ft_library,
        font_data,
        di.length,
        0,
        &ft_face);
    if (error != 0)
    {
        _wdprintf(L"FontLoadFace(%s): failed: error = %u\n", 
            filename, error);
        VmmFree(font_data);
        return NULL;
    }

    do
    {
        error = FT_Set_Char_Size(ft_face, 0, size_points_64, 96, 96);
        size_points_64 += 64;
    } while (error != 0 && size_points_64 <= 6000);

    _wdprintf(L"FontLoadFace(%s): size_points_64 = %u, error = %u\n", 
        filename, size_points_64 - 64, error);

    font = malloc(sizeof(*font));
    memset(font, 0, sizeof(*font));
    LmuxInit(&font->lmux);
    font->face = ft_face;
    font->data = font_data;
    font->options = options;
    return font;
}


void FontDelete(font_t *font)
{
    FT_Done_Face(font->face);
    VmmFree(font->data);
    LmuxDelete(&font->lmux);
    free(font);
}


void FontGetMaxSize(font_t *font, unsigned *width, unsigned *height)
{
    *width = (font->face->size->metrics.max_advance + 63) >> 6;
    *height = (font->face->size->metrics.height + 64) >> 6;
}


void FontDrawText(font_t *font, surface_t *surf, unsigned x, unsigned y, 
                  const wchar_t *str, size_t length, 
                  colour_t fg, colour_t bg)
{
    FT_Error error;
    status_t status;
    rect_t rect;
    wchar_t previous;
    unsigned baseline, i, bitmap_x;
    int pitch;
    void *data;
    bitmapdesc_t bitmap;
    unsigned flags;
    point_t size;

    FontGetTextSize(font, str, length, &size);

    LmuxAcquire(&font->lmux);
    rect.left = x;
    rect.top = y;
    baseline = (font->face->size->metrics.ascender + 63) >> 6;
    bitmap_x = 0;

    if (fg != font->last_fg || 
        bg != font->last_bg)
    {
        font->palette2[0].red = COLOUR_RED(bg);
        font->palette2[0].green = COLOUR_GREEN(bg);
        font->palette2[0].blue = COLOUR_BLUE(bg);
        font->palette2[1].red = COLOUR_RED(fg);
        font->palette2[1].green = COLOUR_GREEN(fg);
        font->palette2[1].blue = COLOUR_BLUE(fg);

        for (i = 0; i < _countof(font->palette256); i++)
        {
            font->palette256[i].red = ((COLOUR_RED(fg) * i) + 
                (COLOUR_RED(bg) * (255 - i))) / 255;
            font->palette256[i].green = ((COLOUR_GREEN(fg) * i) 
                + (COLOUR_GREEN(bg) * (255 - i))) / 255;
            font->palette256[i].blue = ((COLOUR_BLUE(fg) * i) 
                + (COLOUR_BLUE(bg) * (255 - i))) / 255;
        }

        font->last_fg = fg;
        font->last_bg = bg;
    }

    bitmap.width = size.x;
    bitmap.height = size.y;
    bitmap.bits_per_pixel = 0;
    bitmap.pitch = 0;
    bitmap.data = NULL;
    bitmap.palette = font->palette2;

    status = surf->vtbl->SurfConvertBitmap(surf, &data, &pitch, &bitmap);
    bitmap.data = data;
    bitmap.pitch = pitch;
    if (status != 0)
    {
        LmuxRelease(&font->lmux);
        return;
    }

    if (font->options & FB_FONT_MONO)
        flags = FT_LOAD_TARGET_MONO;
    else
        flags = 0;

    previous = 0xffff;
    while (length > 0)
    {
        if (previous == 0xffff ||
            *str != previous)
        {
            error = FT_Load_Char(font->face, *str, FT_LOAD_RENDER | flags);
            previous = *str;
        }

        if (error == 0)
        {
            FT_Bitmap *ft_bitmap;
            bitmapdesc_t glyph;

            ft_bitmap = &font->face->glyph->bitmap;

            //surf->vtbl->SurfFillRect(surf,
                //x, y,
                //x + (font->face->glyph->advance.x >> 6), y + height,
                //bg);

            glyph.width = ft_bitmap->width;
            glyph.height = ft_bitmap->rows;
            glyph.pitch = ft_bitmap->pitch;
            glyph.data = ft_bitmap->buffer;

            switch (ft_bitmap->pixel_mode)
            {
            case FT_PIXEL_MODE_MONO:
                glyph.bits_per_pixel = 1;
                glyph.palette = font->palette2;
                break;

            case FT_PIXEL_MODE_GRAY:
                glyph.bits_per_pixel = 8;
                glyph.palette = font->palette256;
                break;
            }

            status = surf->vtbl->SurfConvertBitmap(surf, &data, &pitch, &glyph);
            glyph.data = data;
            glyph.pitch = pitch;
            if (status == 0)
            {
                rect_t rect_src, rect_dest;

                rect_src.left = rect_src.top = 0;
                rect_src.right = glyph.width;
                rect_src.bottom = glyph.height;

                rect_dest.left = bitmap_x + font->face->glyph->bitmap_left;
                rect_dest.top = baseline - font->face->glyph->bitmap_top;
                rect_dest.right = rect_dest.left + glyph.width;
                rect_dest.bottom = rect_dest.top + glyph.height;

                surf->vtbl->SurfBltMemoryToMemory(surf, 
                    &bitmap, &rect_dest,
                    &glyph, &rect_src);
            }

            free(glyph.data);
            bitmap_x += font->face->glyph->advance.x >> 6;
        }

        length--;
        str++;
    }

    rect.left = x;
    rect.top = y;
    rect.right = x + bitmap.width;
    rect.bottom = y + bitmap.height;
    surf->vtbl->SurfBltMemoryToScreen(surf,
        &rect,
        bitmap.data,
        bitmap.pitch);

    free(bitmap.data);
    LmuxRelease(&font->lmux);
}


void FontGetTextSize(font_t *font, const wchar_t *str, size_t length, point_t *size)
{
    wchar_t previous;
    FT_Error error;
    unsigned flags;

    LmuxAcquire(&font->lmux);

    if (font->options & FB_FONT_MONO)
        flags = FT_LOAD_TARGET_MONO;
    else
        flags = 0;

    size->x = 0;
    size->y = (font->face->size->metrics.height + 64) >> 6;

    previous = 0xffff;
    while (length > 0)
    {
        if (previous == 0xffff ||
            *str != previous)
        {
            error = FT_Load_Char(font->face, *str, FT_LOAD_RENDER | flags);
            previous = *str;
        }

        if (error == 0)
            size->x += font->face->glyph->advance.x >> 6;

        str++;
        length--;
    }

    LmuxRelease(&font->lmux);
}


uint32_t FontGetOptions(font_t *font)
{
    return font->options;
}


void FontSetOptions(font_t *font, uint32_t options)
{
    font->options = options;
}


static void FontCleanup(void)
{
    if (ft_library != NULL)
    {
        FT_Done_FreeType(ft_library);
        ft_library = NULL;
    }
}


bool FontInit(void)
{
    FT_Init_FreeType(&ft_library);
    atexit(FontCleanup);
    return true;
}
