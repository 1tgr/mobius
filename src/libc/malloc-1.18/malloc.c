/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.c"

RCSID("$Id: malloc.c,v 1.1 2001/11/05 18:31:43 pavlovskii Exp $")

/*
 * GETBIN, UNLINK, LINK and CARVE are free-list maintenance macros used in
 * several places.  A free-list is a doubly-linked list of non-contiguous
 * blocks, marked by boundary tags that indicate the size.
 */

/* GETBIN returns a number such that i <= _malloc_binmax[bin] */
#define GETBIN(i)   \
	(((i) <= _malloc_binmax[3]) ? \
		(((i) <= _malloc_binmax[1]) ? \
			(((i) <= _malloc_binmax[0]) ? 0 : 1) \
		: \
			(((i) <= _malloc_binmax[2]) ? 2 : 3) \
		) \
	: \
		(((i) <= _malloc_binmax[5]) ? \
			(((i) <= _malloc_binmax[4]) ? 4 : 5) \
		: \
			(((i) <= _malloc_binmax[6]) ? 6 : 7) \
		) \
	)

/* UNLINK removes the block 'ep' from the free list 'epbin' */
#define UNLINK(ep, epbin) \
	{ \
		REGISTER Word *epnext = NEXT(ep); \
		if (ep == epnext) { \
			_malloc_rovers[epbin] = NULL; \
			if (_malloc_firstbin == epbin) \
				while (! _malloc_rovers[_malloc_firstbin] && \
				       _malloc_firstbin < MAXBINS-1) \
					_malloc_firstbin++; \
		} else { \
			REGISTER Word *epprev = PREV(ep); \
			NEXT(epprev) = epnext; \
			PREV(epnext) = epprev; \
			if (ep == _malloc_rovers[epbin]) \
				_malloc_rovers[epbin] = epprev; \
		} \
	}

/*
 * LINK adds the block 'ep' (psize words) to the free list 'epbin',
 * immediately after the block pointed to by that bin's rover.
 */
#define LINK(ep, epsize, epbin) \
	{ \
		REGISTER Word *epprev; \
		REGISTER Word *eprover = _malloc_rovers[epbin]; \
		\
		if (eprover == NULL) { \
			_malloc_rovers[epbin] = eprover = epprev = ep; \
			if (_malloc_firstbin > epbin) \
				_malloc_firstbin = epbin; \
		} else { \
			CHECKFREEPTR(eprover, "while checking rover"); \
			epprev = PREV(eprover); \
		} \
		NEXT(ep) = eprover; \
		PREV(eprover) = ep; \
		NEXT(epprev) = ep; \
		PREV(ep) = epprev; /* PREV(eprover) */\
		SIZEFIELD(ep) = SIZEFIELD(ep-epsize+1) = FREEMASK(epsize); \
	}

#define CARVE(ep, epsize, epbin, reqsize) \
	 { \
		REGISTER size_t eprest = epsize - reqsize; \
		int newepbin; \
		\
		if (eprest >= _malloc_minchunk) { \
			newepbin = GETBIN(eprest); \
			if (newepbin != epbin) { \
				UNLINK(ep, epbin); \
				LINK(ep, eprest, newepbin); \
			} else { \
				SIZEFIELD(ep) = SIZEFIELD(ep-eprest+1) \
					= FREEMASK(eprest); \
			} \
		} else { \
			/* alloc the entire block */ \
			UNLINK(ep, epbin); \
			reqsize = epsize; \
		} \
	 }

