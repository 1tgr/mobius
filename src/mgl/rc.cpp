/* $Id: rc.cpp,v 1.1 2002/09/13 23:23:01 pavlovskii Exp $ */

#include <mgl/rc.h>

#include <stdlib.h>
#include <limits.h>
#include <stddef.h>

#include "bpp8.h"

using namespace mgl;

Rc::Rc(Bitmap *bitmap)
{
    m_bitmap = NULL;
    m_width = 1280;
    m_height = 960;
    m_clip_rects = NULL;

    if (bitmap != NULL)
        Attach(bitmap);
}

Rc::~Rc()
{
    Attach(NULL);
}

void Rc::Attach(Bitmap *bitmap)
{
    if (m_bitmap != NULL)
    {
        m_bitmap->Release();
        m_bitmap = NULL;
    }

    if (m_clip_rects != NULL)
    {
        delete[] m_clip_rects;
        m_num_clip_rects = 0;
        m_clip_rects = NULL;
    }

    m_bitmap = bitmap;

    if (m_bitmap != NULL)
    {
        m_bitmap->Retain();
        
        m_num_clip_rects = 1;
        m_clip_rects = new rect_t[1];
        m_clip_rects[0].left = m_clip_rects[0].top = 0;
        m_clip_rects[0].right = m_bitmap->GetWidth();
        m_clip_rects[0].bottom = m_bitmap->GetHeight();

        DoLine = &Rc::LineGeneric;
        DoFillRect = &Rc::FillRectGeneric;
        switch (m_bitmap->GetColourDepth())
        {
        case 8:
            DoHLine = &Rc::HLine8;
            DoVLine = &Rc::VLine8;
            DoPutPixel = &Rc::PutPixel8;
            DoGetPixel = &Rc::GetPixel8;
            break;

        case 16:
            DoHLine = &Rc::HLine16;
            DoVLine = &Rc::VLine16;
            DoPutPixel = &Rc::PutPixel16;
            DoGetPixel = &Rc::GetPixel16;
            break;
        }
    }
}

/*
 * Generic primitives
 */

void Rc::LineGeneric(int x1, int y1, int x2, int y2, MGLcolour clr)
{
    if (x1 == x2)
        (this->*DoVLine)(x1, y1, y2, clr);
    else if (y1 == y2)
        (this->*DoHLine)(x1, x2, y1, clr);
    else
    {
        /*
         * Line code from Allegro gfx.c
         */

        int dx = x2-x1;
        int dy = y2-y1;
        int i1, i2;
        int x, y;
        int dd;

    /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)    \
    {                                                                    \
        if (d##pri_c == 0) {                                             \
            (this->*DoPutPixel)(x1, y1, clr);                            \
            return;                                                      \
        }                                                                \
        \
        i1 = 2 * d##sec_c;                                               \
        dd = i1 - (sec_sign (pri_sign d##pri_c));                        \
        i2 = dd - (sec_sign (pri_sign d##pri_c));                        \
        \
        x = x1;                                                          \
        y = y1;                                                          \
        \
        while (pri_c pri_cond pri_c##2) {                                \
            (this->*DoPutPixel)(x, y, clr);                              \
            if (dd sec_cond 0) {                                         \
                sec_c sec_sign##= 1;                                     \
                dd += i2;                                                \
            }                                                            \
            else                                                         \
                dd += i1;                                                \
            \
            pri_c pri_sign##= 1;                                         \
        }                                                                \
    }

        if (dx >= 0) {
            if (dy >= 0) {
                if (dx >= dy) {
                    /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
                    DO_LINE(+, x, <=, +, y, >=);
                }
                else {
                    /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
                    DO_LINE(+, y, <=, +, x, >=);
                }
            }
            else {
                if (dx >= -dy) {
                    /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
                    DO_LINE(+, x, <=, -, y, <=);
                }
                else {
                    /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
                    DO_LINE(-, y, >=, +, x, >=);
                }
            }
        }
        else {
            if (dy >= 0) {
                if (-dx >= dy) {
                    /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
                    DO_LINE(-, x, >=, +, y, >=);
                }
                else {
                    /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
                    DO_LINE(+, y, <=, -, x, <=);
                }
            }
            else {
                if (-dx >= -dy) {
                    /* (x1 > x2) && (y1 > y2) && (dx >= dy) */
                    DO_LINE(-, x, >=, -, y, <=);
                }
                else {
                    /* (x1 > x2) && (y1 > y2) && (dx < dy) */
                    DO_LINE(-, y, >=, -, x, <=);
                }
            }
        }
    }
}

void Rc::FillRectGeneric(const rect_t& rect, MGLcolour clr)
{
    int y;
    for (y = rect.top; y < rect.bottom; y++)
        (this->*DoHLine)(rect.left, rect.right, y, clr);
}

/*
 * 8bpp primitives
 */

void Rc::HLine8(int x1, int x2, int y, MGLcolour clr)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint8_t *ptr;
    unsigned i;
    int ax1, ax2;

    if (x2 < x1)
    {
        int t;
        t = x2;
        x2 = x1;
        x1 = t;
    }

    for (i = 0; i < m_num_clip_rects; i++)
    {
        if (y >= m_clip_rects[i].top && y < m_clip_rects[i].bottom)
        {
            ax1 = max(m_clip_rects[i].left, x1);
            ax2 = min(m_clip_rects[i].right, x2);

            for (ptr = video_base + ax1 + y * m_bitmap->GetPitch();
                ax1 < ax2;
                ax1++, ptr++)
                *ptr = bpp8Dither(ax1, y, clr);
        }
    }
}

void Rc::VLine8(int x, int y1, int y2, MGLcolour clr)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint8_t *ptr;
    unsigned i;
    int ay1, ay2;

    if (y2 < y1)
    {
        int t;
        t = y2;
        y2 = y1;
        y1 = t;
    }

    for (i = 0; i < m_num_clip_rects; i++)
    {
        if (x >= m_clip_rects[i].left && x < m_clip_rects[i].right)
        {
            ay1 = max(m_clip_rects[i].top, y1);
            ay2 = min(m_clip_rects[i].bottom, y2);

            for (ptr = video_base + x + ay1 * m_bitmap->GetPitch();
                ay1 < ay2;
                ay1++, ptr += m_bitmap->GetPitch())
                *ptr = bpp8Dither(x, ay1, clr);
        }
    }
}

void Rc::PutPixel8(int x, int y, MGLcolour clr)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    unsigned i;

    for (i = 0; i < m_num_clip_rects; i++)
        if (x >= m_clip_rects[i].left &&
            y >= m_clip_rects[i].top &&
            x <  m_clip_rects[i].right &&
            y <  m_clip_rects[i].bottom)
        {
            video_base[x + y * m_bitmap->GetPitch()] = bpp8Dither(x, y, clr);
            break;
        }
}

