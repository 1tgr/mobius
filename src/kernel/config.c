#include <kernel/config.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

const char* cfgGetToken(const char* str, char* out, size_t max, const char* ctrl)
{
	unsigned char map[32];
	int count;
	const char* start;

	for (count = 0; count < 32; count++)
		map[count] = 0;

	do {
		map[*ctrl >> 3] |= (1 << (*ctrl & 7));
	} while (*ctrl++);

	while ( (map[*str >> 3] & (1 << (*str & 7))) && *str )
		str++;
	
	if (*str == '\n' ||
		*str == 0)
		return NULL;
	
	start = str;
	for ( ; *str ; str++ )
	{
		if ( map[*str >> 3] & (1 << (*str & 7)) )
		{
			*out++ = '\0';
			break;
		}
		
		*out = *str;
		out++;
	}

	return str;
}

hashtable_t* cfgParseStrLine(const char** line)
{
	hashtable_t* hash;
	const char* delims = "\r\t= ";
	hashelem_t elem;
	size_t size;
	char token[256];
	const char* str;
	wchar_t* wide;

	hash = hashCreate(31);

	while ((str = cfgGetToken(*line, token, countof(token), delims)))
	{
		*line = str;

		if (*token == '#')
		{
			while (**line != '\n' &&
				**line)
				(*line)++;
			break;
		}

		wide = malloc(sizeof(wchar_t) * (strlen(token) + 1));
		size = mbstowcs(wide, token, strlen(token) + 1);
		wide[size] = 0;
		elem.str = wide;

		if (**line == '=')
		{
			*line = cfgGetToken(*line, token, countof(token), delims);

			elem.data = malloc(sizeof(wchar_t) * (strlen(token) + 1));
			size = mbstowcs((wchar_t*) elem.data, token, strlen(token) + 1);
			((wchar_t*) elem.data)[size] = 0;
		}
		else
			elem.data = NULL;

		hashInsert(hash, &elem);
	}

	(*line)++;
	return hash;
}

void cfgDeleteItem(hashelem_t *e)
{
	free((void*) e->str);
	free(e->data);
}

void cfgDeleteTable(hashtable_t* hash)
{
	hashList(hash, cfgDeleteItem);
	free(hash->table);
	free(hash);
}