/* $Id: crt0.c,v 1.1 2002/08/05 16:25:54 pavlovskii Exp $ */

/*
 * Note: this source file compiles to both /lib/crt0.o and /lib/wcrt0.o. 
 *  wcrt0.o gets WIDE defined.
 */

#include <os/rtl.h>
#include <os/defs.h>
#include <stdlib.h>
#include <ctype.h>
#include <wchar.h>

#ifdef WIDE

#define MAIN		wmain
#define CHAR		wchar_t
#define _istspace	iswspace
#define _tcslen		wcslen

#else

#define MAIN	main
#define CHAR	char
#define _istspace	isspace
#define _tcslen		strlen

#endif

void exit(int code);
int MAIN(int argc, CHAR **argv);

CHAR *__crt0_argv[2];

int __crt0_parseargs(CHAR ***argv)
{
	wchar_t *wargs;
	CHAR *args, *ch;
#ifndef WIDE
	size_t size;
#endif
	int argc, i;
	CHAR **theargv;

	wargs = ProcGetProcessInfo()->cmdline;
#ifdef WIDE
	args = wargs;
#else
	size = wcslen(wargs);
	args = malloc(size + 1);
	size = wcstombs(args, wargs, size);
	if (size == -1)
		args[0] = '\0';
	else
		args[size] = '\0';
#endif

	argc = 1;
	for (ch = args; *ch; ch++)
		if (*ch == ' ')
		{
			*ch = '\0';
			argc++;
			while (*ch && _istspace(*ch))
				ch++;
		}

	if (argc <= _countof(__crt0_argv))
		theargv = __crt0_argv;
	else
		theargv = malloc(sizeof(CHAR*) * argc);

	ch = args;
	for (i = 0; i < argc; i++)
	{
		theargv[i] = ch;
		ch += _tcslen(ch) + 1;
	}

	*argv = theargv;
	return argc;
}

extern void __main(void);

void mainCRTStartup(void)
{
	CHAR **argv;
	int argc;
    argc = __crt0_parseargs(&argv);
#ifdef WIDE
    /* xxx - what is the matter with GNU developers and wide characters?! */
    __main();
#endif
	exit(MAIN(argc, argv));
}
