/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"
#include "trace.h"
#include <wchar.h>

RCSID("$Id: _strdup.c,v 1.2 2002/01/15 00:13:06 pavlovskii Exp $")

char *
__strdup(s, fname, linenum)
const char *s;
const char *fname;
int linenum;
{
	char *cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = strdup(s);
	RECORD_FILE_AND_LINE((univptr_t) cp, fname, linenum);
	return(cp);
}

wchar_t *
__wcsdup(s, fname, linenum)
const wchar_t *s;
const char *fname;
int linenum;
{
	wchar_t *cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = _wcsdup(s);
	RECORD_FILE_AND_LINE((univptr_t) cp, fname, linenum);
	return(cp);
}
