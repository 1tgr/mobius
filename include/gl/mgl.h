/* $Id: mgl.h,v 1.7 2002/08/04 17:22:39 pavlovskii Exp $ */

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