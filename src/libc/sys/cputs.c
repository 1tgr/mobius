/* $Id: cputs.c,v 1.3 2002/01/10 20:50:17 pavlovskii Exp $ */

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

int _cputs(const char *str)
{
	wchar_t *wc;
	size_t len;
	int ret;

	len = strlen(str);
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