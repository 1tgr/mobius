/* $Id: hello.c,v 1.1 2002/12/21 09:49:08 pavlovskii Exp $ */

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
