/* $Id: malloc.h,v 1.2 2002/09/08 20:47:03 pavlovskii Exp $ */

#ifndef __MALLOC_H
#define __MALLOC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

typedef struct malloc_state *mstate;
extern struct malloc_state __av_default;

void *  calloc(size_t _nelem, size_t _size);
void *  malloc(size_t _size);
void    free(void *_ptr);
void *  realloc(void *_ptr, size_t _size);
void *  memalign(size_t alignment, size_t n);
void *  valloc(size_t n);

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

void *  __malloc(size_t _size, const char *, int);
void    __free(void *_ptr, const char *, int);
void *  __realloc(void *_ptr, size_t _size, const char *, int);
wchar_t *__wcsdup(const wchar_t *_str, const char *, int);
char *  __strdup(const char *_str, const char *, int);
__maldbg_header_t *__malloc_find_block(void *addr);
void    __malloc_leak(int tag);
void    __malloc_leak_dump(void);

#ifdef KERNEL
#define malloc(x)       __malloc((x), __FILE__, __LINE__)
#define calloc(x, n)    __calloc((x), (n), __FILE__, __LINE__)
#define free(p)         __free((p), __FILE__, __LINE__)
#define realloc(p, x)   __realloc((p), (x), __FILE__, __LINE__)
#define _strdup(p)      __strdup((p), __FILE__, __LINE__)
#define _wcsdup(p)      __wcsdup((p), __FILE__, __LINE__)
#endif

#ifdef __cplusplus
}
#endif

#endif
