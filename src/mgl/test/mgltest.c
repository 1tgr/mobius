/* $Id: mgltest.c,v 1.3 2002/03/06 19:31:41 pavlovskii Exp $ */

#include <gl/mgl.h>
#include <gl/wmf.h>
#include <os/rtl.h>

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

mglrc_t *rc;

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *str;
    wmf_t *wmf;
    
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
    glSetClearColour(0x00000f);
    glClear();

    WmfDraw(wmf);
    WmfClose(wmf);

    glFlush();
    ConReadKey();
    mglDeleteRc(rc);
    return EXIT_SUCCESS;
}