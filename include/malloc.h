/* $Id: malloc.h,v 1.1.1.1 2002/12/31 01:26:19 pavlovskii Exp $ */

#ifndef __MALLOC_H
#define __MALLOC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

typedef struct malloc_state *mstate;
extern struct malloc_state __av_default;

void ** independent_calloc(size_t n_elements, size_t size, void* chunks[]);
void ** independent_comalloc(size_t n_elements, size_t sizes[], void* chunks[]);
void *  pvalloc(size_t n);
void    cfree(void* p);

mstate  malloc_create_heap(mstate av, void *(*allocator)(size_t));
void    malloc_delete_heap(mstate av);

void *  acalloc(mstate av, size_t _nelem, size_t _size);
void *  amalloc(mstate av, size_t _size);
void    afree(mstate av, void *_ptr);
void *  arealloc(mstate av, void *_ptr, size_t _size);
void *  amemalign(mstate av, size_t alignment, size_t n);
void *  avalloc(mstate av, size_t n);

void ** independent_acalloc(mstate av, size_t n_elements, size_t size, void* chunks[]);
void ** independent_acomalloc(mstate av, size_t n_elements, size_t sizes[], void* chunks[]);
void *  apvalloc(mstate av, size_t n);
void    acfree(mstate av, void* p);

void *  _alloca(size_t _size);

typedef struct __maldbg_header_t __maldbg_header_t;
struct __maldbg_header_t
{
    int line;
    const char *file;
    __maldbg_header_t *prev, *next;
    size_t size;
    int tag;
    unsigned int magic[2];
};

#ifdef __cplusplus
}
#endif

#endif
