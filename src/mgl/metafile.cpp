/* $Id: metafile.cpp,v 1.2 2002/12/18 23:06:10 pavlovskii Exp $ */

#include <mgl/types.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <mgl/rc.h>
#include <mgl/metafile.h>

namespace mgl
{
#pragma pack(push, 2)
struct PLACEABLEMETAHEADER
{
    uint32_t Key;           /* Magic number (always 9AC6CDD7h) */
    uint16_t Handle;        /* Metafile HANDLE number (always 0) */
    int16_t  Left;          /* Left coordinate in metafile units */
    int16_t  Top;           /* Top coordinate in metafile units */
    int16_t  Right;         /* Right coordinate in metafile units */
    int16_t  Bottom;        /* Bottom coordinate in metafile units */
    uint16_t Inch;          /* Number of metafile units per inch */
    uint32_t Reserved;      /* Reserved (always 0) */
    uint16_t Checksum;      /* Checksum value for previous 10 words */
};

#define WMF_PLACEABLE_MAGIC 0x9AC6CDD7

struct WMFHEAD
{
    uint16_t FileType;       /* Type of metafile (0=memory, 1=disk) */
    uint16_t HeaderSize;     /* Size of header in words (always 9) */
    uint16_t Version;        /* Version of Microsoft Windows used */
    uint32_t FileSize;       /* Total size of the metafile in words */
    uint16_t NumOfObjects;   /* Number of objects in the file */
    uint32_t MaxRecordSize;  /* The size of largest record in words */
    uint16_t NumOfParams;    /* Not Used (always 0) */
};

struct WMFRECORD
{
    uint32_t Size;          /* Total size of the record in WORDs */
    uint16_t Function;      /* Function number (defined in WINDOWS.H) */
    /*uint16_t Parameters[];*/  /* Parameter values passed to function */
};
#pragma pack(pop)
};

using namespace mgl;

#undef WMF_FUNCTION
#define WMF_FUNCTION(name, code)    { #name, &Metafile::WMF_##name, code },

const Metafile::WmfFunction Metafile::m_functions[] = {
#include <mgl/wmfdefs.h>
};

MGLcolour Metafile::GetMglColour(uint32_t clr)
{
    uint8_t *wclr;
    wclr = (uint8_t*) &clr;
    return MGL_COLOUR(wclr[0], wclr[1], wclr[2]);
}

Metafile::Metafile()
{
    m_data = NULL;
    m_objects = NULL;
    m_num_objects = 0;
    m_file = NULL;
    wprintf(L"Metafile::Metafile\n");
}

Metafile::~Metafile()
{
    unsigned i;

    wprintf(L"Metafile::~Metafile: m_data = %p\n", m_data);
    for (i = 0; i < m_num_objects; i++)
	free(m_objects[i]);
    free(m_objects);

    if (m_data != NULL)
        VmmFree(m_data);
    HndClose(m_file);
}

