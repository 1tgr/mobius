/* $Id$ */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <os/defs.h>
#include <os/syscall.h>

typedef union block_t block_t;
union block_t
{
	struct
	{
		size_t size;
		const char *file;
		int line;
	} allocated;

	struct
	{
		size_t size;
		block_t *next;
	} free;
};


static block_t *free_first, *free_last;

void *__morecore(size_t *bytes)
{
	*bytes = max(PAGE_ALIGN_UP(*bytes), 4 * PAGE_SIZE);
	return VmmAlloc(*bytes / PAGE_SIZE,
		0,
		VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
}


void *__malloc(size_t size, const char *file, int line)
{
	block_t *prev, *block;
	size_t remainder;

	size = ((size + sizeof(block_t) - 1) & -sizeof(block_t)) + sizeof(block_t);

	do
	{
		prev = NULL;
		block = free_first;
		while (block != NULL)
		{
			if (block->free.size >= size)
				break;

			prev = block;
			block = block->free.next;
		}

		if (block == NULL)
		{
			block_t *slab;
			size_t bytes;

			bytes = size;
			slab = __morecore(&bytes);
			if (slab == NULL)
				return NULL;

			slab->free.size = bytes;
			slab->free.next = NULL;

			if (free_last != NULL)
				free_last->free.next = slab;
			if (free_first == NULL)
				free_first = slab;
			free_last = slab;
		}
	} while (block == NULL);

	remainder = block->free.size - size;
	if (remainder == 0)
	{
		if (prev == NULL)
			free_first = block->free.next;
		else
			prev->free.next = block->free.next;

		if (block->free.next == NULL)
			free_last = prev;
	}
	else
	{
		block->free.size = remainder;
		block = (block_t*) ((char*) block + remainder);
	}

	block->allocated.size = size;
	block->allocated.file = file;
	block->allocated.line = line;

	return block + 1;
}


void __free(void *ptr, const char *file, int line)
{
	block_t *block;

	if (ptr == NULL)
		return;

	block = (block_t*) ptr - 1;
	block->free.size = block->allocated.size;
	block->free.next = NULL;
	if (free_last != NULL)
		free_last->free.next = block;
	free_last = block;

	if (free_first == NULL)
		free_first = block;
}


void *__realloc(void *ptr, size_t size, const char *file, int line)
{
	void *new_ptr;
	block_t *old_block;

	new_ptr = __malloc(size, file, line);
	if (new_ptr == NULL)
		return NULL;

	if (ptr != NULL)
	{
		old_block = (block_t*) ptr - 1;
		memcpy(new_ptr, ptr, min(old_block->allocated.size, size));
		free(ptr);
	}

	return new_ptr;
}
