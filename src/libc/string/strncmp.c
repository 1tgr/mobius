/*****************************************************************************
	name:   strncmp
	action: compares first Count characters of strings Str1 and Str2
	returns:0 if equal
		<0 if Str1 < Str2
		>0 if Str1 > Str2
*****************************************************************************/
int strncmp(const char *Str1, const char *Str2, unsigned int Count)
{   for(; (*Str2 != '\0') && (*Str1 == *Str2) && (Count != 1); Count--)
	{   Str1++;
		Str2++; }
	return(*Str1 -  *Str2); }
