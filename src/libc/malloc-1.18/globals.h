/* $Id: globals.h,v 1.1 2001/11/05 18:31:43 pavlovskii Exp $ */
#ifndef __GLOBALS_H__
#define __GLOBALS_H__
/*
 *  Remember to initialize the variable in globals.c if you want, and
 *  provide an alternative short name in globrename.h
 */
#include "globrename.h"

extern size_t _malloc_minchunk;

extern Word *_malloc_rovers[];
extern const short _malloc_binmax[];
extern int _malloc_firstbin;
extern int _malloc_lastbin;
extern Word *_malloc_hiword;
extern Word *_malloc_loword;

extern size_t _malloc_sbrkunits;

extern Word *_malloc_mem;

extern int _malloc_tracing;	/* No tracing */
extern FILE *_malloc_statsfile;
extern char _malloc_statsbuf[];

extern int _malloc_leaktrace;

#ifdef PROFILESIZES
extern int _malloc_scount[];
#endif /* PROFILESIZES */

#ifdef DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
extern int _malloc_debugging;
#endif /* DEBUG */

extern univptr_t (* _malloc_memfunc) proto((size_t));

extern int __m_prblock proto((univptr_t, int, FILE *));

#endif /* __GLOBALS_H__ */ /* Do not add anything after this line */