bool Metafile::Open(const wchar_t *name)
{
    dirent_standard_t di;

    if (!FsQueryFile(name, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
	wprintf(L"%s: FsQueryFile failed\n", name);
	return NULL;
    }

    /*file = FsOpen(name, FILE_READ);
    if (file == NULL)
    	return false;

    m_data = malloc(di.length);
    if (m_data == NULL)
        return false;

    if (!FsReadSync(file, m_data, di.length, NULL))
    {
        free(m_data);
        return false;
    }

    FsClose(file);*/

    m_file = FsOpen(name, FILE_READ);
    if (m_file == NULL)
        return false;

    m_data = VmmMapFile(m_file, 
        NULL, 
        PAGE_ALIGN_UP(di.length) / PAGE_SIZE, 
        VM_MEM_USER | VM_MEM_READ);
    wprintf(L"Metafile::Open(%s): m_file = %u, length = %u, m_data = %p\n",
        name, m_file, (uint32_t) PAGE_ALIGN_UP(di.length), m_data);
    if (m_data == NULL)
        return false;

    m_stock_brush.type = wmfBrush;
    m_stock_brush.colour = GetMglColour(0xffffff);
    m_stock_pen.type = wmfPen;
    m_stock_pen.style = PS_SOLID;
    m_stock_pen.width = 1;
    m_stock_pen.colour = 0;
    m_selected.ptr[wmfBrush] = &m_stock_brush;
    m_selected.ptr[wmfPen] = &m_stock_pen;
    return true;
}

void Metafile::Draw(Rc *rc, const MGLrect& dest)
{
    PLACEABLEMETAHEADER *pmh;
    WMFHEAD *wmfh;
    WMFRECORD *wmfr;
    unsigned i;
    MGLrect dims;

    dims = rc->GetDimensions();
    m_window_org.x = dims.left;
    m_window_org.y = dims.top;
    m_window_ext.x = dims.right;
    m_window_ext.y = dims.bottom;

    m_dest_rect = dest;
    wprintf(L"Metafile::Draw: m_data = %p\n", m_data);
    pmh = (PLACEABLEMETAHEADER*) m_data;
    if (pmh->Key == WMF_PLACEABLE_MAGIC)
	wmfh = (WMFHEAD*) (pmh + 1);
    else
	wmfh = (WMFHEAD*) m_data;

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
	for (i = 0; i < _countof(m_functions); i++)
	    if (m_functions[i].code == wmfr->Function)
	    	break;

	if (i < _countof(m_functions))
	    (this->*m_functions[i].func)(rc, wmfr, (uint16_t*) (wmfr + 1));

	wmfr = (WMFRECORD*) ((uint16_t*) wmfr + wmfr->Size);
    }
}

unsigned Metafile::CreateObject(size_t size, enum WmfObject type)
{
    enum WmfObject *obj;
    unsigned i;

    obj = (enum WmfObject*) malloc(size);
    if (obj == NULL)
	return (unsigned) -1;

    *obj = type;
    for (i = 0; i < m_num_objects; i++)
	if (m_objects[i] == NULL)
	    break;

    if (i == m_num_objects)
	m_num_objects++;

    m_objects = (void**) realloc(m_objects, m_num_objects * sizeof(void*));
    m_objects[i] = obj;
    return i;
}

void Metafile::DeleteObject(unsigned num)
{
    if (num < m_num_objects)
    {
	free(m_objects[num]);
	m_objects[num] = NULL;
    }
}

void Metafile::MapPoint(int16_t x, int16_t y, MGLpoint *pt)
{
    pt->x = m_dest_rect.left 
        + (((MGLreal) x - m_window_org.x) * (m_dest_rect.right - m_dest_rect.left)) 
        / m_window_ext.x;
    pt->y = m_dest_rect.top 
        + (((MGLreal) y - m_window_org.y) * (m_dest_rect.bottom - m_dest_rect.top)) 
        / m_window_ext.y;
}

