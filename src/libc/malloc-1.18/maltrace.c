/*
 * Reads a malloc() trace that's been processed by tracemunge, and performs
 * the malloc/realloc/frees from that trace in the same sequence so we can
 * simulate the effects of changes in malloc algorithms on the application
 * which generated the trace.
 */
/* Mark Moraes, D. E. Shaw & Co. */

/*
 * TODO! Put a hash function in this code so we can get rid of tracemunge.
 * Use better parsing than sscanf.  split()?
 */

#include <stdio.h>
#include <string.h>
/*
 * ANSI systems had better have this.  Non-ANSI systems had better not
 * complain about things that are implicitly declared int or void.
 */
#if defined(STDHEADERS)
# include <stdlib.h>
# include <unistd.h>
#endif

#include "malloc.h"

char *progname;
/* For getopt() */
extern int getopt();
extern int optind;
extern char *optarg;

#define MAXTIME 100000
static char *bufs[MAXTIME];
static unsigned long sizes[MAXTIME];

unsigned long alloced = 0;
static unsigned long maxalloced = 0;

int verbose = 0;

#ifdef MYMALLOC
extern char *_malloc_loword;
extern char *_malloc_hiword;
#else
extern char *sbrk();
#endif

extern void doline();

void
usage()
{
	fprintf(stderr, "Usage: %s [-d] [-m mmapfile] [-T tracefile] [-v]\n",
		progname);
	exit(1);
}

int
main(argc, argv)
int argc;
char **argv;
{
	FILE *trfp = NULL;
	int c;
	char *before, *after, *trfname = NULL;
	unsigned long grew;
	int use_mmap = 0;
	char buf[1024];

	progname = argv[0] ? argv[0] : "(no-argv[0])";
	while((c = getopt(argc, argv, "dm:vT:")) != EOF) {
		/* optarg has the current argument if the option was followed by ':'*/
		switch (c) {
		case 'd':
			/* Full heap debugging - S-L-O-W */
#ifdef MYMALLOC
			mal_debug(3);
#endif
			break;
		case 'm':
			use_mmap = 1;
#ifdef MYMALLOC
			mal_mmap(optarg);
#else
			fprintf(stderr, "%s: -m option needs CSRI malloc\n",
				progname);
#endif
			break;
		case 'v':
			verbose++;
			break;
		case 'T':
#ifdef MYMALLOC
			trfp = fopen(optarg, "w");
			if (trfp == NULL) {
				perror(progname);
				fprintf(stderr, "%s: could not open file `%s' for writing",
					progname, optarg);
				exit(1);
			}
			mal_setstatsfile(trfp);
			mal_trace(1);
			trfname = optarg;
#else
			fprintf(stderr, "%s: -T option needs CSRI malloc",
				progname);
#endif
			break;
		case '?':
			usage();
			break;
		}
	}
	/* Any filenames etc. after all the options */
	if (optind < argc) {
		usage();
	}

#ifndef MYMALLOC
	before = sbrk(0);
#endif

	while (fgets(buf, sizeof buf, stdin) != NULL) {
		doline(buf);
	}

#ifndef MYMALLOC
	after = sbrk(0);
#else
	/* assumes contiguous, ick! should provide a mal_stats function */
	before = _malloc_loword;
	after = _malloc_hiword;
#endif
	grew = after - before;
	(void) fprintf(stdout, "Sbrked %ld,  MaxAlloced %ld, Wastage %.2f\n",
		       grew, maxalloced,
		       grew == 0 ? 0.0 :
		       (1.0 - ((double) maxalloced) / grew));
#ifdef MYMALLOC
	if (verbose)
		(void) mal_statsdump(stderr);
#endif
	if (trfp) {
		if (fflush(trfp) != 0 || fclose(trfp) != 0) {
		    perror(progname);
		    fprintf(stderr, "%s: error closing tracefile `%s'",
			    progname, trfname);
		}
	}
	return 0;
	
}

void
doline(buf)
char *buf;
{
	int n, blk;

	switch (buf[0]) {
	case '+':
		if (buf[1] == '+') {
			if (sscanf(buf, "%*[+] %d %*d 0x%*x %*d 0x%*x %d", &n, &blk) == 2 ||
			    sscanf(buf, "%*[+] %d %*d 0x%*x %d", &n, &blk) == 2) {
				if (bufs[blk] == NULL) {
					fprintf(stderr, "block %d not yet allocated", blk);
				}
				bufs[blk] = realloc(bufs[blk], n);
				alloced += (n - sizes[blk]);
				sizes[blk] = n;
				if (alloced > maxalloced)
					maxalloced = alloced;
			}
		} else if (sscanf(buf, "%*[+] %d %*d 0x%*x %d", &n, &blk) == 2) {
			if (bufs[blk] != NULL) {
				fprintf(stderr, "block %d not yet freed\n", blk);
			}
			bufs[blk] = malloc(n);
			sizes[blk] = n;
			alloced += n;
			if (alloced > maxalloced)
				maxalloced = alloced;
		}
		break;
	case '-':
		if (sscanf(buf, "- %*d 0x%*x %d", &blk) == 1) {
			if (bufs[blk] == NULL) {
				fprintf(stderr, "block %d not yet allocated", blk);
			} else {
				free(bufs[blk]);
				alloced -= sizes[blk];
				bufs[blk] = NULL;
				sizes[blk] = 0;
			}
		}
		break;
	case 'a':
	case 'h':	/* heapstart or heapend */
	case 'w':
	case 'n':
	case '\n':
		break;
	}
	if (verbose)
	    printf("%lu %s", alloced, buf);
}
