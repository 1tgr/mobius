#include <malloc.h>

extern BlockHeader* freep;

//! Frees a block of memory previously allocated by malloc().
/*!
 *	Each block of memory allocated by malloc() must be freed using free(), 
 *		or the program will create memory leaks. However, all memory used by 
 *		the process is freed automatically on exit, so any memory leaks will
 *		be restriced to an individual process.
 *
 *	The memory block can no longer be used after it has been freed.
 *
 *	If a NULL pointer is passed to free(), it returns immediately without
 *		error. Otherwise, no checking is performed on the pointer.
 *	\param	ap	Pointer to the start of the block to be freed.
 */
void free(void *ap)
{
	BlockHeader *bp, *p;

	/* free needs to handle a NULL pointer */
	if (!ap)
		return;

	bp = (BlockHeader*) ap - 1;
    //printf("free: buf = %x [%d bytes]\n", ap, bp->s.size * sizeof(BlockHeader));
	for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
		if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
			break;

	if (bp + bp->s.size == p->s.ptr)
	{
		bp->s.size += p->s.ptr->s.size;
		bp->s.ptr = p->s.ptr->s.ptr;
	}
	else
		bp->s.ptr = p->s.ptr;

	if (p + p->s.size == bp)
	{
		p->s.size += bp->s.size;
		p->s.ptr = bp->s.ptr;
	}
	else
		p->s.ptr = bp;

	freep = p;
}
