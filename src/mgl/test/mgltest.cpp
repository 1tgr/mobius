/* $Id: mgltest.cpp,v 1.1 2002/09/13 23:23:01 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <stddef.h>
#include <stdio.h>

#include <os/defs.h>
#include <os/rtl.h>
#include <os/keyboard.h>
#include <os/syscall.h>

#include <mgl/rc.h>
#include <mgl/metafile.h>
#include <mgl/bitmap.h>
#include <mgl/fontmanager.h>
#include <mgl/font.h>

using namespace mgl;

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

static void GeneratePalette(RGBQUAD *palette)
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

void WriteScreenshot(Rc *rc, handle_t file)
{
    unsigned first_bit, written;
    uint8_t *ptr;
    BITMAPFILEHEADER bmf;
    BITMAPINFOHEADER bmi;
    RGBQUAD palette[256];
    uint8_t *image, *line;
    uint16_t *rgb;
    bool has_palette;
    int x, y;

    BitmapLock<uint8_t*> lock(rc->GetBitmap());

    ptr = lock;
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

    image = (uint8_t*) malloc(bmi.biSizeImage);
    rgb = (uint16_t*) ptr;

    /* DIBs are bottom-up (OS/2) so write the BGR values backwards */
    line = image + bmi.biHeight * bmi.biWidth * 3;
    for (y = 0; y < bmi.biHeight; y++)
    {
        line -= bmi.biWidth * 3;
        for (x = 0; x < bmi.biWidth; x++)
        {
            /* 5-6-5 R-G-B; DIB uses 8-8-8 B-G-R */
            line[x * 3 + 0] = (*rgb >> 0) << 3;
            line[x * 3 + 1] = (*rgb >> 5) << 2;
            line[x * 3 + 2] = (*rgb >> 11) << 3;
            rgb++;
        }
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

int main()
{
    RcDevice rc(SYS_DEVICES L"/Classes/video0");
    Bitmap bmp(48, 48, 16);
    Rc bmp_rc(&bmp);
    FontManager mgr;
    //Font *font;
    //dirent_t di;
    //handle_t dir;
    //wchar_t path[256], *ext;
    //rect_t temp;

    MGLrect dims;//, icondims;
    //MGLreal x, y;
    //handle_t file;

    if (!rc.SetMode(640, 480, 8))
        return EXIT_FAILURE;

    rc.SetFillColour(0xffffff);
    dims = rc.GetDimensions();
    rc.FillRect(dims);

    dims = MGLrect(100, 100, 200, 200);
    rc.SetFillColour(0xC00000);
    rc.FillRect(dims);

    /*dir = FsOpenDir(L"/hd/mobius");
    x = 0;
    y = dims.bottom / 2;
    if (dir != NULL)
    {
        _wdprintf(L"dir = %u\n", dir);
        while (FsReadDir(dir, &di, sizeof(di)))
        {
            Metafile *m;
            
            swprintf(path, L"/hd/mobius/%s", di.name);
            _wdprintf(L"%s\n", path);
            ext = wcsrchr(path, '.');
            if (ext == NULL ||
                _wcsicmp(ext, L".wmf") != 0)
                continue;

            m = new Metafile;
            if (!m->Open(path))
            {
                delete m;
                continue;
            }

            icondims = bmp_rc.GetDimensions();
            bmp_rc.SetFillColour(0xffffff);
            bmp_rc.FillRect(icondims);
            m->Draw(&bmp_rc, icondims);

            bmp_rc.Transform(icondims, temp);
            rc.InverseTransform(temp, icondims);

            if (x + icondims.Width() > dims.right)
            {
                x = 0;
                y += icondims.Height();
            }

            bmp.Draw(&rc, x, y);
            x += icondims.Width();

            delete m;
        }

        FsClose(dir);
    }*/

    /*rc.SetFillColour((MGLcolour) -1);
    font = new Font(L"/mobius/bluehfin.ttf", 20 * 20);
    font->DrawText(&rc, dims, 
        L"The Möbius Graphics Layer " L"\x2013" L" Test Program\n"
        L"Tim Robinson, May 2002");
    delete font;*/

    /*file = FsOpen(L"/hd/mobius/screen.bmp", FILE_WRITE);
    if (file == NULL)
        _pwerror(L"/hd/mobius/screen.bmp");
    else
    {
        WriteScreenshot(&rc, file);
        FsClose(file);
    }*/

    while (ConReadKey() & KBD_BUCKY_RELEASE)
        ;

    return EXIT_SUCCESS;
}
