/* $Id: mgltest.c,v 1.5 2002/04/04 00:09:00 pavlovskii Exp $ */

#include <gl/mgl.h>
#include <gl/wmf.h>
#include <os/rtl.h>
#include <os/syscall.h>

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <printf.h>

mglrc_t *rc;
MGLpoint gprintf_pos;

bool gwprintf_help(void* context, const wchar_t* str, size_t len)
{
    /*MGLpoint *pt;
    MGLrect rect, dims;
    unsigned i;

    mglGetDimensions(NULL, &dims);
    
    rect = dims;
    rect.left = pt->x;
    rect.top = pt->y;

    glDrawText(&rect, str, len);
    
    for (i = 0; i < len; i++)
        if (str[i] == '\n')
        {
            pt->x = dims.left;
            pt->y += 16;
        }
        else
            pt->x += 38;*/
    DbgWrite(str, len);
    return true;
}

int vgwprintf(MGLpoint *pt, const wchar_t *fmt, va_list ptr)
{
    if (pt == NULL)
        pt = &gprintf_pos;

    return dowprintf(gwprintf_help, pt, fmt, ptr);
}

int gwprintf(MGLpoint *pt, const wchar_t *fmt, ...)
{
    va_list ptr;
    int ret;

    if (pt == NULL)
        pt = &gprintf_pos;

    va_start(ptr, fmt);
    ret = vgwprintf(pt, fmt, ptr);
    va_end(ptr);
    return ret;
}

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *str;
    wmf_t *wmf;
    MGLrect dims;
    int i;
    
    if (argc >= 2)
	str = argv[1];
    else
	str = L"folder.wmf";
    
    wmf = WmfOpen(str);
    if (wmf == NULL)
    {
	_pwerror(str);
	return EXIT_FAILURE;
    }

    rc = mglCreateRc(NULL);
    glSetColour(0x000000);
    glSetClearColour(0xffffff);
    glClear();
    
    WmfDraw(wmf);
    WmfClose(wmf);

    mglGetDimensions(NULL, &dims);
    
    for (i = 0; i < 256; i++) 
    {
        glSetColour(MGL_COLOUR(255 - i, 0, 0));
        glFillRect((i * dims.right) / 256, 0, 
            ((i + 1) * dims.right) / 256, 30);
        glSetColour(MGL_COLOUR(0, i, 0));
        glFillRect((i * dims.right) / 256, 30, 
            ((i + 1) * dims.right) / 256, 60);
        glSetColour(MGL_COLOUR(0, 0, 255 - i));
        glFillRect((i * dims.right) / 256, 60, 
            ((i + 1) * dims.right) / 256, 90);
        glSetColour(MGL_COLOUR(i, i, i));
        glFillRect((i * dims.right) / 256, 90, 
            ((i + 1) * dims.right) / 256, 180);
    }
      
    glSetColour(0);
    glDrawText(&dims, L"Hello, world!\nThis is a test of the FreeType font engine.", -1);

    ConReadKey();
    mglDeleteRc(rc);
    return EXIT_SUCCESS;
}