static int
grabhunk(nwords)
size_t nwords;
{
	univptr_t cp;
	size_t morecore;
	Word *ptr;
	size_t sbrkwords;
	size_t blksize;
	static char *spare;
	static int nspare;
	
	/* 
	 *  two words for fake boundary tags for the entire block, and one
	 *  for the next ptr of the block.
	 */
#define EXCESS 3
	sbrkwords = (size_t) (((nwords + EXCESS) / _malloc_sbrkunits + 1) *
	 _malloc_sbrkunits);
	morecore = sbrkwords * sizeof(Word) + SBRKEXTRA;
	if ((cp = (* _malloc_memfunc)(morecore)) == (univptr_t) -1)
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
	if ((spare + nspare) == (char *) cp) {
		ptr = (Word *) SBRKALIGN(spare);
		morecore += nspare;
		sbrkwords = morecore / sizeof(Word);
	} else {
		ptr = (Word *) SBRKALIGN(cp);
		morecore -= (char *) ptr - (char *) cp;
	}
	spare = (char *) (ptr + sbrkwords);
	nspare = (morecore - sbrkwords * sizeof(Word));
	PRTRACE(sprintf(_malloc_statsbuf, "sbrk %lu\n",
			(ulong) sbrkwords*sizeof(Word)));
	
	/*
	 * If the new chunk adjoins _malloc_hiword, then _malloc_hiword
	 * need not be a fake boundary tag any longer, (its a real one) and
	 * the higher end of the block we sbrk'ed is the fake tag.  So we
	 * tag it appropriately, make the start of the block point to the
	 * old _malloc_hiword, and free it.  If we aren't next to
	 * _malloc_hiword, then someone else sbrk'ed in between, so we
	 * can't coalesce over the boundary anyway, in which case we just
	 * change _malloc_hiword to be in the new sbrk'ed block without
	 * damaging the old one. And we free the block.
	 */
	if (ptr != _malloc_hiword + 1 || _malloc_mem == NULL) {
		/* Non-contiguous sbrk'ed block, or first sbrk we've done. */
		/* 
		 * First push this block on the stack of non-contiguous blocks
		 * we've sbrked. !! For real paranoia, we'd also check
		 * _malloc_mem...
		 */
		REGISTER Word *tmp = _malloc_mem;

		_malloc_mem = ptr;
		ptr->next = tmp;
		ptr++;
		sbrkwords--;
		
		_malloc_hiword = ptr;
		if (_malloc_loword == NULL || _malloc_loword > ptr) {
			/* First time - set lower bound. */
			PRTRACE(sprintf(_malloc_statsbuf, "heapstart 0x%lx\n",
					(ulong) ptr));
			_malloc_loword = ptr;
		}
		/*
		 * Fake boundary tags to indicate the ends of an arena.
		 * Since they are marked as allocated, no attempt will be
		 * made to coalesce before or after them.
		 */
		SIZEFIELD(ptr) = ALLOCED | sbrkwords;
		_malloc_hiword += sbrkwords - 1;
		PRTRACE(sprintf(_malloc_statsbuf, "heapend 0x%lx\n",
				(ulong) _malloc_hiword));
		SIZEFIELD(_malloc_hiword) = ALLOCED | sbrkwords;
		
		/* * Subtract 2 for the special arena end tags. */
		sbrkwords -= 2;
		ptr++;
		DMEMSET(ptr + FREEHEADERWORDS, sbrkwords - FREE_OVERHEAD);
		ptr = _malloc_hiword - 1;
		_malloc_lastbin = GETBIN(sbrkwords);
		LINK(ptr, sbrkwords, _malloc_lastbin)
		_malloc_rovers[_malloc_lastbin] = ptr;
		while (_malloc_rovers[_malloc_firstbin] == NULL &&
		       _malloc_firstbin < MAXBINS-1)
			_malloc_firstbin++;
		return(1);
	}
	/*
	 * If we get here, then the sbrked chunk is contiguous, so we fake
	 * up the boundary tags and size to look like an allocated block
	 * and then call free()
	 */
	ptr--;
	blksize = SIZE(ptr) + sbrkwords;
	SIZEFIELD(ptr) = ALLOCMASK(sbrkwords);
	_malloc_hiword += sbrkwords;
	SIZEFIELD(_malloc_hiword-1) = SIZEFIELD(ptr);
	/* Update special arena end tags of the memory chunk */
	SIZEFIELD(_malloc_hiword) = ALLOCMASK(blksize);
	SIZEFIELD(_malloc_hiword - blksize + 1) = ALLOCMASK(blksize);
	SET_REALSIZE(ptr, (sbrkwords - ALLOC_OVERHEAD) * sizeof(Word));
	free((univptr_t) (ptr + HEADERWORDS));
	return(1);
}

