#include <malloc.h>
#include <stdio.h>

static BlockHeader base;
BlockHeader *freep;

BlockHeader* morecore(size_t nu);

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
void *malloc(size_t nbytes)
{
	BlockHeader *p, *prevp;
	unsigned nunits;

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
			return (void*) (p + 1);
		}

		if (p == freep)
			if ((p = morecore(nunits)) == NULL)
			{
				//wprintf(L"[malloc] out of memory\n");
				return NULL;
			}
	}
}
