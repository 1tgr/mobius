/* $Id: malwrap.c,v 1.1 2002/12/18 23:29:50 pavlovskii Exp $ */

#include <malloc.h>

void *  calloc(size_t _nelem, size_t _size)
{
    return acalloc(&__av_default, _nelem, _size);
}

void *  malloc(size_t _size)
{
    return amalloc(&__av_default, _size);
}

void    free(void *_ptr)
{
    return afree(&__av_default, _ptr);
}

void *  realloc(void *_ptr, size_t _size)
{
    return arealloc(&__av_default, _ptr, _size);
}

void *  memalign(size_t alignment, size_t n)
{
    return amemalign(&__av_default, alignment, n);
}

void *  valloc(size_t n)
{
    return avalloc(&__av_default, n);
}

void ** independent_calloc(size_t n_elements, size_t size, void* chunks[])
{
    return independent_acalloc(&__av_default, n_elements, size, chunks);
}

void ** independent_comalloc(size_t n_elements, size_t sizes[], void* chunks[])
{
    return independent_acomalloc(&__av_default, n_elements, sizes, chunks);
}

void *  pvalloc(size_t n)
{
    return apvalloc(&__av_default, n);
}

void    cfree(void* p)
{
    return acfree(&__av_default, p);
}
