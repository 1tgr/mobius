/* $Id: hello.c,v 1.5 2002/08/17 22:52:12 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv)
{
	int i;

    if (argc > 1)
    {
        printf("argc = %d\n", argc);
        for (i = 0; i < argc; i++)
            printf("argv[%d] = %S\n", i, argv[i]);
    }
    else
        printf("Hello, world!\n");

    return EXIT_SUCCESS;
}
