#include <string.h>

#pragma function(strcmp)
int strcmp(const char *str1, const char *str2) /* string.h */
{
	while((*str2 != '\0') && (*str1 == *str2))
	{
		str1++;
		str2++;
	}
	return *str1 -  *str2;
}