MGLcolour Rc::GetPixel8(int x, int y)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint8_t pix;

    pix = video_base[x + y * m_bitmap->GetPitch()];
    return MGL_COLOUR(bpp8_palette[pix].red,
        bpp8_palette[pix].green,
        bpp8_palette[pix].blue);
}

/*
 * 16bpp primitives
 */

static inline uint16_t ColourToPixel16(MGLcolour clr)
{
    uint8_t r, g, b;
    r = MGL_RED(clr);
    g = MGL_GREEN(clr);
    b = MGL_BLUE(clr);
    /* 5-6-5 R-G-B */
    return ((r >> 3) << 11) |
           ((g >> 2) << 5) |
           ((b >> 3) << 0);
}

static inline MGLcolour Pixel16ToColour(uint16_t pix)
{
    uint8_t r, g, b;
    r = (pix >> 11) << 3;
    g = (pix >> 5) << 2;
    b = (pix >> 0) << 3;
    /* 5-6-5 R-G-B */
    return MGL_COLOUR(r, g, b);
}

void Rc::HLine16(int x1, int x2, int y, MGLcolour clr)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint16_t *start, *ptr;
    unsigned i;
    int ax1, ax2;

    if (x2 < x1)
    {
        int t;
        t = x2;
        x2 = x1;
        x1 = t;
    }

    clr = ColourToPixel16(clr);
    start = (uint16_t*) (video_base + y * m_bitmap->GetPitch());
    for (i = 0; i < m_num_clip_rects; i++)
    {
        if (y >= m_clip_rects[i].top && y < m_clip_rects[i].bottom)
        {
            ax1 = max(m_clip_rects[i].left, x1);
            ax2 = min(m_clip_rects[i].right, x2);

            for (ptr = start + ax1;
                ax1 < ax2;
                ax1++, ptr++)
                *ptr = clr;
        }
    }
}

