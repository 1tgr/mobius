/* $Id: polygon.c,v 1.1 2002/04/03 23:33:45 pavlovskii Exp $ */

#include "vidfuncs.h"
#include <limits.h>

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
void vidFillPolygon(video_t *vid, const clip_t *clip, const point_t *points, 
                    unsigned vertices, colour_t colour)
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
            vid->vidHLine(vid, 
                clip, 
                edge->x>>POLYGON_FIX_SHIFT,
                (edge->next->x+edge->next->w)>>POLYGON_FIX_SHIFT, 
                c, 
                colour);
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
