/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: stats.c,v 1.1 2001/11/05 18:31:45 pavlovskii Exp $")

/*
 *  Dumps the distribution of allocated sizes we've gathered so far
 */
void
mal_statsdump(fp)
FILE *fp;
{
#ifdef PROFILESIZES
	int i;
	char buf[128];

	for (i = 1; i < MAXPROFILESIZE; i++) {
		if(_malloc_scount[i] > 0) {
			(void) sprintf(buf, "%lu: %lu\n",(ulong)i*sizeof(Word),
				       (ulong) _malloc_scount[i]);
			(void) fputs(buf, fp);
			_malloc_scount[i] = 0;
		}
	}
	if (_malloc_scount[0] > 0) {
		(void) sprintf(buf, ">= %lu: %lu\n",
			       (ulong) MAXPROFILESIZE * sizeof(Word), 
			       (ulong) _malloc_scount[0]);
		(void) fputs(buf, fp);
		_malloc_scount[0] = 0;
	}
	(void) fflush(fp);
#endif /* PROFILESIZES */
}
