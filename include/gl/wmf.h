/* $Id: wmf.h,v 1.3 2002/04/20 12:34:38 pavlovskii Exp $ */

#ifndef WMF_H__
#define WMF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

#pragma pack(push,2)

typedef struct _PlaceableMetaHeader
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
} PLACEABLEMETAHEADER;

#define WMF_PLACEABLE_MAGIC 0x9AC6CDD7

typedef struct _WindowsMetaHeader
{
  uint16_t FileType;       /* Type of metafile (0=memory, 1=disk) */
  uint16_t HeaderSize;     /* Size of header in words (always 9) */
  uint16_t Version;        /* Version of Microsoft Windows used */
  uint32_t FileSize;       /* Total size of the metafile in words */
  uint16_t NumOfObjects;   /* Number of objects in the file */
  uint32_t MaxRecordSize;  /* The size of largest record in words */
  uint16_t NumOfParams;    /* Not Used (always 0) */
} WMFHEAD;

typedef struct _StandardMetaRecord
{
    uint32_t Size;          /* Total size of the record in WORDs */
    uint16_t Function;      /* Function number (defined in WINDOWS.H) */
    /*uint16_t Parameters[];*/  /* Parameter values passed to function */
} WMFRECORD;

#pragma pack(pop)

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

typedef struct wmf_brush_t wmf_brush_t;
struct wmf_brush_t
{
    enum WmfObject type;
    unsigned style;
    MGLcolour colour;
    long hatch;
};

typedef struct wmf_pen_t wmf_pen_t;
struct wmf_pen_t
{
    enum WmfObject type;
    unsigned style;
    MGLreal width;
    MGLcolour colour;
};

typedef struct wmf_font_t wmf_font_t;
typedef struct wmf_region_t wmf_region_t;
typedef struct wmf_palette_t wmf_palette_t;

typedef struct wmf_t wmf_t;
struct wmf_t
{
    union
    {
	void *ptr[4];
	struct
	{
	    wmf_brush_t *brush;
	    wmf_pen_t *pen;
	    wmf_font_t *font;
	    wmf_region_t *region;
	    wmf_palette_t *palette;
	} o;
    } selected;

    void **objects;
    unsigned num_objects;
    wmf_brush_t stock_brush;
    wmf_pen_t stock_pen;
    void *data;
    MGLpoint window_org, window_ext;
    MGLrect dest_rect;
};

wmf_t *WmfOpen(const wchar_t *name);
void WmfClose(wmf_t *wmf);
void WmfDraw(wmf_t *wmf, const MGLrect *dest);

#ifdef __cplusplus
}
#endif

#endif