univptr_t
malloc(nbytes)
size_t nbytes;
{
	REGISTER Word *start, *search = NULL;
	REGISTER Word *p;
	REGISTER size_t required;
	REGISTER size_t searchsize;
	int bin;

#ifdef SVID_MALLOC_0
	/* SVID requires that malloc(0) return NULL, ick! */
	if (nbytes == 0) {
		errno = EINVAL;
		return(NULL);
	}
#endif /* SVID_MALLOC_0 */

	required = ALLOC_OVERHEAD + (nbytes + sizeof(Word) - 1) / sizeof(Word);
	if (required < (size_t) _malloc_minchunk)
		required = _malloc_minchunk;

	searchsize = 0;
	bin = GETBIN(required);
	if (bin < _malloc_firstbin)
		bin = _malloc_firstbin;
	/* typically, we expect to execute this loop only once */
	while (searchsize < required && bin < MAXBINS) {
		if ((search = _malloc_rovers[bin++]) == NULL) {
			continue;
		}
		if (search == _malloc_hiword - 1) {
			/* avoid final "wilderness" block */
			CHECKFREEPTR(search, "while checking \"wilderness\" in malloc()");
			search = NEXT(search);
		}
		start = search;
		do {
			CHECKFREEPTR(search, "while searching in malloc()");
			searchsize = FREESIZE(search);
			if (searchsize >= required) {
				break;
			} else {
				search = NEXT(search);
			}
		} while (search != start);
	}

	if (searchsize < required) {
		if (grabhunk(required) == 0) {
			errno = ENOMEM;
			return(NULL);
		}
		/*
		 * We made sure in grabhunk() or free() that
		 * _malloc_rovers[lastbin] is pointing to the newly sbrked
		 * (and freed) block.
		 */
		bin = _malloc_lastbin;
		search = _malloc_rovers[bin];
		searchsize = FREESIZE(search);
	} else if (bin > 0) {
		bin--;
	}
	CARVE(search, searchsize, bin, required);
	p = search - searchsize + 1;
	SIZEFIELD(p) = SIZEFIELD(p + required - 1) = ALLOCMASK(required);
	PRTRACE(sprintf(_malloc_statsbuf, "+ %lu %lu 0x%lx\n", (ulong) nbytes,
			(ulong) (required - ALLOC_OVERHEAD) * sizeof(Word),
			(ulong) (p + HEADERWORDS)));
	COUNTSIZE(required);
	SET_REALSIZE(p, nbytes);
	return((univptr_t) (p + HEADERWORDS));
}



