#include <os/os.h>
#include <string.h>

#define PATHSEP			'/'
#define PATHSEPSTR		L"/"
#define fPathChr(ch)    ((ch) == PATHSEP)

wchar_t* wcsbscan(const wchar_t* str, const wchar_t* sub)
{
	wchar_t* ch;

	for (; *sub; sub++)
	{
		if ((ch = wcschr(str, *sub)) != NULL)
			return ch;
	}

	return (wchar_t*) str + wcslen(str);
}

//!  Construct a path from the root to the specified file
/*!
 *	Handles ., .. and current directory/drive references correctly
 *
 *  \param	src     source path for input
 *  \param	dst     destination buffer
 *  \return	true if successful
 */
bool fsFullPath(const wchar_t* src, wchar_t* dst)
{
	wchar_t *beg = dst;
	register wchar_t *p, *p1;
	wchar_t d;

	/*p = src;
	if (src[0] && src[1] == ':')
	{
		*p = (char) tolower (*p);
		p += 2;
	}*/

	if (!fPathChr (src[0]) || !fPathChr (src[1]))
	{
		/*d = 0;
		if (*src && src[1] == ':')
		{
			d = (char) (toupper (*src) - 'A' + 1);
			src += 2;
		}

		if (curdir (dst, d))
			// invalid drive
			return TRUE;*/

		wcscpy(dst, procCwd());
		//curdir(dst);

		/* dst = cur dir on drive d, so now bump across d: */
		//dst += 2;
		if (*src == '.' && (src[1] == '\0' ||
					(fPathChr (src[1]) && src[2] == '\0')))
			/* src == "." or ".\", i.e cur dir, we are done */
			return true;
	}
	/*else
		src = p;*/

	if (fPathChr (*src))
		wcscpy(dst, src);
	else
	{
		d = dst[wcslen(dst)-1];
		if (!fPathChr (d))
			wcscat(dst, PATHSEPSTR);
		wcscat(dst, src);
	}

	p1 = src = dst;
	while (*src)
	{
		p = p1;
		p1 = wcsbscan(p, L"\\/");
		d = *p1;
		*p1++ = 0;
		if (!wcscmp(p, L"."))
		{
			do
			{
				if (--dst < src)
					return false;
			} while (!fPathChr (*dst));
		}
		else if (!wcscmp(p, L".."))
		{
			do {
			if (--dst < src)
				return false;
			} while (!fPathChr (*dst));
			do {
			if (--dst < src)
				return false;
			} while (!fPathChr (*dst));
		}
		else
		{
			wcscpy(dst, p);
			dst += wcslen(dst);
		}

		if (fPathChr (d))
			d = PATHSEP;
		if (!(*dst++ = d))
			break;
	}
	// If '..' was passed in one level above the root dir, beg at this point
	// will contain '<drive_letter>:' which will not be treated as a valid
	// directory - To account for this, add a '\' character.
	//
	// Note: This case does not occur if '<drive_letter>:' is passed in.
	//
	if( wcslen( beg ) == 2 )
	{
		*( beg + 2 ) = PATHSEP;
		*( beg + 3 ) = '\0';
	}
	//pname (beg);
	return true;
}
