#include <malloc.h>
#include <string.h>
#include <stdlib.h>

void *realloc(void *memblock, size_t size)
{
	size_t oldsize;
	void* newblock;

	if (memblock)
		oldsize = msize(memblock);
	else
		oldsize = 0;

	newblock = malloc(size);
	if (memblock && newblock)
		memcpy(newblock, memblock, min(size, oldsize));

	free(memblock);
	return newblock;
}