void
free(cp)
univptr_t cp;
{
	/* 
	 * This is where the boundary tags come into their own. The
	 * boundary tag guarantees a constant time insert with full
	 * coalescing (the time varies slightly for the four case possible,
	 * but still, essentially a very fast free.
	 */
	/*
	 *  P0 is the block being freed. P1 is the pointer to the block
	 *  before the block being freed, and P2 is the block after it.
	 *  We can either coalesce with P1, P2, both, or neither
	 */
	REGISTER Word *p0, *p1, *p2;
	REGISTER size_t sizep0;
	int bin, oldbin = -1;

	if (cp == NULL)
		return;
		
	p0 = (Word *) cp;
	p0 -= HEADERWORDS;

	CHECKALLOCPTR(p0, "passed to free()");
	/* With debugging, the CHECKALLOCPTR would have already aborted */
	if (TAG(p0) == FREE) { 
		errno = EINVAL;
		return;
	}

	/*
	 * clear the entire block that used to be p0's, just in case
	 * someone tries to refer to it or anything in it again.  We leave
	 * the end tags alone for now - we'll smash them individually
	 * depending on the way p0 merges with p1 and/or p2.
	 */
	sizep0 = SIZE(p0);
	DMEMSET(p0 + FREEHEADERWORDS, sizep0 - FREE_OVERHEAD);
	PRTRACE(sprintf(_malloc_statsbuf, "- %lu 0x%lx\n",
			(ulong) (sizep0 - ALLOC_OVERHEAD) * sizeof(Word),
			(ulong) (p0 + HEADERWORDS)));

	p1 = p0 - 1;
	/*
	 * p0 now points to the end of the block -- we start treating it as
	 * a free block
	 */
	p0 += sizep0 - 1;
	p2 = p0 + 1;

	/*
	 * We can't match the SIZEFIELDs of p1/p2 with p1/p2 + SIZE(p1/p2)
	 * -1 because they might be a fake tag to indicate the bounds of
	 * the arena. Further, we should only check p1 if p0-1 is not the
	 * _malloc_loword or an arena bound - else p1 is probably not a
	 * valid pointer. If tag p0-1 is allocated, then it could be an
	 * arena bound.
	 */

	if (TAG(p2) == FREE) {
		/*
		 * Aha - block p2 (physically after p0) is free.  Merging
		 * p0 with p2 merely means increasing p2's size to
		 * incorporate p0 - no other pointer shuffling needed.
		 * We'll move it to the right free-list later, if necessary.
		 */
		p2 += FREESIZE(p2) - 1;
		oldbin = GETBIN(FREESIZE(p2));
		CHECKFREEPTR(p2, "while checking p2 in free()");
		sizep0 += FREESIZE(p2);
		SIZEFIELD(p2- sizep0 + 1) = SIZEFIELD(p2) = FREEMASK(sizep0);
		/*  Smash p0's old end tag and p2's old start tag. */
		DMEMSET(p0 - FREETRAILERWORDS + 1, FREETRAILERWORDS + FREEHEADERWORDS);
		p0 = p2; /* p0 just vanished - became part of p2 */
	}
	if (TAG(p1) == FREE) {
		/*
		 * The block p1 (physically precedes p0 in memory) is free.
		 * We grow p0 backward to absorb p1 and delete p1 from its
		 * free list, since it no longer exists.
		 */
		CHECKFREEPTR(p1, "while checking p1 in free()");
		sizep0 += FREESIZE(p1);
		bin = GETBIN(FREESIZE(p1));
		UNLINK(p1, bin);
		SIZEFIELD(p0 - sizep0 + 1) = SIZEFIELD(p0) = FREEMASK(sizep0);
		/*
		 * smash the free list pointers in p1 (SIZE, NEXT, PREV) to
		 * make sure no one refers to them again. We cannot smash
		 * the start boundary tag because it becomes the start tag
		 * for the new block.  Also trash p0's start tag.
		 */
		DMEMSET(p1 - FREETRAILERWORDS + 1, FREETRAILERWORDS + FREEHEADERWORDS);
	}
	bin = GETBIN(sizep0);
	if (oldbin != bin) {
		/*
		 * If we're here, it means block P0 needs to be inserted in
		 * the correct free list, either because it didn't merge
		 * with anything, or because it merged with p1 so we
		 * deleted p1, or it merged with p2 and grew out p2's
		 * existing free-list.
		 */
		if (oldbin >= 0) {
			/* merged with P2, still in P2's free-list */
			UNLINK(p0, oldbin);
		}
		LINK(p0, sizep0, bin);
		_malloc_lastbin = bin;
		_malloc_rovers[bin] = p0;
	}

	CHECKHEAP();
	return;
}


/*
 *  WARNING: This realloc() IS *NOT* upwards compatible with the
 *  convention that the last freed block since the last malloc may be
 *  realloced. Allegedly, this was because the old free() didn't
 *  coalesce blocks, and reallocing a freed block would perform the
 *  compaction. Yuk!
 */