void Rc::VLine16(int x, int y1, int y2, MGLcolour clr)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint16_t *ptr;
    unsigned i;
    int ay1, ay2;

    if (y2 < y1)
    {
        int t;
        t = y2;
        y2 = y1;
        y1 = t;
    }

    clr = ColourToPixel16(clr);
    for (i = 0; i < m_num_clip_rects; i++)
    {
        if (x >= m_clip_rects[i].left && x < m_clip_rects[i].right)
        {
            ay1 = max(m_clip_rects[i].top, y1);
            ay2 = min(m_clip_rects[i].bottom, y2);

            for (ptr = (uint16_t*) (video_base + ay1 * m_bitmap->GetPitch()) + x;
                ay1 < ay2;
                ay1++, ptr += m_bitmap->GetPitch() / 2)
                *ptr = clr;
        }
    }
}

void Rc::PutPixel16(int x, int y, MGLcolour clr)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint16_t *ptr;
    unsigned i;

    for (i = 0; i < m_num_clip_rects; i++)
        if (x >= m_clip_rects[i].left &&
            y >= m_clip_rects[i].top &&
            x <  m_clip_rects[i].right &&
            y <  m_clip_rects[i].bottom)
        {
            ptr = (uint16_t*) (video_base + y * m_bitmap->GetPitch());
            ptr[x] = ColourToPixel16(clr);
            break;
        }
}

MGLcolour Rc::GetPixel16(int x, int y)
{
    BitmapLock<uint8_t*> video_base(m_bitmap);
    uint16_t *ptr;
    ptr = (uint16_t*) (video_base + y * m_bitmap->GetPitch());
    return Pixel16ToColour(ptr[x]);
}

/*
 * Co-ordinate mapping functions
 */

void Rc::Transform(MGLreal x, MGLreal y, point_t& pt) const
{
    pt.x = (int) (x * m_bitmap->GetWidth()) / m_width;
    pt.y = (int) (y * m_bitmap->GetHeight()) / m_height;
}

void Rc::Transform(const MGLrect& a, rect_t& b) const
{
    b.left =   (int) (a.left   * m_bitmap->GetWidth()) / m_width;
    b.top =    (int) (a.top    * m_bitmap->GetHeight()) / m_height;
    b.right =  (int) (a.right  * m_bitmap->GetWidth()) / m_width;
    b.bottom = (int) (a.bottom * m_bitmap->GetHeight()) / m_height;
}

void Rc::InverseTransform(int x, int y, MGLpoint& pt) const
{
    pt.x = ((MGLreal) x * m_width) / m_bitmap->GetWidth();
    pt.y = ((MGLreal) y * m_height) / m_bitmap->GetHeight();
}

void Rc::InverseTransform(const rect_t& a, MGLrect& b) const
{
    b.left =   ((MGLreal) a.left   * m_width) / m_bitmap->GetWidth();
    b.top =    ((MGLreal) a.top    * m_height) / m_bitmap->GetHeight();
    b.right =  ((MGLreal) a.right  * m_width) / m_bitmap->GetWidth();
    b.bottom = ((MGLreal) a.bottom * m_height) / m_bitmap->GetHeight();
}

void Rc::SetClip(const MGLclip& clip)
{
    unsigned i;

    if (m_clip_rects != NULL)
        delete[] m_clip_rects;

    m_num_clip_rects = clip.num_rects;
    m_clip_rects = new rect_t[m_num_clip_rects];

    for (i = 0; i < m_num_clip_rects; i++)
        Transform(clip.rects[i], m_clip_rects[i]);
}

/*
 * Public rendering functions
 */

void Rc::Line(MGLreal x1, MGLreal y1, MGLreal x2, MGLreal y2)
{
    point_t a, b;
    Transform(x1, y1, a);
    Transform(x2, y2, b);
    (this->*DoLine)(a.x, a.y, b.x, b.y, m_state.colour_pen);
}

void Rc::FillRect(const MGLrect& rect)
{
    rect_t r;
    Transform(rect, r);
    (this->*DoFillRect)(r, m_state.colour_fill);
}

void Rc::Rectangle(const MGLrect& rect)
{
    rect_t r;
    Transform(rect, r);
    (this->*DoHLine)(r.left, r.right, r.top, m_state.colour_pen);
    (this->*DoHLine)(r.left, r.right, r.bottom, m_state.colour_pen);
    (this->*DoVLine)(r.left, r.top, r.bottom, m_state.colour_pen);
    (this->*DoVLine)(r.right, r.top, r.bottom, m_state.colour_pen);
}

