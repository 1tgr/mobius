#include <malloc.h>
#include <os/os.h>

//! Requests a block of memory from the operating system.
/*!
 *	Because calls to the kernel are relatively expensive, libc requests memory 
 *		in large blocks, which malloc() splits up as needed. These blocks are
 *		multiples of the host's page size and are automatically allocated by
 *		malloc().
 *
 *	\param	nu	Size, in multiples of sizeof(BlockHeader), of the block to 
 *		allocate. This is rounded to PAGE_SIZE automatically, and increased
 *		to four pages if smaller.
 *	\return	A pointer to the start of the block allocated. This block is also
 *		added to the freed memory list.
 */
/*
BlockHeader* morecore(size_t nu)
{
	byte *cp;
	BlockHeader *up;
	dword bytes;
	extern BlockHeader* freep;

	bytes = ((nu * sizeof(BlockHeader)) + PAGE_SIZE) & -PAGE_SIZE;
	//if (bytes < PAGE_SIZE * 4)
		//bytes = PAGE_SIZE * 4;

	cp = vmmAlloc(bytes / PAGE_SIZE, NULL, 
		MEM_COMMIT | MEM_USER | MEM_READ | MEM_WRITE);
	//wprintf(L"libc morecore: allocated %d pages at %p\n", bytes / PAGE_SIZE, cp);

	nu = bytes / sizeof(BlockHeader);
	up = (BlockHeader*) cp;
	up->s.size = nu;
	free((void*) (up + 1));
	return freep;
}
*/

char *sbrk(int size)
{
	char *cp;
	size_t bytes;
	bytes = (size + PAGE_SIZE) & -PAGE_SIZE;
	cp = vmmAlloc(bytes / PAGE_SIZE, NULL, 
		MEM_COMMIT | MEM_USER | MEM_READ | MEM_WRITE);
	return cp;
}

size_t getpagesize()
{
	return PAGE_SIZE;
}