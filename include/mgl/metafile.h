/* $Id: metafile.h,v 1.1.1.1 2002/12/31 01:26:22 pavlovskii Exp $ */

#ifndef __MGL_METAFILE_H
#define __MGL_METAFILE_H

#include "types.h"

namespace mgl
{

/* Brush Styles */
#define BS_SOLID            0
#define BS_NULL             1
#define BS_HOLLOW           BS_NULL
#define BS_HATCHED          2
#define BS_PATTERN          3
#define BS_INDEXED          4
#define BS_DIBPATTERN       5
#define BS_DIBPATTERNPT     6
#define BS_PATTERN8X8       7
#define BS_DIBPATTERN8X8    8
#define BS_MONOPATTERN      9

/* Hatch Styles */
#define HS_HORIZONTAL       0       /* ----- */
#define HS_VERTICAL         1       /* ||||| */
#define HS_FDIAGONAL        2       /* \\\\\ */
#define HS_BDIAGONAL        3       /* ///// */
#define HS_CROSS            4       /* +++++ */
#define HS_DIAGCROSS        5       /* xxxxx */

/* Pen Styles */
#define PS_SOLID            0
#define PS_DASH             1       /* -------  */
#define PS_DOT              2       /* .......  */
#define PS_DASHDOT          3       /* _._._._  */
#define PS_DASHDOTDOT       4       /* _.._.._  */
#define PS_NULL             5
#define PS_INSIDEFRAME      6
#define PS_USERSTYLE        7
#define PS_ALTERNATE        8

enum WmfObject
{
    wmfBrush,
    wmfPen,
    wmfFont,
    wmfRegion,
    wmfPalette,
};

struct wmf_brush_t
{
    enum WmfObject type;
    unsigned style;
    MGLcolour colour;
    long hatch;
};

struct wmf_pen_t
{
    enum WmfObject type;
    unsigned style;
    MGLreal width;
    MGLcolour colour;
};

struct wmf_font_t;
struct wmf_region_t;
struct wmf_palette_t;
struct WMFRECORD;

class Rc;

class Metafile
{
protected:
    union
    {
	void *ptr[5];
	struct
	{
	    wmf_brush_t *brush;
	    wmf_pen_t *pen;
	    wmf_font_t *font;
	    wmf_region_t *region;
	    wmf_palette_t *palette;
	} o;
    } m_selected;

    void **m_objects;
    unsigned m_num_objects;
    wmf_brush_t m_stock_brush;
    wmf_pen_t m_stock_pen;
    void *m_data;
    handle_t m_file;
    MGLpoint m_window_org, m_window_ext;
    MGLrect m_dest_rect;

    static const struct WmfFunction
    {
        const char *name;
        void (Metafile::*func)(Rc *rc, const WMFRECORD *, uint16_t *);
        uint16_t code;
    } m_functions[];

    static MGLcolour GetMglColour(uint32_t clr);
    unsigned CreateObject(size_t size, enum WmfObject type);
    void DeleteObject(unsigned num);
    void MapPoint(int16_t x, int16_t y, MGLpoint *pt);

#undef WMF_FUNCTION
#define WMF_FUNCTION(name, code) void WMF_##name(Rc *rc, const WMFRECORD *rec, uint16_t *params);

#include "wmfdefs.h"

public:
    Metafile();
    ~Metafile();
    
    bool Open(const wchar_t *name);
    void Draw(Rc *rc, const MGLrect& dest);
};

};

#endif
