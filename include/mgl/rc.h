/* $Id: rc.h,v 1.2 2002/12/18 23:54:44 pavlovskii Exp $ */

#ifndef __MGL_RC_H
#define __MGL_RC_H

#include "types.h"
#include <os/video.h>
#include "refcount.h"
#include "bitmap.h"

namespace mgl
{

struct MGL_EXPORT DrawState
{
    MGLcolour colour_pen;
    MGLcolour colour_fill;

    DrawState()
    {
        colour_pen = 0x000000;
        colour_fill = 0xffffff;
    }
};

class MGL_EXPORT Rc : public RefCount<Rc>
{
protected:
    Bitmap *m_bitmap;
    MGLreal m_width, m_height;
    unsigned m_num_clip_rects;
    rect_t *m_clip_rects;

    void LineGeneric(int x1, int y1,  int x2, int y2, MGLcolour clr);
    void FillRectGeneric(const rect_t& rect, MGLcolour clr);

    void HLine8(int x1, int x2, int y, MGLcolour clr);
    void VLine8(int x, int y1, int y2, MGLcolour clr);
    void PutPixel8(int x, int y, MGLcolour clr);
    MGLcolour GetPixel8(int x, int y);
    void Scroll8(const rect_t& rect, int dx, int dy);

    void HLine16(int x1, int x2, int y, MGLcolour clr);
    void VLine16(int x, int y1, int y2, MGLcolour clr);
    void PutPixel16(int x, int y, MGLcolour clr);
    MGLcolour GetPixel16(int x, int y);
    void Scroll16(const rect_t& rect, int dx, int dy);

public:
    DrawState m_state;

    void (Rc::*DoLine)(int x1, int y1, int x2, int y2, MGLcolour clr);
    void (Rc::*DoHLine)(int x1, int x2, int y, MGLcolour clr);
    void (Rc::*DoVLine)(int x, int y1, int y2, MGLcolour clr);
    void (Rc::*DoPutPixel)(int x, int y, MGLcolour clr);
    MGLcolour (Rc::*DoGetPixel)(int x, int y);
    void (Rc::*DoFillRect)(const rect_t& rect, MGLcolour clr);
    void (Rc::*DoScroll)(const rect_t& rect, int dx, int dy);

    Rc(Bitmap *bitmap);
    virtual ~Rc();

    void Attach(Bitmap *bitmap);

    Bitmap *GetBitmap()
    {
        return m_bitmap;
    }

    void SetPenColour(MGLcolour clr)
    {
        m_state.colour_pen = clr;
    }

    void SetFillColour(MGLcolour clr)
    {
        m_state.colour_fill = clr;
    }

    void Transform(MGLreal x, MGLreal y, point_t& pt) const;
    void Transform(const MGLrect& a, rect_t& b) const;
    void InverseTransform(int x, int y, MGLpoint& pt) const;
    void InverseTransform(const rect_t& a, MGLrect& b) const;

    MGLrect GetDimensions() const
    {
        return MGLrect(0, 0, m_width, m_height);
    }

    void SetClip(const MGLclip& clip);

    void Line(MGLreal x1, MGLreal y1, MGLreal x2, MGLreal y2);
    void FillRect(const MGLrect& rect);
    void Rectangle(const MGLrect& rect);
    void FillPolygon(const MGLpoint *points, unsigned vertices);
    void Polygon(const MGLpoint *points, unsigned vertices);
    void Bevel(const MGLrect& rect, int width, int diff, bool is_raised);
    void Scroll(const MGLrect& rect, MGLreal dx, MGLreal dy);
};

class MGL_EXPORT RcDevice : public Rc
{
public:
    handle_t m_video, m_area;
    void *m_frame_buffer;
    videomode_t m_mode;

public:
    RcDevice(const wchar_t *video);
    RcDevice(handle_t video);
    ~RcDevice();

    bool SetMode(unsigned width, unsigned height, unsigned char bpp);

private:
    void Init();
};

};

#endif
