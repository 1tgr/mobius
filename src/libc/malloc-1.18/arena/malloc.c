/*  This file should be edited with 4-column tabs! */
/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/
#include "adefs.h"
#include "arena.h"

RCSID("$Header: /cvsroot/mobius/mn/src/libc/malloc-1.18/arena/malloc.c,v 1.1 2001/11/05 18:31:47 pavlovskii Exp $");

static Arena heap = AINIT;

univptr_t
malloc(nbytes)
size_t nbytes;
{
    return amalloc(&heap, nbytes);
}

void
free(cp)
univptr_t cp;
{
    afree(&heap, cp);
}


univptr_t
realloc(cp, nbytes)
univptr_t cp;
size_t nbytes;
{
    return arealloc(&heap, cp, nbytes);
}


univptr_t
calloc(nelem, elsize)
size_t nelem, elsize;
{
    return acalloc(&heap, nelem, elsize);
}
