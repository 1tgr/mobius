#include <malloc.h>
#include <stdio.h>
#include <os/os.h>
#include <assert.h>

semaphore_t sem_malloc;
static BlockHeader base;
BlockHeader *freep;

BlockHeader* morecore(size_t nu);

typedef struct malloc_debug_t malloc_debug_t;
struct malloc_debug_t
{
	malloc_debug_t *next;
	int line;
	const char *file;
	dword start;
};

#define MALLOC_START	0xA110C574
#define MALLOC_END		0xA110C34D

malloc_debug_t *__first, *__last;

void	__free_core(void *ptr);
//void*	__realloc_core(void* ptr, size_t size);
size_t	__msize_core(void* ptr);

//! Allocates a block of memory.
/*!
 *	malloc() is required to allocate blocks of memory of arbitrary size
 *		(i.e. whose size is not known at compile time). The contents of the
 *		new block are undefined.
 *
 *	Blocks allocated using malloc() should be freed using free(). Calls to
 *		malloc() may require more memory from the operating system, which 
 *		requires a system call.
 *
 *	Blocks returned by malloc() are guaranteed to be \e at \e least \p nbytes 
 *		long. More memory may be allocated for memory manager overhead.
 *	\param	nbytes	Size, in bytes, of the block requested.
 *	\return	A pointer to the start of the new block, or NULL if the 
 *		allocation failed (e.g. if the operation system was unable to
 *		allocate any more memory to the process).
 */
void *malloc_core(size_t nbytes)
{
	BlockHeader *p, *prevp;
	unsigned nunits;

	semAcquire(&sem_malloc);
    nunits = (nbytes + sizeof(BlockHeader) - 1) / sizeof(BlockHeader) + 1;
    //wprintf(L"malloc: %d bytes = %d units freep = %p", nbytes, nunits, freep);
    if ((prevp = freep) == NULL)
	{
		base.s.ptr = freep = prevp = &base;
		base.s.size = 0;
	}

	for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr)
	{
		if (p->s.size >= nunits)
		{
			if (p->s.size == nunits)
				prevp->s.ptr = p->s.ptr;
			else
			{
				p->s.size -= nunits;
				p += p->s.size;
				p->s.size = nunits;
			}

			freep = prevp;
			//wprintf(L"buf = %x\n", p + 1);
			semRelease(&sem_malloc);
			return (void*) (p + 1);
		}

		if (p == freep)
			if ((p = morecore(nunits)) == NULL)
			{
				//wprintf(L"[malloc] out of memory\n");
				semRelease(&sem_malloc);
				return NULL;
			}
	}
}

void __malloc_check()
{
	malloc_debug_t *h;
	char *s;
	dword *dw;

	for (h = __first; h; h = h->next)
	{
		if (h->start != MALLOC_START)
		{
			s = (char*) &h->start;
			wprintf(L"%S(%d): damaged memory before %p: %c%c%c%c",
				h->file, h->line, h + 1, s[0], s[1], s[2], s[3]);
		}

		dw = (dword*) ((byte*) h + msize(h));
		if (dw[-1] != MALLOC_END)
		{
			s = (char*) &h->start;
			wprintf(L"%S(%d): damaged memory after %p: %c%c%c%c",
				h->file, h->line, h + 1, s[0], s[1], s[2], s[3]);
		}
	}
}

void *__malloc_dbg(size_t nbytes, const char *file, int line)
{
	malloc_debug_t *header;
	dword *dw;

	__malloc_check();
	nbytes += sizeof(*header) + sizeof(dword);
	header = malloc_core(nbytes);
	if (header == NULL)
		return NULL;

	header->start = MALLOC_START;
	header->file = file;
	header->line = line;
	dw = (dword*) ((byte*) (header + 1) + nbytes);
	*dw = MALLOC_END;

	if (__first == NULL)
		__first = header;
	if (__last)
		__last->next = header;
	header->next = NULL;
	__last = header;

	return header + 1;
}

void __free_dbg(void *p)
{
	malloc_debug_t *header, *h, *prev;

	__malloc_check();
	header = (malloc_debug_t*) p + 1;

	prev = NULL;
	for (h = __first; h; h = h->next)
	{
		if (h == header)
		{
			if (prev)
				prev->next = header->next;
			if (__last == header)
				__last = prev;
			break;
		}

		prev = h;
	}

	if (__first == header)
		__first = header->next;

	__free_core(header);
}

size_t __msize_dbg(void *p)
{
	return __msize_core(p) - sizeof(malloc_debug_t) - sizeof(dword);
}

/*void *__realloc_dbg(void *p, size_t size)
{
	malloc_debug_t *header;
	header = __realloc_core(p, size + sizeof(malloc_debug_t) + sizeof(dword));
	if (header)
		return header + 1;
	else
		return NULL;
}*/