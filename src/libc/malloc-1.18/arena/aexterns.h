/* 
 *  Malloc definitions from General Utilities <stdlib.h>. Note that we
 *  disagree with Berkeley Unix on the return type of free/cfree.
 */
extern univptr_t amalloc proto((Arena *, size_t));
extern univptr_t acalloc proto((Arena *, size_t, size_t));
extern univptr_t arealloc proto((Arena *, univptr_t, size_t));
extern void afree proto((Arena *, univptr_t));

/* Internal definitions */
extern int __nothing proto((void));

/* General Utilities <stdlib.h> */

extern void	abort proto((void));
extern void	exit proto((int));

/*
 *  Input/Output <stdio.h> Note we disagree with Berkeley Unix on
 *  sprintf().
 */

#if defined(sun) || defined(__sun__)
extern int sprintf proto((char *, const char *, ...));
#endif

extern int fputs proto((const char *, FILE *));
extern int fflush proto((FILE *));
extern int setvbuf proto((FILE *, char *, int, memsize_t));

/* Character Handling: <string.h> */

extern univptr_t memset proto((univptr_t, int, memsize_t));
extern univptr_t memcpy proto((univptr_t, const univptr_t, memsize_t));
extern char *strcpy proto((char *, const char *));
extern memsize_t strlen proto((const char *));

/* UNIX */
extern int getpagesize proto((void));
extern caddr_t sbrk proto((int));
extern int write proto((int, const char *, int));
