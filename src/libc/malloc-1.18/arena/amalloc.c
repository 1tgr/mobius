/*  This file should be edited with 4-column tabs! */
/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "adefs.h"
#include "arena.h"

RCSID("$Header: /cvsroot/mobius/mn/src/libc/malloc-1.18/arena/amalloc.c,v 1.1 2001/11/05 18:31:47 pavlovskii Exp $");

static Arena colosseum = AINIT;

void
ainit(ap)
Arena *ap;
{
    *ap = colosseum;
}

static int
grabhunk(ap, nwords)
Arena *ap;
size_t nwords;
{
	univptr_t cp;
	int morecore;
	Word *ptr;
	size_t sbrkwords;
	size_t blksize;
	
	/*
	 * two words for fake boundary tags for the entire block, and one
	 * for the next ptr of the block.
	 */
#define EXCESS 3
	sbrkwords = (size_t) (((nwords + EXCESS) / ap->sbrkunits + 1) *
	 ap->sbrkunits);
	morecore = sbrkwords * sizeof(Word) + SBRKEXTRA;
	if ((cp = (univptr_t) sbrk(morecore)) == (univptr_t) -1)
		return(0);
	/* 
	 * Should first GUARANTEE that what sbrk returns is aligned to
	 * Word boundaries - see align.h. Unfortunately, to guarantee
	 * that the pointer returned by sbrk is aligned on a word
	 * boundary, we must ask for sizeof(Word) -1 extra bytes, since
	 * we have no guarantee what other sbrk'ed blocks exist. (Sun
	 * sbrk always returns an aligned value, that is another story!)
	 * We use spare and nspare to keep track of the bytes wasted, so
	 * that we can try and reuse them later. If no other sbrk()s are
	 * called, then nspare rotates through the values 3, 2, 1, 0,
	 * and the first branch of the if() is always taken.
	 */
	if ((ap->spare + ap->nspare) == (char *) cp) {
		ptr = (Word *) SBRKALIGN(ap->spare);
		morecore += ap->nspare;
		sbrkwords = morecore / sizeof(Word);
	} else {
		ptr = (Word *) SBRKALIGN(cp);
		morecore -= (char *) ptr - (char *) cp;
	}
	ap->spare = (char *) (ptr + sbrkwords);
	ap->nspare = (morecore - sbrkwords * sizeof(Word));
	ap->totalavail += sbrkwords;
	PRTRACE(sprintf(ap->statsbuf, "sbrk %d\n", sbrkwords*sizeof(Word)));
	
	/* 
	 * Here we need to check if it adjoins the ap->hiword.  If
	 * it does, then ap->hiword need not be a fake boundary tag
	 * any longer, (its a real one) and the higher end of the block
	 * we sbrk'ed is the fake tag.  So we tag it appropriately, make
	 * the start of the block point to the old ap->hiword, and
	 * free it.  If we aren't next to ap->hiword, then someone
	 * else sbrk'ed in between, so we can't coalesce over the
	 * boundary anyway, in which case we just change ap->hiword
	 * to be in the new sbrk'ed block without damaging the old one.
	 * And we free the block.
	 */
	if (ptr != ap->hiword + 1) {
		/* Non-contiguous sbrk'ed block, or first sbrk we've done. */
		/* 
		 * First push this block on the stack of non-contiguous blocks
		 * we've sbrked. !! For real paranoia, we'd also check
		 * ap->mem...
		 */
		REGISTER Word *tmp = ap->mem;

		ap->mem = ptr;
		ptr->next = tmp;
		ptr++;
		sbrkwords--;
		
		ap->hiword = ptr;
		if (ap->loword == NULL) {
			/* First time - set lower bound. */
			PRTRACE(sprintf(ap->statsbuf, "heapstart 0x%x\n", ptr));
			ap->loword = ptr;
		}
		/*
		 *  Fake boundary tags to indicate the ends of an arena. Since they
		 *  are marked as allocated, no attempt will be made to coalesce
		 *  before or after them.
		 */
		SIZEFIELD(ptr) = ALLOCED | sbrkwords;
		ap->hiword += sbrkwords - 1;
		PRTRACE(sprintf(ap->statsbuf, "heapend 0x%x\n", ap->hiword));
		SIZEFIELD(ap->hiword) = ALLOCED | sbrkwords;
		
		ptr++;
		/*
		 *  The 2 we subtract are the special arena end tags, which is
		 *  why we don't use HEADERWORDS and TRAILERWORDS
		 */
		sbrkwords -= 2;
		SIZEFIELD(ptr) = FREEMASK(sbrkwords);
		DMEMSET(ptr + FREEHEADERWORDS, sbrkwords - FREE_OVERHEAD);
		ptr = ap->hiword - 1;
		SIZEFIELD(ptr) = FREEMASK(sbrkwords);
		/* links */
		if (ap->rover == NULL) {
			/* Only block in free list - links point to itself */
			NEXT(ptr) = ptr;
			PREV(ptr) = ptr;
		} else {
			/*
			 *  Non-contiguous sbrk - insert into free list. No
			 *  point calling free() because we know this cannot be
			 *  coalesced
			 */
			NEXT(ptr) = ap->rover;
			tmp = PREV(ap->rover);
			PREV(ptr) = tmp;	/* PREV(ptr) = PREV(ap->rover); */
			NEXT(tmp) = ptr;	/* NEXT(PREV(ap->rover)) = ptr; */
			PREV(ap->rover) = ptr;
		}
		ap->rover = ptr;
		return(1);
	}
	/* Set boundary tags and size */
	ptr--;
	blksize = SIZE(ptr) + sbrkwords;
	SIZEFIELD(ptr) = ALLOCMASK(sbrkwords);
	ap->hiword += sbrkwords;
	SIZEFIELD(ap->hiword-1) = SIZEFIELD(ptr);
	/* Update fake end tags of the memory chunk */
	SIZEFIELD(ap->hiword) = ALLOCMASK(blksize);
	SIZEFIELD(ap->hiword - blksize + 1) = ALLOCMASK(blksize);
	SET_REALSIZE(ptr, (sbrkwords - ALLOC_OVERHEAD) * sizeof(Word));
	afree(ap, (univptr_t) (ptr + HEADERWORDS));
	return(1);
}

