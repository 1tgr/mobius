/* $Id: params.c,v 1.1 2002/03/04 19:27:37 pavlovskii Exp $ */

#include <stdlib.h>
#include <ctype.h>
#include <wchar.h>

#include "common.h"

unsigned ShParseParams(wchar_t *params, wchar_t ***names, wchar_t ***values, 
		       wchar_t ** unnamed)
{
    unsigned num_names;
    wchar_t *ch;
    int state;

    num_names = 0;
    state = 0;
    if (unnamed)
	*unnamed = L"";
    for (ch = params; *ch;)
    {
	switch (state)
	{
	case 0:    /* normal character */
	    if (*ch == PARAMSEP ||
		ch == params ||
		(iswspace(*ch) &&
		ch[1] != '\0' &&
		!iswspace(ch[1]) &&
		ch[1] != PARAMSEP))
	    {
		state = 1;
		num_names++;
	    }
	    break;

	case 1:    /* part of name */
	    if (iswspace(*ch) ||
		*ch == PARAMSEP)
	    {
		state = 0;
		continue;
	    }
	    break;
	}

	ch++;
    }

    *names = malloc(sizeof(wchar_t*) * (num_names + 1));
    *values = malloc(sizeof(wchar_t*) * (num_names + 1));
    (*names)[num_names] = NULL;
    (*values)[num_names] = NULL;

    num_names = 0;
    state = 0;
    for (ch = params; *ch;)
    {
	switch (state)
	{
	case 0:    /* normal character */
	    if (*ch == PARAMSEP)
	    {
		/* option with name */
		state = 1;
		*ch = '\0';
		(*names)[num_names] = ch + 1;
		(*values)[num_names] = L"";
		num_names++;
	    }
	    else if (iswspace(*ch))
	    {
		/* unnamed option */
		*ch = '\0';
		if (ch[1] != '\0' &&
		    !iswspace(ch[1]) &&
		    ch[1] != PARAMSEP)
		{
		    state = 1;
		    (*names)[num_names] = L"";
		    (*values)[num_names] = ch + 1;
		    num_names++;
		    if (unnamed && **unnamed == '\0')
			*unnamed = ch + 1;
		}
	    }
	    else if (ch == params)
	    {
		/* unnamed option at start of string */
		state = 1;
		(*names)[num_names] = L"";
		(*values)[num_names] = ch;
		num_names++;
		if (unnamed && **unnamed == '\0')
		    *unnamed = ch;
	    }
	    break;

	case 1:    /* part of name */
	    if (iswspace(*ch) ||
		*ch == PARAMSEP)
	    {
		state = 0;
		continue;
	    }
	    else if (*ch == '=')
	    {
		*ch = '\0';
		(*values)[num_names - 1] = ch + 1;
	    }
	    break;
	}

	 ch++;
    }

    return num_names;
}

bool ShHasParam(wchar_t **names, const wchar_t *look)
{
    unsigned i;
    for (i = 0; names[i]; i++)
	if (_wcsicmp(names[i], look) == 0)
	    return true;
    return false;
}

wchar_t *ShFindParam(wchar_t **names, wchar_t **values, const wchar_t *look)
{
    unsigned i;
    for (i = 0; names[i]; i++)
	if (_wcsicmp(names[i], look) == 0)
	    return values[i];
    return L"";
}
