/* $Id: wmf.c,v 1.5 2002/04/20 12:47:28 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <gl/mgl.h>
#include <gl/wmf.h>

#undef WMF_FUNCTION
#define WMF_FUNCTION(name, code) void WMF_##name(wmf_t *wmf, const WMFRECORD *rec, uint16_t *params);

#include "wmfdefs.h"

//extern rgb_t palette[16 + 50];

#undef WMF_FUNCTION
#define WMF_FUNCTION(name, code)    { #name, WMF_##name, code },

static const struct
{
    const char *name;
    void (*func)(wmf_t *, const WMFRECORD *, uint16_t *);
    uint16_t code;
} wmf_functions[] = {
#include "wmfdefs.h"
};

/*MGLcolour WmfGetMglColour(uint32_t clr)
{
    unsigned i, best, dist, bestindex;
    uint8_t r, g, b;

    r = clr;
    g = clr >> 8;
    b = clr >> 16;
    best = UINT_MAX;
    bestindex = 0;
    for (i = 0; i < _countof(palette); i++)
    {
	dist = (palette[i].red - r) * (palette[i].red - r) +
	    (palette[i].green - g) * (palette[i].green - g) +
	    (palette[i].blue - b) * (palette[i].blue - b);
	if (dist < best)
	{
	    best = dist;
	    bestindex = i;
	}
    }

    return bestindex;
}*/

MGLcolour WmfGetMglColour(uint32_t clr)
{
    uint8_t *wclr;
    wclr = (uint8_t*) &clr;
    return MGL_COLOUR(wclr[0], wclr[1], wclr[2]);
}

