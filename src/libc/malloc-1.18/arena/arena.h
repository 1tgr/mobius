/*  Author: Mark Moraes <moraes@csri.toronto.edu> */
/* $Header: /cvsroot/mobius/mn/src/libc/malloc-1.18/arena/arena.h,v 1.1 2001/11/05 18:31:47 pavlovskii Exp $ */
#ifndef __ARENA_H__
#define __ARENA_H__

#ifndef MAXPROFILESIZE
# define MAXPROFILESIZE 1
#endif

typedef struct {
    size_t minchunk;
    Word *rover;
    Word *hiword;
    Word *loword;
    size_t sbrkunits;
    size_t totalavail;
    char *spare;
    int nspare;
    Word *mem;
    FILE *statsfile;
    char statsbuf[128];
    int tracing;
    int leaktrace;
    int debugging;
    int scount[MAXPROFILESIZE];
} Arena;

#define AINIT { \
    FIXEDOVERHEAD,  /* minchunk */ \
    NULL,	    /* rover */ \
    NULL,	    /* hiword */ \
    NULL,	    /* loword */ \
    DEF_SBRKUNITS,  /* sbrkunits */ \
    0,		    /* totalavail */ \
    NULL,	    /* spare */ \
    0,		    /* nspare */ \
    NULL,	    /* mem */ \
    stderr,	    /* statsfile */ \
    { '\0' },	    /* statsbuf */ \
    0,		    /* tracing */ \
    0,		    /* leaktrace */ \
    0,		    /* debugging */ \
    { 0 },	    /* scount */ \
}

#endif /* __ARENA_H__ */ /* Do not add anything after this line */