univptr_t
amalloc(ap, nbytes)
Arena *ap;
size_t nbytes;
{
	REGISTER Word *start, *search;
	REGISTER Word *p;
	REGISTER size_t required;
	REGISTER size_t searchsize;
	REGISTER size_t rest;
	size_t roversize;

#ifndef BUGCOMPATIBILITY
	ASSERT(nbytes != 0, "What do you expect when you malloc(0)?!!");
	if (nbytes == 0) { /* If we're debugging, then we died on the ASSERT */
		errno = EINVAL;
		return(NULL);
	}
#endif /* BUGCOMPATIBILITY */

	required = ALLOC_OVERHEAD + (nbytes + sizeof(Word) - 1) /
	 sizeof(Word);
	if (required < (size_t) ap->minchunk)
		required = ap->minchunk;
	search = ap->rover;
	if (!search || required > ap->totalavail) {
		/* Not enough memory in free list - allocate enough memory. */
		if (grabhunk(ap, required) == 0) {
			errno = ENOMEM;
			return( NULL);
		}
		search = ap->rover;
	}

	ASSERT(PTR_IN_HEAP(ap->rover), "corrupt rover pointer in malloc()");
	ASSERT(VALID_END_SIZE_FIELD(ap->rover), "corrupt block in malloc()");
	ASSERT(VALID_NEXT_PTR(ap->rover), "corrupt block in malloc()");
	ASSERT(VALID_PREV_PTR(ap->rover), "corrupt block in malloc()");
	CHECKHEAP();

	start = search;
	roversize = FREESIZE(search);
	do {
		ASSERT(PTR_IN_HEAP(search), "corrupt pointer in malloc()");
		ASSERT(VALID_END_SIZE_FIELD(search), "corrupt pointer in malloc()");
		ASSERT(VALID_NEXT_PTR(search), "corrupt pointer in malloc()");
		ASSERT(VALID_PREV_PTR(search), "corrupt pointer in malloc()");
		searchsize = FREESIZE(search);
		if (searchsize >= required) {
			break;
		} else {
			search = NEXT(search);
		}
	} while (search != start);
	
	if (searchsize < required) {
		if (grabhunk(ap, required) == 0) {
			errno = ENOMEM;
			return( NULL);
		}
		/* 
		 * We made sure in grabhunk() or afree() that
		 * ap->rover is pointing to the newly sbrked (and
		 * freed) block.
		 */
		search = ap->rover;
		roversize = searchsize = FREESIZE(search);
	}
	rest = searchsize - required;
	if (rest >= ap->minchunk) {
		SIZEFIELD(search) = FREEMASK(rest);
		p = search - rest;
		SIZEFIELD(p+1) = FREEMASK(rest);
		SIZEFIELD(p) = ALLOCMASK(required);
		p -= required - 1;
		SIZEFIELD(p) = ALLOCMASK(required);
		ap->totalavail -= required;
		/* keep rover at the larger block */
		if (rest > roversize)
			ap->rover = search;
	} else {
		/* alloc the entire block */
		REGISTER Word *nextp = NEXT(search);
		
		SIZEFIELD(search) |= ALLOCED;
		p = search - searchsize + 1;
		SIZEFIELD(p) = SIZEFIELD(search);
		if (search == nextp) {
			ap->rover = NULL;
		} else {
			REGISTER Word *prevp = PREV(search);
			
			NEXT(prevp) = nextp;
			PREV(nextp) = prevp;
			/* keep rover at the larger block, unless we just allocated rover*/
			if (search == ap->rover || FREESIZE(nextp) > roversize)
				ap->rover = nextp;
		}
		ap->totalavail -= searchsize;
	}
	PRTRACE(sprintf(ap->statsbuf, "+ %u %u 0x%x\n", nbytes,
	 (SIZE(p) - ALLOC_OVERHEAD)*sizeof(Word), p + HEADERWORDS));
	COUNTSIZE(SIZE(p));
	SET_REALSIZE(p, nbytes);
	return((univptr_t) (p + HEADERWORDS));
}