wmf_t *WmfOpen(const wchar_t *name)
{
    wmf_t *wmf;
    handle_t file;
    dirent_t di;
    MGLrect dims;

    if (!FsQueryFile(name, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
	wprintf(L"%s: FsQueryFile failed\n", name);
	return NULL;
    }

    wmf = malloc(sizeof(wmf_t));
    if (wmf == NULL)
	return NULL;

    memset(wmf, 0, sizeof(wmf_t));
    file = FsOpen(name, FILE_READ);
    if (file == NULL)
    {
	WmfClose(wmf);
	return NULL;
    }

    wmf->data = malloc(di.length);
    if (wmf->data == NULL)
    {
	WmfClose(wmf);
	return NULL;
    }

    if (!FsReadSync(file, wmf->data, di.length, NULL))
    {
	WmfClose(wmf);
	return NULL;
    }

    wmf->stock_brush.type = wmfBrush;
    wmf->stock_brush.colour = WmfGetMglColour(0xffffff);
    wmf->stock_pen.type = wmfPen;
    wmf->stock_pen.style = PS_SOLID;
    wmf->stock_pen.width = 1;
    wmf->stock_pen.colour = 0;
    wmf->selected.ptr[wmfBrush] = &wmf->stock_brush;
    wmf->selected.ptr[wmfPen] = &wmf->stock_pen;
    mglGetDimensions(NULL, &dims);
    wmf->window_org.x = dims.left;
    wmf->window_org.y = dims.top;
    wmf->window_ext.x = dims.right;
    wmf->window_ext.y = dims.bottom;
    return wmf;
}

void WmfClose(wmf_t *wmf)
{
    unsigned i;

    for (i = 0; i < wmf->num_objects; i++)
	free(wmf->objects[i]);
    free(wmf->objects);
    free(wmf->data);
    free(wmf);
}

void WmfDraw(wmf_t *wmf, const MGLrect *dest)
{
    PLACEABLEMETAHEADER *pmh;
    WMFHEAD *wmfh;
    WMFRECORD *wmfr;
    unsigned i;
    
    wmf->dest_rect = *dest;
    pmh = wmf->data;
    if (pmh->Key == WMF_PLACEABLE_MAGIC)
	wmfh = (WMFHEAD*) (pmh + 1);
    else
	wmfh = wmf->data;

    /*printf(
	"          Magic: %lx\n"
	"         Handle: %u\n"
	"         Bounds: %d,%d,%d,%d\n"
	"            DPI: %u\n",
	pmh.Key, 
	pmh.Handle, 
	pmh.Left, pmh.Top, pmh.Right, pmh.Bottom,
	pmh.Inch);*/

    /*printf(
	"           Type: %u\n"
	"    Header size: %u\n"
	"        Version: %u\n"
	"      File size: %lu\n"
	"        Objects: %u\n"
	"Max record size: %lu\n",
	wmfh.FileType, wmfh.HeaderSize, wmfh.Version, 
	wmfh.FileSize, wmfh.NumOfObjects, wmfh.MaxRecordSize);*/

    /*start = ftell(f);
    while (fread(&wmfr, 1, sizeof(wmfr), f) == sizeof(wmfr))
    {
	str = "unknown";
	
	for (i = 0; i < _countof(wmf_functions); i++)
	    if (wmf_functions[i].code == wmfr.Function)
	    {
		str = wmf_functions[i].name;
		break;
	    }

	printf("%x: %s\t", wmfr.Function, str);
	rec = malloc(wmfr.Size * 2 - sizeof(wmfr));
	if (rec == NULL)
	    break;

	fread(rec, 1, wmfr.Size * 2 - sizeof(wmfr), f);
	free(rec);
    }*/

    wmfr = (WMFRECORD*) (wmfh + 1);
    while (wmfr->Function != 0)
    {
	for (i = 0; i < _countof(wmf_functions); i++)
	    if (wmf_functions[i].code == wmfr->Function)
	    	break;
	    
	if (i < _countof(wmf_functions))
	    wmf_functions[i].func(wmf, wmfr, (uint16_t*) (wmfr + 1));

	wmfr = (WMFRECORD*) ((uint16_t*) wmfr + wmfr->Size);
    }
}

unsigned WmfCreateObject(wmf_t *wmf, size_t size, enum WmfObject type)
{
    enum WmfObject *obj;
    unsigned i;

    obj = malloc(size);
    if (obj == NULL)
	return -1;

    *obj = type;
    for (i = 0; i < wmf->num_objects; i++)
	if (wmf->objects[i] == NULL)
	    break;

    if (i == wmf->num_objects)
	wmf->num_objects++;

    wmf->objects = realloc(wmf->objects, wmf->num_objects * sizeof(void*));
    wmf->objects[i] = obj;
    return i;
}

void WmfDeleteObject(wmf_t *wmf, unsigned num)
{
    if (num < wmf->num_objects)
    {
	free(wmf->objects[num]);
	wmf->objects[num] = NULL;
    }
}

void WmfMapPoint(wmf_t *wmf, int16_t x, int16_t y, MGLpoint *pt)
{
    pt->x = wmf->dest_rect.left 
        + (((MGLreal) x - wmf->window_org.x) * (wmf->dest_rect.right - wmf->dest_rect.left)) 
        / wmf->window_ext.x;
    pt->y = wmf->dest_rect.top 
        + (((MGLreal) y - wmf->window_org.y) * (wmf->dest_rect.bottom - wmf->dest_rect.top)) 
        / wmf->window_ext.y;
}

void WMF_SETBKCOLOR (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETBKMODE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETMAPMODE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETROP2 (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETRELABS (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETPOLYFILLMODE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETSTRETCHBLTMODE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETTEXTCHAREXTRA (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETTEXTCOLOR (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETTEXTJUSTIFICATION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 

void WMF_SETWINDOWORG (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    wmf->window_org.x = params[1];
    wmf->window_org.y = params[0];
}

void WMF_SETWINDOWEXT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    wmf->window_ext.x = params[1];
    wmf->window_ext.y = params[0];
}

void WMF_SETVIEWPORTORG (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
}

void WMF_SETVIEWPORTEXT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
}

void WMF_OFFSETWINDOWORG (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SCALEWINDOWEXT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_OFFSETVIEWPORTORG (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SCALEVIEWPORTEXT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_LINETO (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_MOVETO (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_EXCLUDECLIPRECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_INTERSECTCLIPRECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_ARC (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_ELLIPSE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_FLOODFILL (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_PIE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_RECTANGLE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_ROUNDRECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_PATBLT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SAVEDC (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETPIXEL (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_OFFSETCLIPRGN (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_TEXTOUT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_BITBLT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_STRETCHBLT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 

void WMF_POLYGON (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    uint16_t i;
    MGLpoint *pts;

    pts = malloc(params[0] * sizeof(MGLpoint));
    for (i = 0; i < params[0]; i++)
    	WmfMapPoint(wmf, params[i * 2 + 1], params[i * 2 + 2], pts + i);

    if (wmf->selected.o.brush->style != BS_NULL)
    {
	glSetColour(wmf->selected.o.brush->colour);
	glFillPolygon(pts, params[0]);
    }

    if (wmf->selected.o.pen->style != PS_NULL)
    {
	glSetColour(wmf->selected.o.pen->colour);
	glPolygon(pts, params[0]);
    }

    free(pts);
}

void WMF_POLYLINE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    MGLpoint pt;
    unsigned i;
    
    if (wmf->selected.o.pen->style != PS_NULL)
    {
	glSetColour(wmf->selected.o.pen->colour);
	for (i = 0; i < params[0]; i++)
	{
	    WmfMapPoint(wmf, params[i * 2 + 1], params[i * 2 + 2], &pt);

    	    if (i == 0)
		glMoveTo(pt.x, pt.y);
	    else
		glLineTo(pt.x, pt.y);
	}
    }
}

void WMF_ESCAPE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_RESTOREDC (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_FILLREGION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_FRAMEREGION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_INVERTREGION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_PAINTREGION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SELECTCLIPREGION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 

void WMF_SELECTOBJECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    if (params[0] < wmf->num_objects &&
	wmf->objects[params[0]] != NULL)
    {
	enum WmfObject *type;
	type = wmf->objects[params[0]];
	wmf->selected.ptr[*type] = wmf->objects[params[0]];
    }
} 

void WMF_SETTEXTALIGN (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_CHORD (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETMAPPERFLAGS (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_EXTTEXTOUT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETDIBTODEV (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SELECTPALETTE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_REALIZEPALETTE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_ANIMATEPALETTE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETPALENTRIES (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_POLYPOLYGON (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_RESIZEPALETTE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_DIBBITBLT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_DIBSTRETCHBLT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_DIBCREATEPATTERNBRUSH (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_STRETCHDIB (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_EXTFLOODFILL (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 
void WMF_SETLAYOUT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params) { } 

void WMF_DELETEOBJECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    WmfDeleteObject(wmf, params[0]);
}

void WMF_CREATEPALETTE (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    WmfCreateObject(wmf, 0, wmfPalette);
}

void WMF_CREATEPATTERNBRUSH (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    WmfCreateObject(wmf, 0, wmfBrush);
}

void WMF_CREATEPENINDIRECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    unsigned num;
    wmf_pen_t *pen;
    
    num = WmfCreateObject(wmf, sizeof(wmf_pen_t), wmfPen);
    if (num != -1)
    {
	pen = wmf->objects[num];
	pen->style = params[0];
	pen->width = params[1];
	pen->colour = WmfGetMglColour(*(uint32_t*) (params + 2));
    }
}

void WMF_CREATEFONTINDIRECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    WmfCreateObject(wmf, 0, wmfFont);
}

void WMF_CREATEBRUSHINDIRECT (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    unsigned num;
    wmf_brush_t *brush;

    num = WmfCreateObject(wmf, sizeof(wmf_brush_t), wmfBrush);
    if (num != -1)
    {
	brush = wmf->objects[num];
	brush->style = params[0];
	brush->colour = WmfGetMglColour(*(uint32_t*) (params + 1));
	brush->hatch = params[3];
    }
} 

void WMF_CREATEREGION (wmf_t *wmf, const WMFRECORD *rec, uint16_t *params)
{
    WmfCreateObject(wmf, 0, wmfRegion);
} 

