#include <malloc.h>

size_t __msize_core(void* memblock)
{
	BlockHeader* hdr = (BlockHeader*) memblock - 1;
	return hdr->s.size * sizeof(*hdr);
}