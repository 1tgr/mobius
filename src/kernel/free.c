#include <malloc.h>

extern BlockHeader* freep;

void free(void *ap)
{
	BlockHeader *bp, *p;

	/* free needs to handle a NULL pointer */
	if (!ap)
		return;

	bp = (BlockHeader*) ap - 1;
    //wprintf(L"free: buf = %x [%d bytes]\n", ap, bp->s.size * sizeof(BlockHeader));
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
