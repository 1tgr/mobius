#include <malloc.h>

size_t msize(void* memblock)
{
	BlockHeader* hdr = (BlockHeader*) memblock - 1;
	return hdr->s.size * sizeof(*hdr);
}