#define isupper(ch)	((ch) >= 'A' && (ch) <= 'Z')
#define tolower(ch)	(isupper(ch) ? (ch) - 'A' + 'a' : (ch))

/*****************************************************************************
*****************************************************************************/
int stricmp(const char *str1, const char *str2) /* string.h */
{
	while((*str2 != '\0') && (tolower(*str1) == tolower(*str2)))
	{
		str1++;
		str2++;
	}
	return *str1 -  *str2;
}
