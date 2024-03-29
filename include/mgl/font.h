/* $Id: font.h,v 1.1.1.1 2002/12/31 01:26:21 pavlovskii Exp $ */

#ifndef __MGL_FONT_H
#define __MGL_FONT_H

#include "types.h"
#include "refcount.h"

struct FT_FaceRec_;
struct FT_Bitmap_;
struct point_t;

namespace mgl
{

class Rc;

struct FontMetrics
{
    MGLreal char_width_max;
    MGLreal char_width_min;
    MGLreal height;
    MGLreal ascent;
    MGLreal descent;
};

class MGL_EXPORT Font : public RefCount<Font>
{
protected:
    FT_FaceRec_ *m_ft_face;
    void *m_font_data;
    handle_t m_file;

    void IterateString(Rc *rc, const MGLrect& rect, const wchar_t *str, 
        int len, point_t *size, 
        void (*fn)(Rc *, FT_Bitmap_ *, int, int, MGLcolour, MGLcolour));

public:
    Font(const wchar_t *filename, unsigned size_twips);
    virtual ~Font();

    MGLpoint GetTextSize(Rc *rc, const wchar_t *str, int len = -1);
    void DrawText(Rc *rc, const MGLrect& rect, const wchar_t *str, int len = -1);
    void GetMetrics(Rc *rc, FontMetrics *m);
};

};

#endif
