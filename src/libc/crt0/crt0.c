/* $Id: crt0.c,v 1.2 2002/08/05 17:10:45 pavlovskii Exp $ */

/*
 * Note: this source file compiles to both /lib/crt0.o and /lib/wcrt0.o. 
 *  wcrt0.o gets WIDE defined.
 */

#include <stdlib.h>
#include <internal.h>

void mainCRTStartup(void)
{
    CHAR **argv;
    int argc;
    argc = __crt0_parseargs(&argv);
#ifdef WIDE
    /* xxx - what is the matter with GNU developers and wide characters?! */
    __main();
#endif
    exit(MAIN(argc, argv));
}
