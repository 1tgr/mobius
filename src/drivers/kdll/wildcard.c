#include <sys/types.h>

bool wildcard(const wchar_t* str, const wchar_t* spec)
{
	while (*spec)
	{
		if (*spec == '*')
		{
			spec++;
			if (!*spec)
				return true;

			while (*str && *str != *spec)
				str++;
		}
		else if (*str != *spec)
			return false;

		str++;
		spec++;
	}

	return true;
}