/* $Id: mgl.h,v 1.6 2002/04/10 12:32:37 pavlovskii Exp $ */

#ifndef __GL_MGL_H
#define __GL_MGL_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *  \defgroup   mgl   Mobius Graphics Layer (mgl)
 * @{
 */

#include <gl/types.h>

mglrc_t	    *mglCreateRc(const wchar_t *server);
bool        mglDeleteRc(mglrc_t *rc);
bool        mglUseRc(mglrc_t *rc);
bool        mglGetDimensions(mglrc_t *rc, MGLrect *rect);
void        mglMoveCursor(MGLpoint pt);
void        mglSetClip(mglrc_t *rc, const MGLclip *clip);

void        glFillRect(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
MGLcolour   glSetColour(MGLcolour clr);
#define     glSetColor    glSetColour
void        glClear(void);
MGLcolour   glSetClearColour(MGLcolour clr);
#define     glSetClearColor	  glSetClearColour
void        glMoveTo(MGLreal x, MGLreal y);
void        glLineTo(MGLreal x, MGLreal y);
void        glRectangle(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
void        glFlush(void);
void        glPutPixel(MGLreal x, MGLreal y);
void        glDrawText(const MGLrect *rc, const wchar_t *str, int len);
void        glGetTextSize(const wchar_t *str, int len, MGLpoint *size);
void        glFillPolygon(const MGLpoint *points, unsigned num_points);
void        glPolygon(const MGLpoint *points, unsigned num_points);
void        glBevel(const MGLrect *rect, MGLcolour colour, int border, 
                    unsigned char diff, bool is_extruded);

#define MGL_ALPHA(c)	((uint8_t) (((c) & 0xff000000) >> 24))
#define MGL_RED(c)	((uint8_t) (((c) & 0x00ff0000) >> 16))
#define MGL_GREEN(c)	((uint8_t) (((c) & 0x0000ff00) >> 8))
#define MGL_BLUE(c)	((uint8_t) (((c) & 0x000000ff)))
#define MGL_COLOUR4(r,g,b,a)	\
    ((uint8_t) (b) | \
     (uint8_t) (g) << 8 | \
     (uint8_t) (r) << 16 | \
     (uint8_t) (a) << 24)
#define MGL_COLOR4(r,g,b,a)    MGL_COLOUR4(r,g,b,a)
#define MGL_COLOUR(r,g,b)    MGL_COLOUR4(r,g,b,0)
#define MGL_COLOR(r,g,b)    MGL_COLOR4(r,g,b,0)

void        RectSet(MGLrect* r, MGLreal left, MGLreal top, MGLreal right, MGLreal bottom);
void        RectSetEmpty(MGLrect* r);
bool        RectIsEmpty(const MGLrect* r);
void        RectUnion(MGLrect* a, const MGLrect* b);
bool        RectIncludesPoint(const MGLrect* r, MGLreal x, MGLreal y);
void        RectOffset(MGLrect* r, MGLreal dx, MGLreal dy);
bool        RectIntersects(const MGLrect* r1, const MGLrect* r2);
void        RectInflate(MGLrect* r, MGLreal dx, MGLreal dy);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif