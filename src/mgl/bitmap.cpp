/* $Id: bitmap.cpp,v 1.1 2002/09/13 23:23:01 pavlovskii Exp $ */

#include <mgl/bitmap.h>
#include <mgl/rc.h>
#include <stdlib.h>

using namespace mgl;

Bitmap::Bitmap(unsigned width, unsigned height, unsigned char bpp)
{
    m_desc.bpp = bpp;
    m_desc.bytes_per_line = (width * bpp) / 8;
    m_desc.width = width;
    m_desc.height = height;

    m_buffer = malloc(m_desc.height * m_desc.bytes_per_line);
    m_owns_buffer = true;
}

Bitmap::~Bitmap()
{
    if (m_owns_buffer)
        free(m_buffer);
}

void Bitmap::Draw(Rc *dest, MGLreal x, MGLreal y)
{
    Bitmap *bmp;
    point_t pos;

    bmp = dest->GetBitmap();
    if (bmp == NULL)
        return;

    dest->Transform(x, y, pos);
    switch (bmp->GetColourDepth())
    {
    case 8:
        Draw8bpp(bmp, pos.x, pos.y);
        break;

    case 16:
        Draw16bpp(bmp, pos.x, pos.y);
        break;
    }
}

/*
 * Copy to bitmap: dest is 8bpp
 */

void Bitmap::Draw8bpp(Bitmap *dest, int x, int y)
{
    BitmapLock<uint8_t*> lock(dest);
    uint8_t *pix_dest;
    unsigned ay;

    pix_dest = lock + y * dest->m_desc.bytes_per_line + x;
    if (m_desc.bpp == 8)
    {
        uint8_t *pix_src;
        pix_src = (uint8_t*) m_buffer;
        for (ay = 0; 
            ay < m_desc.height; 
            ay++, pix_src += m_desc.bytes_per_line, pix_dest += dest->m_desc.bytes_per_line)
            memcpy(pix_dest, pix_src, m_desc.bytes_per_line);
    }
}

/*
 * Copy to bitmap: dest is 16bpp
 */

void Bitmap::Draw16bpp(Bitmap *dest, int x, int y)
{
    BitmapLock<uint8_t*> lock(dest);
    uint16_t *pix_dest;
    unsigned ay;

    pix_dest = (uint16_t*) (lock + y * dest->m_desc.bytes_per_line) + x;
    if (m_desc.bpp == 16)
    {
        uint16_t *pix_src;
        pix_src = (uint16_t*) m_buffer;
        for (ay = 0; 
            ay < m_desc.height; 
            ay++, pix_src += m_desc.bytes_per_line / 2, pix_dest += dest->m_desc.bytes_per_line / 2)
            memcpy(pix_dest, pix_src, m_desc.bytes_per_line);
    }
}