void
afree(ap, cp)
Arena *ap;
univptr_t cp;
{
	/* 
	 * This is where the boundary tags come into their own. The
	 * boundary tag guarantees a constant time insert with full
	 * coalescing (the time varies slightly for the four case possible,
	 * but still, essentially a very fast free.
	 */
	/* 
	 * P0 is the block being freed. P1 is the pointer to the block
	 * before the block being freed, and P2 is the block after it.
	 * We can either coalesce with P1, P2, both, or neither
	 */
	REGISTER Word *p0, *p1, *p2;

	if (cp == NULL)
		return;
		
	p0 = (Word *) cp;
	p0 -= HEADERWORDS;

	/* A little paranoia... */
	ASSERT(PTR_IN_HEAP(p0), "bad pointer passed to afree()");
	ASSERT(TAG(p0) != FREE, "freed block passed to afree()");
	ASSERT(VALID_START_SIZE_FIELD(p0), "corrupt block passed to afree()");
	ASSERT(VALID_MAGIC(p0), "block with end overwritten passed to afree()");
	/* With debugging, the assert would have already aborted */
	if (TAG(p0) == FREE) { 
		errno = EINVAL;
		return;
	}

	/* 
	 * clear the entire block that used to be p0's, just in case
	 * someone tries to refer to it or anything in it again.  We
	 * leave the end tags alone for now - we'll smash them
	 * individually depending on the way p0 merges with p1 and/or p2.
	 */
	DMEMSET(p0 + FREEHEADERWORDS, SIZE(p0) - FREE_OVERHEAD);
	PRTRACE(sprintf(ap->statsbuf, "- %u 0x%x\n",
	 (SIZE(p0) - ALLOC_OVERHEAD)*sizeof(Word), p0 + HEADERWORDS));
	ap->totalavail += SIZE(p0);

	p1 = p0 - 1;
	/*
	 * p0 now points to the end of the block -- we start treating it as
	 * a free block
	 */
	p0 += SIZE(p0) - 1;
	p2 = p0 + 1;

	/*
	 * More paranoia.... We can't match the SIZEFIELDs of p1/p2 with
	 * p1/p2 + SIZE(p1/p2) -1 because they might be a fake tag to
	 * indicate the bounds of the arena. Further, we should only check
	 * p1 if p0-1 is not the ap->loword or an arena bound - else p1 is
	 * probably not a valid pointer. If tag p0-1 is allocated, then it
	 * could be an arena bound.
	 */

	if (TAG(p2) == FREE) {
		/*
		 * Aha - block which is physically after p0 is free.
		 * Merging with it merely means increasing its size to
		 * incorporate the block being freed - no pointer
		 * shuffling.
		 */
		p2 += FREESIZE(p2) - 1;
		ASSERT(PTR_IN_HEAP(p2), "corrupt block in afree()");
		ASSERT(TAG(p2)==FREE, "corrupt block in afree()");
		ASSERT(VALID_END_SIZE_FIELD(p2), "corrupt block in afree()");
		ASSERT(VALID_NEXT_PTR(p2), "corrupt block in afree()");
		ASSERT(VALID_PREV_PTR(p2), "corrupt block in afree()");
		
		SIZEFIELD(p2) = FREEMASK(FREESIZE(p2) + SIZE(p0));
		SIZEFIELD(p2 - FREESIZE(p2) + 1) = SIZEFIELD(p2);
		/*
		 *  Smash p0's old end tag and p2's old start tag.
		 */
		DMEMSET(p0 - FREETRAILERWORDS + 1, FREETRAILERWORDS + FREEHEADERWORDS);
		p0 = p2; /* p0 just vanished - became part of p2 */
	}
	if (TAG(p1) == FREE) {
		/* 
		 * Block that physically precedes p0 in memory is free. Merging
		 * with it means rearranging the links to because the end of
		 * the block (the handle it is known by) is now the end of p0
		 * rather than itself. So the blocks after and before it in the
		 * free list need to be told.
		 */
		REGISTER Word *nextp1, *prevp1;
		
		ASSERT(PTR_IN_HEAP(p1), "corrupt block in afree()");
		ASSERT(VALID_END_SIZE_FIELD(p1), "corrupt block in afree()");
		ASSERT(VALID_NEXT_PTR(p1), "corrupt block in afree()");
		ASSERT(VALID_PREV_PTR(p1), "corrupt block in afree()");
		
		/* p0 grows to absorb p1 */
		SIZEFIELD(p0) = FREEMASK(SIZE(p0) + FREESIZE(p1));
		SIZEFIELD(p0 - FREESIZE(p0) + 1) = SIZEFIELD(p0);
		nextp1 = NEXT(p1);
		prevp1 = PREV(p1);
		/*
		 * We smash the free list pointers in p1 (SIZE, NEXT, PREV) to
		 * make sure no one refers to them again. We cannot smash the
		 * start boundary tag because in both cases, it becomes the start
		 * tag for the new block.  We also trash p0's start tag.
		 */
		DMEMSET(p1 - FREETRAILERWORDS + 1, FREETRAILERWORDS + FREEHEADERWORDS);
		if (p0 != p2) {
			/* 
			 * Ok - p0 coalesced with the block physically
			 * before it (p1) (which is why we're here, but
			 * it didn't coalesce with the block after it
			 * (p2) which is why p0 != p2.  So we need to
			 * insert p0 in the list in place of p1.
			 */
			
			if (nextp1 != p1) {
				/* Fix the PREV ptr of the next blk in the list */
				PREV(nextp1) = p0;
				/* Fix the NEXT ptr of the previous blk in the list */
				NEXT(prevp1) = p0;
				/* Copy the link info from p1 to p0 */
				NEXT(p0) = nextp1;
				PREV(p0) = prevp1;
			} else {
				NEXT(p0) = p0;
				PREV(p0) = p0;
			}
			/* p1 just vanished - became part of p0 */
			ap->rover = p0;
			CHECKHEAP();
			return;
		} else {
			/* 
			 * p0 merged with p2, and with p1, which
			 * essentially means that p2 grows to absorb p1
			 * in the free list (bridged by p0). So we
			 * simply delete p1. Free list reduces by one
			 * blk.
			 */
			/* Fix the PREV ptr of the next blk in the list */
			PREV(nextp1) = prevp1;
			/* Fix the NEXT ptr of the previous blk in the list */
			NEXT(prevp1) = nextp1;
		}
	}
	if (p0 != p2) {
		/* 
		 * If we're here, it means block P0 didn't coalesce, so
		 * we need to insert it in the free list - we put it
		 * before ROVER, and make ROVER point to it. Or it means
		 * ROVER was NULL, i.e. free list is empty, which means
		 * we have to take care of the boundary linking Free
		 * list grows by one.
		 */
		if (ap->rover == NULL) {
			/*
			 * Free list was empty - so we point
			 * ap->rover at the block we're freeing to
			 * get a proper circular linking.
			 */
			ap->rover = p0;
		} else {
			ASSERT(PTR_IN_HEAP(ap->rover),
			 "corrupt rover pointer in afree()");
			ASSERT(VALID_END_SIZE_FIELD(ap->rover),
			 "corrupt block in afree()");
			ASSERT(VALID_NEXT_PTR(ap->rover), "corrupt block in afree()");
			ASSERT(VALID_PREV_PTR(ap->rover), "corrupt block in afree()");
		}
		NEXT(p0) = ap->rover;
		PREV(p0) = PREV(ap->rover);
		PREV(ap->rover) = p0;
		NEXT(PREV(p0)) = p0;
		SIZEFIELD(p0) &= SIZEMASK; /* sets the boundary tag to FREE */
		SIZEFIELD(p0 - FREESIZE(p0) + 1) = SIZEFIELD(p0);
	}
	ap->rover = p0;

	CHECKHEAP();
	return;
}



