/* $Id: mgltest.c,v 1.7 2002/05/05 13:51:30 pavlovskii Exp $ */

#include <gl/mgl.h>
#include <gl/wmf.h>
#include <os/rtl.h>
#include <os/syscall.h>
#include <os/keyboard.h>

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <printf.h>
#include <stddef.h>

mglrc_t *rc;
MGLpoint gprintf_pos;

bool gwprintf_help(void* context, const wchar_t* str, size_t len)
{
    /*MGLpoint *pt;
    MGLrect rect, dims;
    unsigned i;

    mglGetDimensions(NULL, &dims);
    
    rect = dims;
    rect.left = pt->x;
    rect.top = pt->y;

    glDrawText(&rect, str, len);
    
    for (i = 0; i < len; i++)
        if (str[i] == '\n')
        {
            pt->x = dims.left;
            pt->y += 16;
        }
        else
            pt->x += 38;*/
    DbgWrite(str, len);
    return true;
}

int vgwprintf(MGLpoint *pt, const wchar_t *fmt, va_list ptr)
{
    if (pt == NULL)
        pt = &gprintf_pos;

    return dowprintf(gwprintf_help, pt, fmt, ptr);
}

int gwprintf(MGLpoint *pt, const wchar_t *fmt, ...)
{
    va_list ptr;
    int ret;

    if (pt == NULL)
        pt = &gprintf_pos;

    va_start(ptr, fmt);
    ret = vgwprintf(pt, fmt, ptr);
    va_end(ptr);
    return ret;
}

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER {
    uint32_t   biSize;
    int32_t    biWidth;
    int32_t    biHeight;
    uint16_t   biPlanes;
    uint16_t   biBitCount;
    uint32_t   biCompression;
    uint32_t   biSizeImage;
    int32_t    biXPelsPerMeter;
    int32_t    biYPelsPerMeter;
    uint32_t   biClrUsed;
    uint32_t   biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
} RGBQUAD;

void GeneratePalette(RGBQUAD *palette)
{
    int i;

#define SC(x) ((((x)%6) * 252) / 5)
    for (i = 0; i < 16; i++)
    {
        palette[i].rgbRed = i * 16;
        palette[i].rgbGreen = i * 16;
        palette[i].rgbBlue = i * 16;
    }

    for (; i < 40; i++)
        palette[i].rgbRed = 
            palette[i].rgbGreen = 
            palette[i].rgbBlue = 255;

    for (; i < 256; i++)
    {
        palette[i].rgbRed = SC((i - 40) / 36);
        palette[i].rgbGreen = SC((i - 40) / 6);
        palette[i].rgbBlue = SC(i - 40);
    }
#undef SC
}

void WriteScreenshot(handle_t file)
{
    int i, first_bit, written;
    uint8_t *ptr;
    BITMAPFILEHEADER bmf;
    BITMAPINFOHEADER bmi;
    RGBQUAD palette[256];
    uint8_t *image;
    uint16_t *rgb;
    bool has_palette;

    ptr = VmmMapShared(L"fb_s3", NULL, 3 | MEM_READ | MEM_WRITE);

    bmi.biSize = sizeof(bmi);
    bmi.biWidth = 640;
    bmi.biHeight = 480;
    bmi.biPlanes = 1;
    bmi.biBitCount = 24;
    bmi.biCompression = 0;
    bmi.biSizeImage = ((bmi.biWidth * bmi.biBitCount + 31) & ~31) / 8 
        * bmi.biHeight;
    bmi.biXPelsPerMeter = 
        bmi.biYPelsPerMeter = 0;
    bmi.biClrUsed = 0;
    bmi.biClrImportant = 0;

    has_palette = (bmi.biBitCount <= 8) ? 1 : 0;
    bmf.bfType = 'MB';
    bmf.bfSize = sizeof(BITMAPFILEHEADER) 
        + sizeof(BITMAPINFOHEADER)
        + has_palette * (sizeof(RGBQUAD) * 256)
        + bmi.biSizeImage;
    bmf.bfReserved1 = bmf.bfReserved2 = 0;
    bmf.bfOffBits = sizeof(BITMAPFILEHEADER) 
        + sizeof(BITMAPINFOHEADER)
        + has_palette * (sizeof(RGBQUAD) * 256);

    if (has_palette)
        GeneratePalette(palette);

    image = malloc(bmi.biSizeImage);
    rgb = (uint16_t*) ptr;
    for (i = 0; i < 640 * 480; i++, rgb++)
    {
        /* 5-6-5 R-G-B; DIB uses 8-8-8 B-G-R */
        image[i * 3 + 0] = (*rgb >> 0) << 3;
        image[i * 3 + 1] = (*rgb >> 5) << 2;
        image[i * 3 + 2] = (*rgb >> 11) << 3;
    }

    FsWriteSync(file, &bmf, sizeof(bmf), NULL);
    FsWriteSync(file, &bmi, sizeof(bmi), NULL);

    if (has_palette)
        FsWriteSync(file, palette, sizeof(palette), NULL);

    first_bit = 32768 
        - (sizeof(bmf) + sizeof(bmi) + has_palette * sizeof(palette));
    FsWriteSync(file, image, first_bit, NULL);
    written = first_bit;
    while (written < bmi.biSizeImage)
    {
        FsWriteSync(file, 
            image + written, 
            min(32768, bmi.biSizeImage - written), 
            NULL);
        written += min(32768, bmi.biSizeImage - written);
    }

    free(image);
}

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *str;
    wmf_t *wmf;
    MGLrect dims;
    handle_t file;

    if (argc >= 2)
	str = argv[1];
    else
	str = L"/hd/boot/dir2.wmf";
    
    wmf = WmfOpen(str);
    if (wmf == NULL)
    {
	_pwerror(str);
	return EXIT_FAILURE;
    }

    rc = mglCreateRc(NULL);
    glSetColour(0x000000);
    glSetClearColour(0xffffff);
    glClear();
    
    mglGetDimensions(NULL, &dims);

    if (dims.right > dims.bottom)
        dims.right = dims.bottom;
    else
        dims.bottom = dims.right;

    WmfDraw(wmf, &dims);
    WmfClose(wmf);

    /*for (i = 0; i < 256; i++) 
    {
        glSetColour(MGL_COLOUR(255 - i, 0, 0));
        glFillRect((i * dims.right) / 256, 0, 
            ((i + 1) * dims.right) / 256, 30);
        glSetColour(MGL_COLOUR(0, i, 0));
        glFillRect((i * dims.right) / 256, 30, 
            ((i + 1) * dims.right) / 256, 60);
        glSetColour(MGL_COLOUR(0, 0, 255 - i));
        glFillRect((i * dims.right) / 256, 60, 
            ((i + 1) * dims.right) / 256, 90);
        glSetColour(MGL_COLOUR(i, i, i));
        glFillRect((i * dims.right) / 256, 90, 
            ((i + 1) * dims.right) / 256, 180);
    }
      
    glSetColour(0);
    glDrawText(&dims, L"Hello, world!\nThis is a test of the FreeType font engine.", -1);*/

    file = FsOpen(L"/hd/boot/screen.bmp", FILE_WRITE);
    if (file == NULL)
        _pwerror(L"/hd/boot/screen.bmp");
    else
    {
        WriteScreenshot(file);
        FsClose(file);
    }

    while (ConReadKey() & KBD_BUCKY_RELEASE)
        ;

    mglDeleteRc(rc);
    return EXIT_SUCCESS;
}
