/* $Id: dowprintf.c,v 1.1 2002/12/21 09:50:07 pavlovskii Exp $ */

#ifdef TEST
typedef enum { false, true } bool;
#define _countof(a)	(sizeof(a) / sizeof((a)[0]))
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed short int16_t;
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <printf.h> /* fnptr. Also include stdarg.h for va_list, va_arg() */
#include <wchar.h>

/*****************************************************************************
	name:	dowPrintf
	action: minimal subfunction for w?printf, calls function
		Fn with arg Ptr for each character to be output
	returns:total number of characters output

	%[flag][width][.prec][mod][conv]
	flag:	-	left justify, pad right w/ blanks			DONE
			0	pad left w/ 0 for numerics					xxx
			+	always print sign, + or -					xxx
			' ' (blank) 									xxx
			#	(?)											xxx
	width:		(field width)								DONE
	prec:		(precision) 								xxx
	conv:	d,i decimal int 								DONE
			o	octal										DONE
			x,X hex 										DONE
			f,e,g,E,G float 								xxx
			c	char										DONE
			s	string										DONE
			p	ptr 										DONE
	mod:	N	near ptr									xxx
			u	decimal unsigned							DONE
			F	far ptr 									xxx
			h	short int									DONE
			l	long int									DONE
			L	long double 								xxx
*****************************************************************************/
/* flags used in processing format string */
#define     VPR_LJ  0x01    /* left justify */
#define     VPR_CA  0x02    /* use A-F instead of a-f for hex */
#define     VPR_SG  0x04    /* signed numeric conversion (%d vs. %u) */
#define     VPR_32  0x08    /* long (32-bit) numeric conversion */
#define     VPR_16  0x10    /* short (16-bit) numeric conversion */
#define     VPR_WS  0x20    /* VPR_SG set and Num was < 0 */
#define     VPR_NC  0x40    /* %c or %s is char, not wchar_t */
#define     VPR_PAD 0x80
#define		VPR_PREC 0x100

/* largest number handled is 2^32-1, lowest radix handled is 8.
2^32-1 in base 8 has 11 digits (add one for trailing NUL) */
#define     VPR_BUFLEN  12

/* Some idiot passed NULL to %s... */
#define		VPR_NULL	L"(null)"

/*!	\brief Implements the core functionality of the ...printf() functions.
 *
 *	This function is used by wprintf(), swprintf(), etc. to process a string
 *		and output it to a file or string. The function provided will be called
 *		to handle the string formed.
 *	\param	fn	The function to be called periodically for blocks of 
 *		characters.
 *	\param	Ptr	The string to be processed. Conforms to normal printf() 
 *		specifications.
 *	\param	Args	A va_list of the arguments that control the output.
 *	\return	The total number of characters passed to fn.
 */
