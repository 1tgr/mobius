/* $Id: crt0.c,v 1.3 2002/02/27 18:33:38 pavlovskii Exp $ */

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
	size_t size;
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

void mainCRTStartup(void)
{
	CHAR **argv;
	int argc;
	argc = __crt0_parseargs(&argv);
	exit(MAIN(argc, argv));
}