void Metafile::WMF_SETBKCOLOR (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETBKMODE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETMAPMODE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETROP2 (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETRELABS (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETPOLYFILLMODE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETSTRETCHBLTMODE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETTEXTCHAREXTRA (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETTEXTCOLOR (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETTEXTJUSTIFICATION (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 

void Metafile::WMF_SETWINDOWORG (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    m_window_org.x = params[1];
    m_window_org.y = params[0];
}

void Metafile::WMF_SETWINDOWEXT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    m_window_ext.x = params[1];
    m_window_ext.y = params[0];
}

void Metafile::WMF_SETVIEWPORTORG (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
}

void Metafile::WMF_SETVIEWPORTEXT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
}

void Metafile::WMF_OFFSETWINDOWORG (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SCALEWINDOWEXT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_OFFSETVIEWPORTORG (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SCALEVIEWPORTEXT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_LINETO (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_MOVETO (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_EXCLUDECLIPRECT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_INTERSECTCLIPRECT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_ARC (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_ELLIPSE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_FLOODFILL (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_PIE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_RECTANGLE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_ROUNDRECT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_PATBLT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SAVEDC (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETPIXEL (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_OFFSETCLIPRGN (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_TEXTOUT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_BITBLT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_STRETCHBLT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 

void Metafile::WMF_POLYGON (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    uint16_t i;
    MGLpoint *pts;

    pts = new MGLpoint[params[0]];
    for (i = 0; i < params[0]; i++)
        MapPoint(params[i * 2 + 1], params[i * 2 + 2], pts + i);

    if (m_selected.o.brush->style != BS_NULL)
    {
        rc->SetFillColour(m_selected.o.brush->colour);
        rc->FillPolygon(pts, params[0]);
    }

    if (m_selected.o.pen->style != PS_NULL)
    {
        rc->SetPenColour(m_selected.o.pen->colour);
        rc->Polygon(pts, params[0]);
    }

    delete[] pts;
}

void Metafile::WMF_POLYLINE (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    MGLpoint pt, prev;
    unsigned i;
    
    if (m_selected.o.pen->style != PS_NULL)
    {
        rc->SetPenColour(m_selected.o.pen->colour);
        for (i = 0; i < params[0]; i++)
        {
            MapPoint(params[i * 2 + 1], params[i * 2 + 2], &pt);

            if (i == 0)
                prev = pt;
            else
                rc->Line(prev.x, prev.y, pt.x, pt.y);
        }
    }
}

void Metafile::WMF_ESCAPE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_RESTOREDC (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_FILLREGION (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_FRAMEREGION (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_INVERTREGION (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_PAINTREGION (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SELECTCLIPREGION (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 

void Metafile::WMF_SELECTOBJECT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    if (params[0] < m_num_objects &&
	m_objects[params[0]] != NULL)
    {
	enum WmfObject *type;
	type = (enum WmfObject*) m_objects[params[0]];
	m_selected.ptr[*type] = m_objects[params[0]];
    }
} 

void Metafile::WMF_SETTEXTALIGN (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_CHORD (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETMAPPERFLAGS (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_EXTTEXTOUT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETDIBTODEV (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SELECTPALETTE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_REALIZEPALETTE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_ANIMATEPALETTE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETPALENTRIES (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_POLYPOLYGON (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_RESIZEPALETTE (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_DIBBITBLT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_DIBSTRETCHBLT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_DIBCREATEPATTERNBRUSH (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_STRETCHDIB (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_EXTFLOODFILL (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 
void Metafile::WMF_SETLAYOUT (Rc *rc, const WMFRECORD *rec, uint16_t *params) { } 

void Metafile::WMF_DELETEOBJECT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    DeleteObject(params[0]);
}

void Metafile::WMF_CREATEPALETTE (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    CreateObject(0, wmfPalette);
}

void Metafile::WMF_CREATEPATTERNBRUSH (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    CreateObject(0, wmfBrush);
}

void Metafile::WMF_CREATEPENINDIRECT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    unsigned num;
    wmf_pen_t *pen;
    
    num = CreateObject(sizeof(wmf_pen_t), wmfPen);
    if (num != (unsigned) -1)
    {
	pen = (wmf_pen_t*) m_objects[num];
	pen->style = params[0];
	pen->width = params[1];
	pen->colour = GetMglColour(*(uint32_t*) (params + 2));
    }
}

void Metafile::WMF_CREATEFONTINDIRECT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    CreateObject(0, wmfFont);
}

void Metafile::WMF_CREATEBRUSHINDIRECT (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    unsigned num;
    wmf_brush_t *brush;

    num = CreateObject(sizeof(wmf_brush_t), wmfBrush);
    if (num != (unsigned) -1)
    {
	brush = (wmf_brush_t*) m_objects[num];
	brush->style = params[0];
	brush->colour = GetMglColour(*(uint32_t*) (params + 1));
	brush->hatch = params[3];
    }
} 

void Metafile::WMF_CREATEREGION (Rc *rc, const WMFRECORD *rec, uint16_t *params)
{
    CreateObject(0, wmfRegion);
} 

