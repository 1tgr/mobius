/* $Id: hello.c,v 1.4 2002/03/27 22:13:01 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int i;

    if (argc > 1)
    {
        printf("argc = %d\n", argc);
        for (i = 0; i < argc; i++)
            printf("argv[%d] = %s\n", i, argv[i]);
    }
    else
        printf("Hello, world!\n");
        
    return EXIT_SUCCESS;
}
