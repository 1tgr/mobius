/* $Id: mgl.h,v 1.3 2002/03/06 19:35:44 pavlovskii Exp $ */

#ifndef __GL_MGL_H
#define __GL_MGL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/*!
 *  \defgroup	mgl   Mobius Graphics Layer (mgl)
 * @{
 */

typedef    int	    MGLreal;
typedef    uint32_t MGLcolour;
#define    MGLcolor MGLcolour

typedef struct mglrc_t mglrc_t;

typedef struct MGLpoint MGLpoint;
struct MGLpoint
{
    MGLreal x, y;
};

typedef struct MGLrect MGLrect;
struct MGLrect
{
    MGLreal left, top, right, bottom;
};

mglrc_t	    *mglCreateRc(const wchar_t *server);
bool	    mglDeleteRc(mglrc_t *rc);
bool	    mglUseRc(mglrc_t *rc);
bool	    mglGetDimensions(mglrc_t *rc, MGLrect *rect);

void	    glFillRect(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
MGLcolour   glSetColour(MGLcolour clr);
#define     glSetColor    glSetColour
void	    glClear(void);
MGLcolour   glSetClearColour(MGLcolour clr);
#define     glSetClearColor	  glSetClearColour
void	    glMoveTo(MGLreal x, MGLreal y);
void	    glLineTo(MGLreal x, MGLreal y);
void	    glRectangle(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
void	    glFlush(void);
void	    glPutPixel(MGLreal x, MGLreal y);
void	    glDrawText(const MGLrect *rc, const wchar_t *str, size_t len);
void	    glFillPolygon(const MGLpoint *points, unsigned num_points);
void	    glPolygon(const MGLpoint *points, unsigned num_points);

#define MGL_ALPHA(c)	((byte) (((c) & 0xff000000) >> 24))
#define MGL_RED(c)	  ((byte) (((c) & 0xff000000) >> 16))
#define MGL_GREEN(c)	((byte) (((c) & 0x00ff0000) >> 8))
#define MGL_BLUE(c)	   ((byte) (((c) & 0x000000ff)))
#define MGL_COLOUR4(r,g,b,a)	\
    ((byte) (b) | \
    (byte) (g) << 8 | \
    (byte) (r) << 16 | \
    (byte) (a) << 24)
#define MGL_COLOR4(r,g,b,a)    MGL_COLOUR4(r,g,b,a)
#define MGL_COLOUR(r,g,b)    MGL_COLOUR4(r,g,b,0xff)
#define MGL_COLOR(r,g,b)    MGL_COLOR4(r,g,b,0xff)

void	    RectSet(MGLrect* r, 
		    MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
void	    RectSetEmpty(MGLrect* r);
bool	    RectIsEmpty(const MGLrect* r);
void	    RectUnion(MGLrect* a, const MGLrect* b);
bool	    RectIncludesPoint(const MGLrect* r, MGLreal x, MGLreal y);
void	    RectOffset(MGLrect* r, MGLreal dx, MGLreal dy);
bool	    RectIntersects(const MGLrect* r1, const MGLrect* r2);
void	    RectInflate(MGLrect* r, MGLreal dx, MGLreal dy);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif