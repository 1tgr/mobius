/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: dumpheap.c,v 1.1 2001/11/05 18:31:43 pavlovskii Exp $")

int
__m_prblock(p, is_end_ptr, fp)
univptr_t p;
int is_end_ptr;
FILE *fp;
{
	Word *blk = (Word *) p;
	Word *blkend;
	ulong tag;
	ulong blksize;
	char buf[512];	/* long enough for the sprintfs below */

	if (blk == NULL)
		return 0;

	if ( ! PTR_IN_HEAP(blk)) {
		sprintf(buf, "  ** pointer 0x%lx not in heap\n", (ulong) blk);
		fputs(buf, fp);
		return 0;
	}
	blksize = SIZE(blk);
	tag = TAG(blk);
	if (is_end_ptr) {
		blkend = blk;
		blk -= blksize - 1;
	} else {
		blkend = blk + blksize - 1;
	}
	(void) sprintf(buf, "  %s blk: 0x%lx to 0x%lx, %lu (0x%lx) words",
		       tag == FREE ? "Free" : "Allocated", (ulong) blk,
		       (ulong) blkend, blksize, blksize);
	(void) fputs(buf, fp);
	if ( is_end_ptr && ! PTR_IN_HEAP(blk)) {
		sprintf(buf, "  ** start pointer 0x%lx not in heap\n",
			(ulong) blk);
		fputs(buf, fp);
		return 0;
	}
	if ( !is_end_ptr && ! PTR_IN_HEAP(blkend)) {
		sprintf(buf, "  ** end pointer 0x%lx not in heap\n",
			(ulong) blk);
		fputs(buf, fp);
		return 0;
	}
	if (tag == FREE) {
		int i, n;
		char *cp;

		(void) sprintf(buf, " next=0x%lx, prev=0x%lx\n",
			       (ulong) NEXT(blkend), (ulong) PREV(blkend));
		(void) fputs(buf, fp);
		/* Make sure free block is filled with FREEMAGIC */
		n = (blksize - FREE_OVERHEAD) * sizeof(Word);
		cp = (char *) (blk + FREEHEADERWORDS);
#ifdef DEBUG
		for (i = 0; i < n; i++, cp++) {
			if (*cp != FREEMAGIC) {
				(void) fputs("  ** modified after free().\n",
					     fp);
				break;
			}
		}
#endif
	} else {
#ifdef DEBUG
		if (!TOO_SMALL(blk)) {
			(void) sprintf(buf, " really %lu bytes\n",
				       (ulong) REALSIZE(blk));
			(void) fputs(buf, fp);
		} else {
			(void) fputs("\n", fp);
		}
#else
		(void) fputs("\n", fp);
#endif
	}
	if (TAG(blk) == FREE) {
		if( ! VALID_NEXT_PTR(blkend))
			(void) fputs("  ** bad next pointer\n", fp);
		if( ! VALID_PREV_PTR(blkend))
			(void) fputs("  ** bad prev pointer\n", fp);
	} else {
		if ( ! VALID_MAGIC(blk))
			(void) fputs("  ** end of block overwritten\n", fp);
	}
	if ( ! VALID_START_SIZE_FIELD(blk)) {
		sprintf(buf, "  ** bad size field: tags = 0x%lx, 0x%lx\n",
			SIZEFIELD(blk), SIZEFIELD(blkend));
		(void) fputs(buf, fp);
		return 0;
	}
	(void) fflush(fp);
	return 1;
}

/*
 * Similar to malloc_verify except that it prints the heap as it goes along.
 * The only ASSERTs in this routine are those that would cause it to wander
 * off into the sunset because of corrupt tags.
 */
void
mal_heapdump(fp)
FILE *fp;
{
	REGISTER Word *ptr;
	REGISTER Word *blk;
	REGISTER Word *blkend;
	int i;
	char buf[512];	/* long enough for the sprintfs below */

	if (_malloc_loword == NULL) { /* Nothing malloc'ed yet */
		(void) fputs("Null heap - nothing malloc'ed yet\n", fp);
		return;
	}
		
	(void) fputs("Heap printout:\n", fp);
	(void) fputs("Free list rover pointers:\n", fp);
	sprintf(buf, "  First non-null bin is %d\n", _malloc_firstbin);
	(void) fputs(buf, fp);
	for (i = 0; i < MAXBINS; i++) {
		if ((ptr = _malloc_rovers[i]) == NULL)
			continue;
		(void) sprintf(buf, "  %d: 0x%lx\n", i, (ulong) ptr);
		(void) fputs(buf, fp);
		if ( ! PTR_IN_HEAP(ptr))
			(void) fputs("  ** not in heap\n", fp);
		if ( ! VALID_END_SIZE_FIELD(ptr))
			(void) fputs("  ** bad end size field\n", fp);
		if ( ! VALID_NEXT_PTR(ptr))
			(void) fputs("  ** bad next pointer\n", fp);
		if ( ! VALID_PREV_PTR(ptr))
			(void) fputs("  ** bad prev pointer\n", fp);
	}
	if (_malloc_rovers[MAXBINS] != NULL) {
		(void) sprintf(buf, "  ** rover terminator is 0x%lx, fixing\n",
			       (ulong) _malloc_rovers[MAXBINS]);
		(void) fputs(buf, fp);
		_malloc_rovers[MAXBINS] = NULL;
	}
	for (ptr = _malloc_mem; ptr != NULL; ptr = ptr->next) {
		/* print the arena */
		(void) sprintf(buf, "Arena from 0x%lx to 0x%lx, %lu (0x%lx) words\n",
			       (ulong) ptr, (ulong) (ptr + SIZE(ptr+1)),
			       (ulong) SIZE(ptr+1)+1, (ulong) SIZE(ptr+1)+1);
		(void) fputs(buf, fp);
		(void) sprintf(buf, "Next arena is 0x%lx\n", (ulong)ptr->next);
		(void) fputs(buf, fp);
		(void) fflush(fp);
		ASSERT(SIZEFIELD(ptr+1) == SIZEFIELD(ptr + SIZE(ptr+1)),
		       "mal_dumpheap: corrupt malloc arena");
		blkend = ptr + SIZE(ptr + 1);
		for (blk = ptr + ARENASTART; blk < blkend; blk += SIZE(blk)) {
			if (!__m_prblock((univptr_t) blk, 0, fp)) {
				__m_botch("mal_dumpheap: corrupt block", "",
					  (univptr_t)0,0,__FILE__,__LINE__);
			}
		}
	}
	(void) fputs("==============\n", fp);
	(void) fflush(fp);
}
