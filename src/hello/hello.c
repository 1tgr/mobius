/* $Id: hello.c,v 1.3 2002/03/13 14:25:52 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int i;
	uint64_t a, b, c;

	a = 12345678ULL;
	b = 910111213141516ULL;
	c = b / a;

	wprintf(L"argc = %d\n", argc);
	for (i = 0; i < argc; i++)
		wprintf(L"argv[%d] = %S\n", i, argv[i]);

	wprintf(L"Hello, world!\n");
	return EXIT_SUCCESS;
}