#ifndef __GL_MGL_H
#define __GL_MGL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

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

void	    rectSet(MGLrect* r, 
		    MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
void	    rectSetEmpty(MGLrect* r);
bool	    rectIsEmpty(const MGLrect* r);
void	    rectUnion(MGLrect* a, const MGLrect* b);
bool	    rectIncludesPoint(const MGLrect* r, MGLreal x, MGLreal y);
void	    rectOffset(MGLrect* r, MGLreal dx, MGLreal dy);
bool	    rectIntersects(const MGLrect* r1, const MGLrect* r2);
void	    rectInflate(MGLrect* r, MGLreal dx, MGLreal dy);

#ifdef __cplusplus
}
#endif

#endif