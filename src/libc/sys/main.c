/* $Id: main.c,v 1.3 2002/03/04 18:56:32 pavlovskii Exp $ */

#include <stdio.h>

void __setup_file_rec_list(void);

void __main(void)
{
    __get_stdin();
    __get_stdout();
    __get_stderr();
    __setup_file_rec_list();
}