/* information for polygon scanline fillers */
typedef struct POLYGON_SEGMENT
{
    int u, v, du, dv;                 /* fixed point u/v coordinates */
    int c, dc;                          /* single color gouraud shade values */
    int r, g, b, dr, dg, db;        /* RGB gouraud shade values */
    float z, dz;                /* polygon depth (1/z) */
    float fu, fv, dfu, dfv;           /* floating point u/v coordinates */
    unsigned char *texture;           /* the texture map */
    int umask, vmask, vshift;         /* texture map size information */
    int seg;                        /* destination bitmap selector */
} POLYGON_SEGMENT;


/* an active polygon edge */
typedef struct POLYGON_EDGE 
{
    int top;                        /* top y position */
    int bottom;                    /* bottom y position */
    int x, dx;                          /* fixed point x position and gradient */
    int w;                          /* width of line segment */
    POLYGON_SEGMENT dat;        /* texture/gouraud information */
    struct POLYGON_EDGE *prev;          /* doubly linked list */
    struct POLYGON_EDGE *next;
} POLYGON_EDGE;

#define POLYGON_FIX_SHIFT         18

/* fill_edge_structure:
*  Polygon helper function: initialises an edge structure for the 2d
*  rasteriser.
*/
static void fill_edge_structure(POLYGON_EDGE *edge, const point_t *p1, const point_t *p2)
{
    if (p2->y < p1->y) {
        const point_t *it;
        
        it = p1;
        p1 = p2;
        p2 = it;
    }
    
    edge->top = p1->y;
    edge->bottom = p2->y - 1;
    edge->dx = ((p2->x - p1->x) << POLYGON_FIX_SHIFT) / (p2->y - p1->y);
    edge->x = (p1->x << POLYGON_FIX_SHIFT) + (1<<(POLYGON_FIX_SHIFT-1)) - 1;
    edge->prev = NULL;
    edge->next = NULL;
    
    if (edge->dx < 0)
        edge->x += min(edge->dx+(1<<POLYGON_FIX_SHIFT), 0);
    
    edge->w = max(abs(edge->dx)-(1<<POLYGON_FIX_SHIFT), 0);
}



/* _add_edge:
*  Adds an edge structure to a linked list, returning the new head pointer.
*/
static POLYGON_EDGE *_add_edge(POLYGON_EDGE *list, POLYGON_EDGE *edge, int sort_by_x)
{
    POLYGON_EDGE *pos = list;
    POLYGON_EDGE *prev = NULL;
    
    if (sort_by_x) {
        while ((pos) && ((pos->x + (pos->w + pos->dx) / 2) < 
            (edge->x + (edge->w + edge->dx) / 2))) {
            prev = pos;
            pos = pos->next;
        }
    }
    else {
        while ((pos) && (pos->top < edge->top)) {
            prev = pos;
            pos = pos->next;
        }
    }
    
    edge->next = pos;
    edge->prev = prev;
    
    if (pos)
        pos->prev = edge;
    
    if (prev) {
        prev->next = edge;
        return list;
    }
    else
        return edge;
}

/* _remove_edge:
*  Removes an edge structure from a list, returning the new head pointer.
*/
static POLYGON_EDGE *_remove_edge(POLYGON_EDGE *list, POLYGON_EDGE *edge)
{
    if (edge->next) 
        edge->next->prev = edge->prev;
    
    if (edge->prev) {
        edge->prev->next = edge->next;
        return list;
    }
    else
        return edge->next;
}

static void *_scratch_mem;
static int _scratch_mem_size;

static void _grow_scratch_mem(int size)
{
    if (size > _scratch_mem_size) {
        size = (size+1023) & 0xFFFFFC00;
        _scratch_mem = realloc(_scratch_mem, size);
        _scratch_mem_size = size;
    }
}

