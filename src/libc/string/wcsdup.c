#include <wchar.h>
#include <malloc.h>

wchar_t *wcsdup(const wchar_t* str)
{
	wchar_t* ret = malloc((wcslen(str) + 1) * sizeof(wchar_t));

	if (!ret)
		return NULL;

	wcscpy(ret, str);
	return ret;
}