univptr_t
realloc(cp, nbytes)
univptr_t cp;
size_t nbytes;
{
	REGISTER Word *p0 = (Word *) cp;
	REGISTER Word *p1;
	univptr_t tmp;
	REGISTER size_t required;
	REGISTER size_t sizep0;
	int bin;

	if (p0 == NULL)
		return(malloc(nbytes));

	if (nbytes == 0) {
		free(cp);
		return(NULL);
	}
	
	required = ALLOC_OVERHEAD + (nbytes + sizeof(Word) - 1) /
		sizeof(Word);
	if (required < (size_t) _malloc_minchunk)
		required = _malloc_minchunk;

	p0 -= HEADERWORDS;
	
	CHECKALLOCPTR(p0, "passed to realloc()");
	/* With debugging, the CHECKALLOCPTR would have already aborted */
	if (TAG(p0) == FREE) { 
		errno = EINVAL;
		return(NULL);
	}
	sizep0 = SIZE(p0);
	if (sizep0 >= required) {
		/* Shrinking the block */
		size_t after = sizep0 - required;
		
		SET_REALSIZE(p0, nbytes);
		if (after < _malloc_minchunk) {
			/*
			 * Not enough to free what's left so we return the
			 * block intact - print no-op for neatness in
			 * trace output.
			 */
			PRTRACE(strcpy(_malloc_statsbuf, "no-op\n"));
			return(cp);
		}
		SIZEFIELD(p0+required-1) = SIZEFIELD(p0) = ALLOCMASK(required);
		p0 += required;
		/*
		 * We free what's after the block - mark it alloced and
		 * throw it to free() to figure out whether to merge it
		 * with what follows...
		 */
		SIZEFIELD(p0 + after - 1) = SIZEFIELD(p0) = ALLOCMASK(after);
		SET_REALSIZE(p0, (after - ALLOC_OVERHEAD) * sizeof(Word));
		free((univptr_t) (p0 + HEADERWORDS));
		return(cp);
	}
		
	/*
	 * If we get here, then we are growing the block p0 to something
	 * bigger.
	 */
	p1 = p0 + sizep0;
	required -= sizep0;
	if (TAG(p1) != FREE || FREESIZE(p1) < required) {
		/* Have to do it the hard way: block after us cannot be used */
		tmp = malloc(nbytes);
		if (tmp != NULL) {
			MEMCPY(tmp, cp, ((SIZE(p0) - ALLOC_OVERHEAD)));
			free(cp);
		}
		return(tmp);
	}
	/*
	 * block after us is free and big enough to provide the required
	 * space, so we grow into that block.
	 */
	p1 += FREESIZE(p1) - 1;
	CHECKFREEPTR(p1, "while checking p1 in realloc()");
	bin = GETBIN(FREESIZE(p1));
	CARVE(p1, FREESIZE(p1), bin, required);
	sizep0 += required;
	SIZEFIELD(p0) = SIZEFIELD(p0+sizep0-1) = ALLOCMASK(sizep0);
	SET_REALSIZE(p0, nbytes);
	PRTRACE(sprintf(_malloc_statsbuf, "++ %lu %lu 0x%lx\n",
			(ulong) nbytes,
			(ulong) (sizep0-ALLOC_OVERHEAD)*sizeof(Word),
			(ulong) cp));
	CHECKHEAP();
	return(cp);
}



/*
 *  !! Given what we know about alignment, we should be able to do better
 *  than memset and set words. Hopefully memset has been tuned.
 */
univptr_t
calloc(nelem, elsize)
size_t nelem, elsize;
{
	REGISTER size_t nbytes = nelem * elsize;
	REGISTER univptr_t cp = malloc(nbytes);

	if (cp)
		(void) memset((univptr_t) cp, 0, (memsize_t) nbytes);
	return(cp);
}


/*
 *  Why would anyone want this.... ?
 */
void
cfree(cp)
univptr_t cp;
{
	free(cp);
}
