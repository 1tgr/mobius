/* $Id: wcsncpy.c,v 1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <wchar.h>

/*****************************************************************************
	name:   wcsncpy
	action: copies at most Count characters from Src string to Dst
	returns:Dst
*****************************************************************************/
wchar_t* wcsncpy(wchar_t* Dst, const wchar_t* Src, unsigned int Count)
{
	wchar_t* RetVal=Dst;

	for(; (*Src != '\0') && (Count != 0); Count--)
		*Dst++=*Src++;
	*Dst='\0';
	return(RetVal);
}
