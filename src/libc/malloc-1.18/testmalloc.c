#if defined(STDHEADERS)
# include <stddef.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
#else
extern char *memset();
/* ignore some complaints about declarations.  get ANSI headers */
#endif

/* saves typing ! */
#define uint unsigned int

#include <stdio.h>
#include "malloc.h"

/* 
 *  Things to test.  1. first malloc.  2. couple of ordinary mallocs 3.
 *  ordinary frees 4. want check a free with prev. merge, next merge,
 *  both, and no merge, and one with empty free list.  5. malloc that
 *  requires sbrk.  6. malloc that requires an sbrk, after test
 *  program does non-contiguous sbrk to check if non-contigous arenas
 *  work.  7. valloc (this should test memalign as well) We should work
 *  out tests to check boundary conditions, when blocks at the start
 *  of the arena are allocated/freed, last free block is allocated...
 */


char *progname;
/* For getopt() */
extern int getopt();
extern int optind;
extern char *optarg;

FILE *dumpfp = stdout;

int
main(argc, argv)
int argc;
char **argv;
{
	char *cp1;
	char *cp2;
	char *cp3;
	char *cp4;
	char *cp5;
	char *cp6;
	extern char *sbrk();
	int errs = 0, c;

	progname = argv[0] ? argv[0] : "(no-argv[0])";
	while((c = getopt(argc, argv, "m:dt")) != EOF) {
		/* optarg has the current argument if the option was followed by ':'*/
		switch (c) {
		case 'm':
			mal_mmap(optarg);
			break;
		case 'd':
			mal_debug(3);
			break;
		case 't':
			mal_setstatsfile(stdout);
			mal_trace(1);
			break;
		case '?':
			errs++;
			break;
		}
	}
	if (optind < argc || errs > 0) {
		fprintf(stderr, "Usage: %s [-m Mmapfile]\n", progname);
		exit(1);
	}
	write(1, "Test starting\n", 14);
	mal_heapdump(dumpfp);
	cp1 = (char *)malloc((uint) 100);
	(void) memset(cp1, 'A', 100);
	mal_heapdump(dumpfp);
	cp2 = (char *)calloc((uint) 15, (uint) 10);
	(void) memset(cp2, 'B', 150);
	mal_heapdump(dumpfp);
	cp3 = (char *)malloc((uint) 191);
	(void) memset(cp3, 'C', 191);
	mal_heapdump(dumpfp);
	cp4 = (char *)malloc((uint) 2);
	(void) memset(cp4, 'D', 2);
	mal_heapdump(dumpfp);
	cp5 = (char *)malloc((uint) 21);
	(void) memset(cp5, 'E', 21);
	mal_heapdump(dumpfp);
	cp6 = (char *)malloc((uint) 3540);
	(void) memset(cp6, 'P', 3540);
	mal_heapdump(dumpfp);
	/* On a machine where sizeof(Word) == 4, rover should be NULL here */
	free(cp6);
	mal_heapdump(dumpfp);
	free(cp3);
	mal_heapdump(dumpfp);
	free(cp4);
	mal_heapdump(dumpfp);
	free(cp2);
	mal_heapdump(dumpfp);
	free(cp5);
	mal_heapdump(dumpfp);
	free(cp1);
	mal_heapdump(dumpfp);
	cp1 = (char *)malloc((uint) 100);
	(void) memset(cp1, 'Q', 100);
	mal_heapdump(dumpfp);
	cp2 = (char *)malloc((uint) 155);
	(void) memset(cp2, 'F', 155);
	mal_heapdump(dumpfp);
	cp3 = (char *)malloc((uint) 8192);
	(void) memset(cp3, 'G', 8192);
	mal_heapdump(dumpfp);
	cp4 = (char *)malloc((uint) 100);
	(void) memset(cp4, 'H', 100);
	mal_heapdump(dumpfp);
	cp5 = (char *)malloc((uint) 29);
	(void) memset(cp5, 'I', 29);
	mal_heapdump(dumpfp);
	free(cp3);
	mal_heapdump(dumpfp);
	free(cp4);
	mal_heapdump(dumpfp);
	free(cp2);
	mal_heapdump(dumpfp);
	free(cp5);
	mal_heapdump(dumpfp);
	free(cp1);
	mal_heapdump(dumpfp);
	cp1 = sbrk(100);
	cp2 = (char *)malloc((uint) 1005);
	(void) memset(cp2, 'J', 1005);
	mal_heapdump(dumpfp);
	cp3 = (char *)calloc((uint) 1024, (uint) 8);
	(void) memset(cp3, 'K', 8192);
	mal_heapdump(dumpfp);
	cp4 = (char *)malloc((uint) 16000);
	(void) memset(cp4, 'L', 16000);
	mal_heapdump(dumpfp);
	cp5 = (char *)malloc((uint) 29);
	(void) memset(cp5, 'M', 29);
	mal_heapdump(dumpfp);
	/* !! Should really test memalign with various cases */
	cp1 = (char *)valloc((uint) 65536);
	(void) memset(cp1, 'N', 65536);
	mal_heapdump(dumpfp);
	cp1 = (char *)realloc(cp1, (uint) 8000);
	(void) memset(cp1, 'O', 8000);
	mal_heapdump(dumpfp);
	cp1 = (char *)realloc(cp1, (uint) 7998);
	(void) memset(cp1, 'T', 7998);
	mal_heapdump(dumpfp);
	free(cp2);
	mal_heapdump(dumpfp);
	free(cp3);
	mal_heapdump(dumpfp);
	free(cp5);
	mal_heapdump(dumpfp);
	free(cp4);
	mal_heapdump(dumpfp);
	cp1 = (char *)realloc(cp1, (uint) 16000);
	(void) memset(cp1, 'R', 16000);
	mal_heapdump(dumpfp);
	cp1 = (char *)realloc(cp1, (uint) 32000);
	(void) memset(cp1, 'S', 32000);
	mal_heapdump(dumpfp);
	cp1 = (char *)realloc(cp1, (uint) 1);
	(void) memset(cp1, 'U', 1);
	cp2 = (char *)malloc(60000);
	(void) memset(cp2, 'V', 60000);
	cp3 = (char *)malloc(18000);
	(void) memset(cp3, 'W', 18000);
	cp4 = (char *)malloc(18000);
	(void) memset(cp4, 'W', 18000);
	mal_heapdump(dumpfp);
	free(cp1);
	mal_heapdump(dumpfp);
	mal_statsdump(dumpfp);
	(void) write(1, "Test done\n", 10);
	return 0;
}

#ifdef atarist
getpagesize()
{
    return 8 * 1024;
}
#endif
