#include <string.h>
#include <stdio.h>
#include <errno.h>

void _pwerror(const wchar_t* string)
{
	wprintf(L"%s: %s\n", string, _wcserror(errno));
}