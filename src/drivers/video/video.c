/* $Id: video.c,v 1.10 2002/03/27 22:08:39 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <errno.h>
#include <wchar.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <os/syscall.h>
#include <kernel/fs.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "video.h"

void swap_int(int *a, int *b)
{
    int temp = *b;
    *b = *a;
    *a = temp;
}

videomode_t video_mode = { 0, 80, 25, 4, 0, VIDEO_MODE_TEXT };

typedef struct request_vid_t request_vid_t;
struct request_vid_t
{
    request_t header;
    params_vid_t params;
};

typedef struct video_drv_t video_drv_t;
struct video_drv_t
{
    device_t dev;
    video_t *vid;
};

FT_Library ft_library;
FT_Face ft_face;
void *font_data;

video_t *vga4Init(void);
video_t *vga8Init(void);

struct
{
    const wchar_t *name;
    video_t *(*init)(void);
    video_t *vid;
} drivers[] =
{
    { L"vga4",        vga4Init,   NULL },
    { L"vga8",        vga8Init,   NULL },
    { NULL,        NULL,            NULL },
};

typedef struct modemap_t modemap_t;
struct modemap_t
{
    videomode_t mode;
    video_t *vid;
};

modemap_t *modes;
unsigned numModes;

void vidHLine(video_t *vid, int x1, int x2, int y, colour_t c)
{
    for (; x1 < x2; x1++)
        vid->vidPutPixel(vid, x1, y, c);
}

void vidVLine(video_t *vid, int x, int y1, int y2, colour_t c)
{
    for (; y1 < y2; y1++)
        vid->vidPutPixel(vid, x, y1, c);
}

/* from Allegro gfx.c */
static void do_line(video_t *vid, int x1, int y1, int x2, int y2, colour_t d, 
                    void (*proc)(video_t*, int, int, colour_t))
{
    int dx = x2-x1;
    int dy = y2-y1;
    int i1, i2;
    int x, y;
    int dd;
    
    /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)        \
    {                                                                        \
        if (d##pri_c == 0) {                                                \
            proc(vid, x1, y1, d);                                        \
            return;                                                        \
        }                                                                \
        \
        i1 = 2 * d##sec_c;                                                \
        dd = i1 - (sec_sign (pri_sign d##pri_c));                        \
        i2 = dd - (sec_sign (pri_sign d##pri_c));                        \
        \
        x = x1;                                                         \
        y = y1;                                                         \
        \
        while (pri_c pri_cond pri_c##2) {                                \
                proc(vid, x, y, d);                                        \
            if (dd sec_cond 0) {                                        \
                sec_c sec_sign##= 1;                                        \
                dd += i2;                                                \
            }                                                                \
            else                                                        \
                dd += i1;                                                \
            \
            pri_c pri_sign##= 1;                                        \
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

void vidLine(video_t *vid, int x1, int y1, int x2, int y2, colour_t d)
{
    if (x1 == x2)
        vid->vidVLine(vid, x1, y1, y2, d);
    else if (y1 == y2)
        vid->vidHLine(vid, x1, x2, y1, d);
    else
        do_line(vid, x1, y1, x2, y2, d, vid->vidPutPixel);
}

void vidFillRect(video_t *vid, int x1, int y1, int x2, int y2, colour_t c)
{
    for (; y1 < y2; y1++)
        vid->vidHLine(vid, x1, x2, y1, c);
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
static void fill_edge_structure(POLYGON_EDGE *edge, const point_t *i1, const point_t *i2)
{
    if (i2->y < i1->y) {
        const point_t *it;

        it = i1;
        i1 = i2;
        i2 = it;
    }

    edge->top = i1->y;
    edge->bottom = i2->y - 1;
    edge->dx = ((i2->x - i1->x) << POLYGON_FIX_SHIFT) / (i2->y - i1->y);
    edge->x = (i1->x << POLYGON_FIX_SHIFT) + (1<<(POLYGON_FIX_SHIFT-1)) - 1;
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
void vidFillPolygon(video_t *vid, const point_t *points, unsigned vertices, 
                    colour_t colour)
{
    int c;
    int top = INT_MAX;
    int bottom = INT_MIN;
    const point_t *i1, *i2;
    POLYGON_EDGE *edge, *next_edge;
    POLYGON_EDGE *active_edges = NULL;
    POLYGON_EDGE *inactive_edges = NULL;
    
    /* allocate some space and fill the edge table */
    _grow_scratch_mem(sizeof(POLYGON_EDGE) * vertices);
    
    edge = _scratch_mem;
    i1 = points;
    i2 = points + vertices - 1;
    
    for (c=0; c < (int) vertices; c++) {
        if (i1->y != i2->y) {
            fill_edge_structure(edge, i1, i2);
            
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
            vid->vidHLine(vid, edge->x>>POLYGON_FIX_SHIFT,
                (edge->next->x+edge->next->w)>>POLYGON_FIX_SHIFT, 
                c, colour);
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

void draw_bitmap(video_t *vid, FT_Bitmap *bmp, int sx, int sy, colour_t fg, colour_t bg)
{
    colour_t clr;
    int x, y, r, g, b, ar, ag, ab;
    uint8_t *ptr;
    
    ptr = bmp->buffer;
    for (y = 0; y < bmp->rows; y++)
    {
        for (x = 0; x < bmp->width; x++)
        {
            switch (bmp->pixel_mode)
            {
            case ft_pixel_mode_mono:
                if (fg != -1 && ptr[x / 8] & (0x80 >> (x & 7)))
                    vid->vidPutPixel(vid, sx + x, sy + y, fg);
                if (bg != -1 && (ptr[x / 8] & (0x80 >> (x & 7))) == 0)
                    vid->vidPutPixel(vid, sx + x, sy + y, bg);
                break;

            case ft_pixel_mode_grays:
                if (bg == -1)
                    clr = vid->vidGetPixel(vid, sx + x, sy + y);
                else
                    clr = bg;
                
                if (fg != -1 && ptr[x] != 0)
                {
                    int p, rp;

                    p = ptr[x] / 16;
                    rp = 15 - p;
                    r = COLOUR_RED(clr);
                    g = COLOUR_GREEN(clr);
                    b = COLOUR_BLUE(clr);
                    ar = COLOUR_RED(fg);
                    ag = COLOUR_GREEN(fg);
                    ab = COLOUR_BLUE(fg);
                    r = (r * rp + ar * p) / 16;
                    g = (g * rp + ag * p) / 16;
                    b = (b * rp + ab * p) / 16;
                    vid->vidPutPixel(vid, sx + x, sy + y, MAKE_COLOUR(r, g, b));
                }

                if (bg != -1 && ptr[x] == 0)
                    vid->vidPutPixel(vid, sx + x, sy + y, bg);
                break;
            }
        }

        ptr += bmp->pitch;
    }
}

void vidTextOut(video_t *vid, rect_t *rect, const wchar_t *str, size_t len, 
                colour_t fg, colour_t bg)
{
    int error, x, y, maxx, maxy;

    //wprintf(L"height = 0x%x = %d\n", 
        //ft_face->size->metrics.height, ft_face->size->metrics.height >> 6);
    maxx = x = rect->left;
    y = rect->top + (ft_face->size->metrics.height >> 7);
    maxy = rect->top;

    while (len > 0)
    {
        if (*str != '\n')
        {
            error = FT_Load_Char(ft_face, *str, FT_LOAD_RENDER);
            if (error)
                wprintf(L"FT_Load_Char: %x\n", error);
        }
        else
            error = 1;

        if (*str == '\n' ||
            x + (ft_face->glyph->advance.x >> 6) >= rect->right)
        {
            x = rect->left;
            y += ft_face->size->metrics.height >> 6;
            if (y > maxy)
                maxy = y;

            if (y >= rect->bottom)
                break;
        }

        if (error == 0 && (fg != -1 || bg != -1))
        {
            //wprintf(L"%c(%d)", *str, ft_face->glyph->bitmap_top >> 6);
            //vid->vidHLine(vid, x + ft_face->glyph->bitmap_left,
                //x + ft_face->glyph->bitmap_left + ft_face->glyph->bitmap.width,
                //y - ft_face->glyph->bitmap_top,
                //7);
            draw_bitmap(vid, &ft_face->glyph->bitmap,
                 x + ft_face->glyph->bitmap_left, 
                 y - ft_face->glyph->bitmap_top,
                 fg, bg);

            x += ft_face->glyph->advance.x >> 6;
            if (x > maxx)
                maxx = x;
        }

        str++;
        len--;
    }

    rect->right = maxx;
    rect->bottom = maxy;
}

int vidMatchMode(const videomode_t *a, const videomode_t *b)
{
    if ((b->width == 0 || a->width == b->width) &&
        (b->height == 0 || a->height == b->height) &&
        (b->bitsPerPixel == 0 || a->bitsPerPixel == b->bitsPerPixel))
        return a->width * a->height * a->bitsPerPixel;
    else
        return 0;
}

bool vidSetMode(video_drv_t *video, videomode_t *mode)
{
    int i, best, score, s;
    video_t *vid;

    if (video->vid)
    {
        video->vid->vidClose(video->vid);
        video->vid = NULL;
    }

    best = -1;
    score = 0;
    for (i = 0; i < numModes; i++)
    {
        s = vidMatchMode(&modes[i].mode, mode);
        wprintf(L"video: mode %u = %ux%ux%u: score = %d\n", 
            i, modes[i].mode.width, modes[i].mode.height, modes[i].mode.bitsPerPixel,
            s);
        if (s > score)
        {
            best = i;
            score = s;
        }
    }

    if (best == -1)
    {
        wprintf(L"video: mode %ux%ux%u not found\n", 
            mode->width, mode->height, mode->bitsPerPixel);
        errno = ENOTFOUND;
        return false;
    }

    *mode = modes[best].mode;
    vid = modes[best].vid;

    assert(vid->vidClose != NULL);
    assert(vid->vidEnumModes != NULL);
    assert(vid->vidSetMode != NULL);
    assert(vid->vidPutPixel != NULL);

    if (vid->vidHLine == NULL)
        vid->vidHLine = vidHLine;
    if (vid->vidVLine == NULL)
        vid->vidVLine = vidVLine;
    if (vid->vidFillRect == NULL)
        vid->vidFillRect = vidFillRect;
    if (vid->vidLine == NULL)
        vid->vidLine = vidLine;
    if (vid->vidFillPolygon == NULL)
        vid->vidFillPolygon = vidFillPolygon;
    if (vid->vidTextOut == NULL)
        vid->vidTextOut = vidTextOut;
    
    if (!vid->vidSetMode(vid, mode))
        return false;

    video->vid = vid;
    wprintf(L"video: using mode %ux%ux%u\n", 
        mode->width, mode->height, mode->bitsPerPixel);

    return true;
}

#define VID_OP(code, type) \
    case code: \
    { \
        type *shape; \
        shape = (type*) &buf->s; \
        \
        
#define VID_END_OP \
        break; \
    }

bool vidRequest(device_t* dev, request_t* req)
{
    video_drv_t *video = (video_drv_t*) dev;
    params_vid_t *params = &((request_vid_t*) req)->params;
    size_t user_length;
    
    switch (req->code)
    {
    case DEV_REMOVE:
        free(dev);
            return true;

    case VID_SETMODE:
        if (!vidSetMode(video, &params->vid_setmode))
        {
            req->result = errno;
            return false;
        }
        else
            return true;
        
    case VID_DRAW:
        {
            vid_shape_t *buf;

            user_length = params->vid_draw.length;
            params->vid_draw.length = 0;
            buf = params->vid_draw.shapes;

            while (params->vid_draw.length < user_length)
            {
                switch (buf->shape)
                {
                VID_OP(VID_SHAPE_FILLRECT, vid_rect_t)
                    video->vid->vidFillRect(video->vid, 
                        shape->rect.left, shape->rect.top, 
                        shape->rect.right, shape->rect.bottom, 
                        shape->colour);
                VID_END_OP

                VID_OP(VID_SHAPE_HLINE, vid_line_t)
                    if (shape->a.x != shape->b.x)
                    {
                        if (shape->b.x < shape->a.x)
                            swap_int(&shape->a.x, &shape->b.x);
                        video->vid->vidHLine(video->vid, 
                            shape->a.x, shape->b.x, 
                            shape->a.y, 
                            shape->colour);
                    }
                VID_END_OP

                VID_OP(VID_SHAPE_VLINE, vid_line_t)
                    if (shape->a.y != shape->b.y)
                    {
                        if (shape->b.y < shape->a.y)
                            swap_int(&shape->a.y, &shape->b.y);
                        video->vid->vidVLine(video->vid, 
                            shape->a.x, 
                            shape->a.y, shape->b.y, 
                            shape->colour);
                    }
                VID_END_OP

                VID_OP(VID_SHAPE_LINE, vid_line_t)
                    /*if (shape->b.x < shape->a.x)
                        swap_int(&shape->a.x, &shape->b.x);
                    if (shape->b.y < shape->a.y)
                        swap_int(&shape->a.y, &shape->b.y);*/

                    video->vid->vidLine(video->vid, 
                        shape->a.x, shape->a.y, 
                        shape->b.x, shape->b.y, 
                        shape->colour);
                VID_END_OP

                VID_OP(VID_SHAPE_PUTPIXEL, vid_pixel_t)
                    video->vid->vidPutPixel(video->vid, 
                        shape->point.x, shape->point.y,
                        shape->colour);
                VID_END_OP

                VID_OP(VID_SHAPE_GETPIXEL, vid_pixel_t)
                    shape->colour = video->vid->vidGetPixel(video->vid, 
                        shape->point.x, shape->point.y);
                VID_END_OP
                }

                buf++;
                params->vid_draw.length += sizeof(*buf);
            }

            return true;
        }

    case VID_TEXTOUT:
    {
        vid_text_t *p = &params->vid_textout;
        video->vid->vidTextOut(video->vid, &p->rect, 
            p->buffer, p->length / sizeof(wchar_t), 
            p->foreColour, p->backColour);
        return true;
    }

    case VID_FILLPOLYGON:
    {
        video->vid->vidFillPolygon(video->vid,
            params->vid_fillpolygon.points, 
            params->vid_fillpolygon.length / sizeof(point_t),
            params->vid_fillpolygon.colour);
        return true;
    }

    case VID_STOREPALETTE:
    {
        vid_palette_t *p = &params->vid_storepalette;
        if (video->vid->vidStorePalette)
        {
            video->vid->vidStorePalette(video->vid, 
                p->entries,
                p->first_index,
                p->length / sizeof(rgb_t));
            return true;
        }
        break;
    }
    }

    req->result = ENOTIMPL;
    return false;
}

static const device_vtbl_t vid_vtbl =
{
    vidRequest,
    NULL,
    NULL,
};

/*
 * Support routines for FreeType
 */

FILE *fopen(const char *file, const char *mode)
{
    wchar_t *wc;
    size_t len;
    handle_t hnd;

    assert(mode[0] == 'r' && mode[1] == 'b' && mode[2] == '\0');

    len = strlen(file);
    wc = malloc(sizeof(wchar_t) * (len + 1));
    if (wc == NULL)
        return NULL;

    len = mbstowcs(wc, file, len);
    if (len == -1)
    {
        free(wc);
        return NULL;
    }

    wc[len] = '\0';
    wprintf(L"fopen: opening %s\n", wc);
    hnd = FsOpen(wc, FILE_READ);
    free(wc);

    return (FILE*) hnd;
}

int fclose(FILE *file)
{
    if (file != NULL)
    {
        FsClose((handle_t) file);
        return 0;
    }
    else
        return EOF;
}

int fseek(FILE *stream, long offset, int origin)
{
    return FsSeek((handle_t) stream, offset, origin);
}

long ftell(FILE *stream)
{
    return FsSeek((handle_t) stream, 0, SEEK_CUR);
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
{
    return FsReadSync((handle_t) stream, buffer, size * count) / size;
}

device_t* vidAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
    video_drv_t* dev;
    int i, j, code;
    videomode_t mode;
    video_t *vid;
    int error, pts;
    const wchar_t *font;
    dirent_t di;

    for (i = 0; drivers[i].name; i++)
    {
        vid = drivers[i].vid = drivers[i].init();

        j = 0;
        do
        {
            code = vid->vidEnumModes(vid, j, &mode);
            if (code == VID_ENUM_ERROR)
                break;

            numModes++;
            modes = realloc(modes, sizeof(modemap_t) * numModes);
            modes[numModes - 1].vid = vid;
            modes[numModes - 1].mode = mode;
            j++;
        } while (code != VID_ENUM_STOP);
    }

    dev = malloc(sizeof(video_drv_t));
    dev->dev.vtbl = &vid_vtbl;
    dev->dev.driver = drv;
    dev->vid = NULL;

    FT_Init_FreeType(&ft_library);

    font = L"/System/Boot/rezn000.ttf";

    if (FsQueryFile(font, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
        handle_t file;
        FT_Open_Args args;

        font_data = malloc(di.length);
        file = FsOpen(font, FILE_READ);
        FsReadSync(file, font_data, di.length);
        FsClose(file);

        args.flags = ft_open_memory;
        args.memory_base = font_data;
        args.memory_size = di.length;
        error = FT_Open_Face(ft_library,
	    &args,
	    0,
	    &ft_face);
        if (error != 0)
            wprintf(L"FT_Open_Face: %x\n", error);
        else
        {
            pts = 20;
            do
            {
                wprintf(L"FT_Set_Char_Size: trying %d pt\n", pts);
                error = FT_Set_Char_Size(ft_face, 0, pts * 64, 96, 96);
                pts++;
            } while (error != 0 && pts <= 72);

            if (error != 0)
                wprintf(L"FT_Set_Char_Size: %x\n", error);
        }
    }

    return &dev->dev;
}

bool DrvInit(driver_t* drv)
{
    drv->add_device = vidAddDevice;
    return true;
}