int dowprintf(WPRINTFUNC Fn, void* Ptr, const wchar_t* Fmt, va_list Args)
{
	unsigned short Count, GivenWd, Precis, State, Flags, ActualWd, Temp;
	wchar_t *Where, Buf[VPR_BUFLEN];
	char* Narrow;
	unsigned long Radix;
	long Num;
	wchar_t ch[2] = { 0, 0 };

	State = Flags = Count = GivenWd = Precis = 0;
	/* begin scanning format specifier list */
	for(; *Fmt; Fmt++)
	{
		switch(State)
		{
		case 0: /* AWAITING % */
			if(*Fmt != '%')         /* not %... */
			{
				ch[0] = *Fmt;
				Fn(Ptr, ch, 1);      /* ...just echo it */
				Count++;
				break;
			}
			/*
			 * found %, get next char and advance state to check if next 
			 * char is a flag
			 */
			else
			{
				State++;
				Fmt++;
			}/* FALL THROUGH */

		case 1:     /* AWAITING FLAGS (-) */
			if(*Fmt == '-')
			{
				if(Flags & VPR_LJ)  /* %-- is illegal */
					State = Flags = GivenWd=0;
				else
					Flags |= VPR_LJ;
				break;
			}
			else if (*Fmt == '0')
			{
				if(Flags & VPR_PAD)  /* %00 is illegal */
					State=Flags=GivenWd=0;
				else
					Flags |= VPR_PAD;
				break;
			}
			/* not a flag char: advance state to check if it's field width */
			else
				State++;    /* FALL THROUGH */

		case 2:     /* AWAITING FIELD WIDTH (<number>) */
			if (*Fmt == '*')
			{
				GivenWd = va_arg(Args, int);
				State++;
				break;
			}
			else if (*Fmt >= '0' && *Fmt <= '9')
			{
				GivenWd=10 * GivenWd + (*Fmt - '0');
				break;
			}
			/* not field width: advance state to check if it's a modifier */
			else
				State++;    /* FALL THROUGH */

		case 3:		/* AWAITING . */
			State++;
			if (*Fmt == '.')
			{
				Flags |= VPR_PREC;
				Precis = 0;
				break;
			}

		case 4:
			if (Flags & VPR_PREC)
			{
				if (*Fmt == '*')
				{
					Precis = va_arg(Args, int);
					State++;
					break;
				}
				else if (*Fmt >= '0' && *Fmt <= '9')
				{
					Precis = 10 * Precis + (*Fmt - '0');
					break;
				}
				/* not precision: advance state to check if it's a modifier */
				else
					State++;    /* FALL THROUGH */
			}
			else
				State++;

		case 5:     /* AWAITING MODIFIER CHARS (lh) */
			if(*Fmt == 'l')
			{
				Flags |= VPR_32;
				break;
			}

			if(*Fmt == 'h')
			{
				Flags |= VPR_16;
				break;
			}
			/* not modifier: advance state to check if it's a conversion char */
			else
				State++;    /* FALL THROUGH */

		case 6:     /* AWAITING CONVERSION CHARS (Xxpndiuocs) */
			Where = Buf + _countof(Buf) - 1;
			Narrow = (char*) Where;
			*Where = '\0';
			switch(*Fmt)
			{
			case '%':
				ch[0] = '%';
				Fn(Ptr, ch, 1);
				break;

			case 'p':
				Flags |= VPR_PAD;
				Radix = 16;
				if (GivenWd == 0)
					GivenWd = sizeof(void*) * 2;
				goto DO_NUM;
			
			case 'X':
				Flags |= VPR_CA;/* FALL THROUGH */

			case 'x':
			case 'n':
				Radix = 16;
				goto DO_NUM;

			case 'd':
			case 'i':
				Flags |= VPR_SG;/* FALL THROUGH */

			case 'u':
				Radix = 10;
				goto DO_NUM;

			case 'o':
				Radix = 8;

DO_NUM:
				/* load the value to be printed. l=long=32 bits: */
				if(Flags & VPR_32)
					Num = va_arg(Args, uint32_t);
				else if(Flags & VPR_16)
				{
					/* h=short=16 bits (signed or unsigned) */
					if(Flags & VPR_SG)
						Num = va_arg(Args, int16_t);
					else
						Num = va_arg(Args, uint16_t);
				}
				else
				{
					/* no h nor l: sizeof(int) bits (signed or unsigned) */
					if(Flags & VPR_SG)
						Num = va_arg(Args, int);
					else
						Num = va_arg(Args, unsigned int);
				}

				/* take care of sign */
				if(Flags & VPR_SG)
				{
					if((long)Num < 0)
					{
						Flags |= VPR_WS;
						Num=-Num;
					}
				}

				/* convert binary to octal/decimal/hex ASCII */
				Temp = 0;
				do
				{
					Temp = (unsigned short) (Num % Radix);
					Where--;
					if (Temp < 10)
						*Where = Temp + '0';
					else if (Flags & VPR_CA)
						*Where = Temp - 10 + 'A';
					else
						*Where = Temp - 10 + 'a';
					Num /= Radix;
				} while(Num != 0);

				/* sign, again */
				if(Flags & VPR_WS)
					*--Where='-';
				goto EMIT;
			case 'c':
				Where--;
				*Where = (wchar_t)va_arg(Args, wchar_t);
				ActualWd=1;
				goto EMIT2;
			case 'C':
				Where--;
				*Where = (char)va_arg(Args, char);
				ActualWd=1;
				goto EMIT2;
			case 'S':
				Narrow = va_arg(Args, char*);
				if (Narrow == NULL)
				{
					Where = VPR_NULL;
					goto EMIT;
				}
				else
				{
					Flags |= VPR_NC;
					ActualWd = (unsigned short) strlen(Narrow);
				}
				goto EMIT2;
			case 's':
				Where = va_arg(Args, wchar_t*);
				if (Where == NULL)
					Where = VPR_NULL;

EMIT:           
				if (Flags & VPR_NC)
					ActualWd = (unsigned short) strlen(Narrow);
				else
					ActualWd = (unsigned short) wcslen(Where);
EMIT2:          if (Flags & VPR_PREC)
					ActualWd = min(ActualWd, Precis);
				if (Flags & VPR_PAD)
				{
					for(; GivenWd > ActualWd; GivenWd--)
					{
						ch[0] = '0';
						Fn(Ptr, ch, 1);
						Count++; 
					}
				}
				else if((Flags & VPR_LJ) == 0)
				{
					/* pad on left with spaces (for right justify) */
					for(; GivenWd > ActualWd; GivenWd--)
					{
						ch[0] = ' ';
						Fn(Ptr, ch, 1);
						Count++; 
					}
				}
				/* emit string/char/converted number from Buf */

				if (Flags & VPR_NC)
				{
					while (ActualWd > 0)
					{
						Temp = (unsigned short) 
							mbstowcs(Buf, Narrow, min(_countof(Buf) - 1, ActualWd));
						Buf[Temp] = '\0';
						Fn(Ptr, Buf, Temp);
						ActualWd -= Temp;
						Narrow += Temp;
						Count += Temp;
					}
				}
				else
				{
					Fn(Ptr, Where, ActualWd);
					Count += ActualWd;
				}
				/* pad on right with spaces (for left justify) */
				if(GivenWd < ActualWd)
					GivenWd=0;
				else
					GivenWd -= ActualWd;
				for(; GivenWd; GivenWd--)
				{
					ch[0] = ' ';
					Fn(Ptr, ch, 1);
					Count++;
				}		/* FALL THROUGH */
			default:	/* FALL THROUGH */
				break;
			}
		default:
			State=Flags=GivenWd=0;
			break;
		}
	}
	return(Count);
}

#ifdef TEST
int _cputs(const char* str);

static bool kprintfhelp(void* pContext, const wchar_t* str, size_t len)
{
	char *buf;
	buf = _alloca(len + 1);
	buf[len] = '\0';
	wcstombs(buf, str, len);
	_cputs(buf);
	return true;
}

int vwprintf(const wchar_t* fmt, va_list ptr)
{
	int ret;
	ret = dowprintf(kprintfhelp, NULL, fmt, ptr);
	return ret;
}

int wprintf(const wchar_t* fmt, ...)
{
	va_list ptr;
	int ret;

	va_start(ptr, fmt);
	ret = vwprintf(fmt, ptr);
	va_end(ptr);
	return ret;
}

int main(void)
{
	wprintf(L"Hello, world!\n");
	wprintf(L"This is a number: %d\n", 42);
	wprintf(L"This is a string: %.5s\n", L"wibble");
	wprintf(L"This is a narrow string: %.5S\n", "elbbiw");
	return 0;
}
#endif