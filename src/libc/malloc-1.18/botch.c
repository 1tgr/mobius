#include "defs.h"
#include "globals.h"

int
__nothing()
{
	return 0;
}

/*
 * Simple botch routine - writes directly to stderr.  CAREFUL -- do not use
 * printf because of the vile hack we use to redefine fputs with write for
 * normal systems (i.e not super-pure ANSI)!
 */
int
__m_botch(s1, s2, p, is_end_ptr, filename, linenumber)
const char *s1, *s2;
univptr_t p;
const char *filename;
int linenumber, is_end_ptr;
{
	__asm__("int3");
#if 0
	static char linebuf[32];	/* Enough for a BIG linenumber! */
	static int notagain = 0;

	if (notagain == 0) {
		/* Try to flush the trace file and unbuffer stderr */
		(void) fflush(_malloc_statsfile);
		(void) setvbuf(stderr, (char *) 0, _IONBF, 0);
		(void) sprintf(linebuf, "%d: ", linenumber);
		(void) fputs("memory corruption found, file ",
			     stderr);
		(void) fputs(filename, stderr);
		(void) fputs(", line ", stderr);
		(void) fputs(linebuf, stderr);
		(void) fputs(s1, stderr);
		if (s2 && *s2) {
			(void) fputs(" ", stderr);
			(void) fputs(s2, stderr);
		}
		(void) fputs("\n", stderr);
		(void) __m_prblock(p, is_end_ptr, stderr);
		/*
		 * In case stderr is buffered and was written to before we
		 * tried to unbuffer it
		 */
		(void) fflush(stderr);
		notagain++;	/* just in case abort() tries to cleanup */
		abort();
	}
#endif
	return 0;	/* SHOULDNTHAPPEN */
}
