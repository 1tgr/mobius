#include <stdio.h>
#include <libc/file.h>

char __init_file_handle_modes[20];
char *__file_handle_modes = __init_file_handle_modes;
