/* $Id: fullpath.c,v 1.3 2002/01/15 00:13:06 pavlovskii Exp $ */

#include <os/rtl.h>
#include <wchar.h>

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

/*!	\brief Construct a path from the root to the specified file
 *
 *	Handles ., .. and current directory/drive references correctly
 *
 *  \param	src     source path for input
 *  \param	dst     destination buffer
 *  \return	true if successful
 */
bool FsFullPath(const wchar_t* src, wchar_t* dst)
{
	register wchar_t *p, *p1;
	wchar_t d;

	dst[0] = '\0';
	if (!fPathChr (src[0]))
	{
		wcscpy(dst, ProcGetCwd());
		
		/* dst = cur dir on drive d, so now bump across d: */
		if (*src == '.' && 
			(src[1] == '\0' || (fPathChr(src[1]) && src[2] == '\0')))
			/* src == "." or ".\", i.e cur dir, we are done */
			return true;
	}

	/*
	 * For absolute paths (/xx/../zz), start the path off with what we've 
	 *	already got
	 */
	if (fPathChr(*src))
		wcscpy(dst, src);
	else
	{
		/*
		 * For relative paths (xx/../zz), add a / if needed and add what we've
		 *	already got
		 */

		d = dst[wcslen(dst) - 1];
		if (dst[0] && !fPathChr(d))
			wcscat(dst, PATHSEPSTR);
		wcscat(dst, src);
	}

	src = p1 = dst;
	while (*src)
	{
		/* Save the start of this component */
		p = p1;

		/* Find the next path separator in the string */
		p1 = wcsbscan(p, L"\\/");
		d = *p1;

		/* Terminate this component here */
		*p1++ = 0;

		/* Special handling for . and .. */

		if (wcscmp(p, L".") == 0)
		{
			/* For ., backtrack to the previous separator and continue there */
			do
			{
				if (--dst < src)
					return false;
			} while (!fPathChr (*dst));
		}
		else if (wcscmp(p, L"..") == 0)
		{
			/* For .., backtrack to the last-but-one separator */
			do
			{
				if (--dst < src)
					return false;
			} while (!fPathChr (*dst));

			do
			{
				if (--dst < src)
					return false;
			} while (!fPathChr (*dst));
		}
		else
		{
			/* Otherwise, copy this component across */
			wcscpy(dst, p);
			dst += wcslen(dst);
		}

		if (fPathChr (d))
			d = PATHSEP;
		if (!(*dst++ = d))
			break;
	}

	return true;
}
