/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"

RCSID("$Id: strsave.c,v 1.1 2001/11/05 18:31:45 pavlovskii Exp $")

/* 
 *  makes a copy of a null terminated string in malloc'ed storage. Dies
 *  if enough memory isn't available or there is a malloc error
 */
char *
strsave(s)
const char *s;
{
	if (s)
		return(strcpy((char *) emalloc((size_t) (strlen(s)+1)),s));
	else
		return((char *) NULL);
}