/* polygon:
*  Draws a filled polygon with an arbitrary number of corners. Pass the 
*  number of vertices, then an array containing a series of x, y points 
*  (a total of vertices*2 values).
*/
void Rc::FillPolygon(const MGLpoint *points, unsigned vertices)
{
    int c;
    int top = INT_MAX;
    int bottom = INT_MIN;
    const MGLpoint *i1, *i2;
    POLYGON_EDGE *edge, *next_edge;
    POLYGON_EDGE *active_edges = NULL;
    POLYGON_EDGE *inactive_edges = NULL;
    point_t p1, p2;
    
    /* allocate some space and fill the edge table */
    _grow_scratch_mem(sizeof(POLYGON_EDGE) * vertices);
    
    edge = (POLYGON_EDGE*) _scratch_mem;
    i1 = points;
    i2 = points + vertices - 1;
    
    for (c=0; c < (int) vertices; c++)
    {
        Transform(i1->x, i1->y, p1);
        Transform(i2->x, i2->y, p2);
        if (p1.y != p2.y)
        {
            fill_edge_structure(edge, &p1, &p2);
            
            if (edge->bottom >= edge->top) {
                
                if (edge->top < top)
                    top = edge->top;
                
                if (edge->bottom > bottom)
                    bottom = edge->bottom;
                
                inactive_edges = _add_edge(inactive_edges, edge, false);
                edge++;
            }
        }
        i2 = i1;
        i1++;
    }
    
    /*if (bottom >= (int) ddsd->dwHeight)
    bottom = ddsd->dwHeight - 1;*/
    
    /* for each scanline in the polygon... */
    for (c=top; c<=bottom; c++) {
        
        /* check for newly active edges */
        edge = inactive_edges;
        while ((edge) && (edge->top == c)) {
            next_edge = edge->next;
            inactive_edges = _remove_edge(inactive_edges, edge);
            active_edges = _add_edge(active_edges, edge, true);
            edge = next_edge;
        }
        
        /* draw horizontal line segments */
        edge = active_edges;
        while ((edge) && (edge->next)) {
            (this->*DoHLine)(edge->x>>POLYGON_FIX_SHIFT,
                (edge->next->x+edge->next->w)>>POLYGON_FIX_SHIFT, 
                c, 
                m_state.colour_fill);
            edge = edge->next->next;
        }
        
        /* update edges, sorting and removing dead ones */
        edge = active_edges;
        while (edge) {
            next_edge = edge->next;
            if (c >= edge->bottom) {
                active_edges = _remove_edge(active_edges, edge);
            }
            else {
                edge->x += edge->dx;
                while ((edge->prev) && 
                    (edge->x+edge->w/2 < edge->prev->x+edge->prev->w/2)) {
                    if (edge->next)
                        edge->next->prev = edge->prev;
                    edge->prev->next = edge->next;
                    edge->next = edge->prev;
                    edge->prev = edge->prev->prev;
                    edge->next->prev = edge;
                    if (edge->prev)
                        edge->prev->next = edge;
                    else
                        active_edges = edge;
                }
            }
            edge = next_edge;
        }
    }
}

void Rc::Polygon(const MGLpoint *points, unsigned vertices)
{
    unsigned i;
    
    for (i = 0; i < vertices - 1; i++)
        Line(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);

    if (vertices > 0)
        Line(points[i].x, points[i].y, points[0].x, points[0].y);
}

MGLcolour ClrLighter(MGLcolour clr, unsigned char diff)
{
    int r, g, b;
    r = MGL_RED(clr) + diff;
    g = MGL_GREEN(clr) + diff;
    b = MGL_BLUE(clr) + diff;
    return MGL_COLOUR(min(r, 255), min(g, 255), min(b, 255));
}

MGLcolour ClrDarker(MGLcolour clr, unsigned char diff)
{
    int r, g, b;
    r = MGL_RED(clr) - diff;
    g = MGL_GREEN(clr) - diff;
    b = MGL_BLUE(clr) - diff;
    return MGL_COLOUR(max(r, 0), max(g, 0), max(b, 0));
}

void Rc::Bevel(const MGLrect& rect, int width, int diff, bool is_raised)
{
    MGLpoint pts[6];
    MGLcolour old_clr;

    old_clr = m_state.colour_fill;
    m_state.colour_fill = is_raised 
        ? ClrLighter(old_clr, diff) 
        : ClrDarker(old_clr, diff);

    pts[0].x = rect.right; pts[0].y = rect.top;
    pts[1].x = rect.left; pts[1].y = rect.top;
    pts[2].x = rect.left; pts[2].y = rect.bottom;
    pts[3].x = rect.left + width; pts[3].y = rect.bottom - width;
    pts[4].x = rect.left + width; pts[4].y = rect.top + width;
    pts[5].x = rect.right - width; pts[5].y = rect.top + width;
    FillPolygon(pts, _countof(pts));

    m_state.colour_fill = is_raised 
        ? ClrDarker(old_clr, diff) 
        : ClrLighter(old_clr, diff);
    pts[0].x = rect.right; pts[0].y = rect.top;
    pts[1].x = rect.right; pts[1].y = rect.bottom;
    pts[2].x = rect.left; pts[2].y = rect.bottom;
    pts[3].x = rect.left + width; pts[3].y = rect.bottom - width;
    pts[4].x = rect.right - width; pts[4].y = rect.bottom - width;
    pts[5].x = rect.right - width; pts[5].y = rect.top + width;
    FillPolygon(pts, _countof(pts));

    m_state.colour_fill = old_clr;
}
