/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: verify.c,v 1.1 2001/11/05 18:31:45 pavlovskii Exp $")

/*
 *  Goes through the entire heap checking all pointers, tags for
 *  consistency. Should catch most casual heap corruption (overwriting
 *  the end of a malloc'ed chunk, etc..) Nonetheless, heap corrupters
 *  tend to be devious and ingenious in ways they corrupt heaps (Believe
 *  me, I know:-). We should probably do the same thing if DEBUG is not
 *  defined, but return 0 instead of aborting. If fullcheck is non-zero,
 *  it also checks that free blocks contain the magic pattern written
 *  into them when they were freed to make sure the program is not still
 *  trying to access those blocks.
 */
int
mal_verify(fullcheck)
int fullcheck;
{
#ifdef DEBUG
	REGISTER Word *ptr, *p, *blk, *blkend;
	int i;

	if (_malloc_loword == NULL) /* Nothing malloc'ed yet */
		return(0);

	for (i = 0; i < MAXBINS; i++) {
		ptr = _malloc_rovers[i];
		if (ptr != NULL) {
			ASSERT(i >= _malloc_firstbin, "firstbin too high");
			CHECKFREEPTR(ptr, "when verifying ROVER pointer");
		}
	}
	ASSERT(_malloc_rovers[MAXBINS] == NULL, "ROVER terminator not NULL");
	for (ptr = _malloc_mem; ptr != NULL; ptr = ptr->next) {
		/*
		 *  Check arena bounds - not same as checking block tags,
		 *  despite similar appearance of the test
		 */
		ASSERT(SIZEFIELD(ptr+1) == SIZEFIELD(ptr + SIZE(ptr+1)),
		       "corrupt malloc arena");
		blkend = ptr + SIZE(ptr + 1);
		for (blk = ptr + ARENASTART; blk < blkend; blk += SIZE(blk)) {
			ASSERT_SP(PTR_IN_HEAP(blk), "corrupt pointer", "", blk);
			ASSERT_SP(VALID_START_SIZE_FIELD(blk),
				"corrupt SIZE field", "", blk);
			if (TAG(blk) == FREE) {
				p = blk + FREESIZE(blk) - 1;
				ASSERT_EP(VALID_NEXT_PTR(p),
				       "corrupt NEXT pointer", "", p);
				ASSERT_EP(VALID_PREV_PTR(p),
					"corrupt PREV pointer", "", p);
				if (fullcheck) {
					/* Make sure all free blocks are filled with FREEMAGIC */
					int i, n;
					char *cp;

					n = (SIZE(blk) - FREE_OVERHEAD) *
					 sizeof(Word);
					cp = (char *) (blk + FREEHEADERWORDS);
					for (i = 0; i < n; i++, cp++) {
						ASSERT_SP(*cp == FREEMAGIC,
							"free block modified after free()",
							"", blk);
					}
				}
			} else {
				ASSERT_SP(VALID_MAGIC(blk),
					  "overwritten end of block", "", blk);
			}
		}
	}
#endif /* DEBUG */
	return(0);
}
