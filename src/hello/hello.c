/* $Id: hello.c,v 1.2 2002/02/27 18:33:38 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int i;

	wprintf(L"argc = %d\n", argc);
	for (i = 0; i < argc; i++)
		wprintf(L"argv[%d] = %S\n", i, argv[i]);

	wprintf(L"Hello, world!\n");
	return EXIT_SUCCESS;
}