/* $Id: cputs.c,v 1.4 2002/02/26 15:46:34 pavlovskii Exp $ */

#include <string.h>
#include <stdlib.h>

int _cputws(const wchar_t *str, size_t count);

/*static __inline__ void *_alloca(size_t size)
{
	void *ret;
	__asm__("sub %1, %%esp\n"
		"mov %%esp, %0"
		: "=a" (ret)
		: "g" (size));
	return ret;
}*/

int _cputs(const char *str, size_t len)
{
	wchar_t *wc;
	int ret;

	/*wc = _alloca(sizeof(wchar_t) * len);*/
	wc = malloc(sizeof(wchar_t) * len);
	len = mbstowcs(wc, str, len);
	if (len == -1)
		ret = 0;
	else
		ret = _cputws(wc, len);
	free(wc);
	return ret;
}