/* 
 * WARNING: This realloc() IS *NOT* upwards compatible with the
 * convention that the last freed block since the last malloc may be
 * realloced. Allegedly, this was because the old free() didn't coalesce
 * blocks, and reallocing a freed block would perform the compaction.
 * Yuk!
 */
univptr_t
arealloc(ap, cp, nbytes)
Arena *ap;
univptr_t cp;
size_t nbytes;
{
	REGISTER Word *p0 = (Word *) cp;
	REGISTER Word *p1;
	univptr_t tmp;
	REGISTER size_t required;
	REGISTER size_t sizep0;

	if (p0 == NULL)
		return(amalloc(ap, nbytes));

	if (nbytes == 0) {
		afree(ap, cp);
		return(NULL);
	}
	
	required = ALLOC_OVERHEAD + (nbytes + sizeof(Word) - 1) /
	 sizeof(Word);

	p0 -= HEADERWORDS;
	
	/* A little paranoia... */
	ASSERT(PTR_IN_HEAP(p0), "bad pointer passed to realloc()");
	ASSERT(TAG(p0) != FREE, "freed block passed to realloc()");
	ASSERT(VALID_START_SIZE_FIELD(p0), "corrupt block passed to realloc()");
	ASSERT(VALID_MAGIC(p0), "block with end overwritten passed to realloc()");
	/* With debugging, the assert would have already aborted */
	if (TAG(p0) == FREE) { 
		errno = EINVAL;
		return(NULL);
	}
	sizep0 = SIZE(p0);
	if (sizep0 >= required) {
		/* Shrinking the block */
		size_t after = sizep0 - required;
		
		SET_REALSIZE(p0, nbytes);
		if (after < ap->minchunk) {
			/* 
			 *  Not enough to free what's left so we return the block
			 *  intact - print no-op for neatness in output.
			 */
			PRTRACE(strcpy(ap->statsbuf, "no-op\n"));
			return(cp);
		}
		SIZEFIELD(p0) = ALLOCMASK(required);
		SIZEFIELD(p0 + required - 1) = SIZEFIELD(p0);
		p0 += required;
		/* 
		 * We free what's after the block - mark it alloced and
		 * throw it to free() to figure out whether to merge it
		 * with what follows...
		 */
		SIZEFIELD(p0) = ALLOCMASK(after);
		SIZEFIELD(p0 + after - 1) = SIZEFIELD(p0);
		SET_REALSIZE(p0, (after - ALLOC_OVERHEAD) * sizeof(Word));
		afree(ap, (univptr_t) (p0 + HEADERWORDS));
		return(cp);
	}
		
	/*
	 * If we get here, then we are growing the block to something
	 * bigger. If the following block is free and big enough to be
	 * realloced, then we grow using that block. This resembles the
	 * malloc code slightly.
	 */
	p1 = p0 + sizep0;
	required -= sizep0;
	if (TAG(p1) == FREE) { /* p1 not free, may be an arena bound or hiword */
		ASSERT(PTR_IN_HEAP(p1), "corrupt pointer in realloc()");
		ASSERT(VALID_START_SIZE_FIELD(p1), "corrupt block in realloc()");
		ASSERT(VALID_NEXT_PTR(p1 + FREESIZE(p1) - 1),
		 "corrupt block in realloc()");
		ASSERT(VALID_PREV_PTR(p1 + FREESIZE(p1) - 1),
		 "corrupt block in realloc()");
	}
	if (TAG(p1) == FREE && FREESIZE(p1) >= required) {
		size_t rest = FREESIZE(p1) - required;
		REGISTER Word *p;

		if (rest >= ap->minchunk) {
			sizep0 += required;
			SIZEFIELD(p0) = ALLOCMASK(sizep0);
			p = p0 + sizep0;
			SIZEFIELD(p-1) = SIZEFIELD(p0);;
			SIZEFIELD(p) = FREEMASK(rest);
			SIZEFIELD(p + rest - 1) = FREEMASK(rest);
			ap->totalavail -= required;
		} else {
			/*
			 * alloc the entire block and merge into p0. Free list
			 * shrinks by a block
			 */
			REGISTER Word *nextp1, *prevp1;
			
			sizep0 += FREESIZE(p1);
			SIZEFIELD(p0) = ALLOCMASK(sizep0);
			SIZEFIELD(p0 + sizep0 - 1) = SIZEFIELD(p0);
			p1 += FREESIZE(p1) - 1;
			p = nextp1 = NEXT(p1);
			if (p1 == nextp1) {
				ap->rover = NULL;
			} else {
				prevp1 = PREV(p1);
				PREV(nextp1) = prevp1;
				NEXT(prevp1) = nextp1;
				ap->rover = nextp1;
			}
			ap->totalavail -= SIZE(p1);
		}
		SET_REALSIZE(p0, nbytes);
		CHECKHEAP();

		PRTRACE(sprintf(ap->statsbuf, "++ %u %u 0x%x %u 0x%x\n",
		 nbytes, (SIZE(p0) - ALLOC_OVERHEAD)*sizeof(Word), cp,
		 SIZE(p)*sizeof(Word), p));
		return(cp);
	}
	/* Have to do it the hard way */
	tmp = amalloc(ap, nbytes);
	if (tmp != NULL) {
		MEMCPY(tmp, cp, ((SIZE(p0) - ALLOC_OVERHEAD)));
		afree(ap, cp);
	}
	return(tmp);
}



/* 
 * !! Given what we know about alignment, we should be able to do better
 * than memset and set words. Hopefully memset has been tuned.
 */
univptr_t
acalloc(ap, nelem, elsize)
Arena *ap;
size_t nelem, elsize;
{
	REGISTER size_t nbytes = nelem * elsize;
	REGISTER univptr_t cp = amalloc(ap, nbytes);

	if (cp)
		(void) memset((univptr_t) cp, 0, (memsize_t) nbytes);
	return(cp);
}
