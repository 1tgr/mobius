/* $Id: int64.c,v 1.2 2002/04/20 12:47:28 pavlovskii Exp $ */

#include <sys/types.h>

static void __do_udiv64(uint64_t numerator, uint64_t denominator, 
    uint64_t *quotient, uint64_t *remainder)
{
    uint64_t n, d, x, answer;

    if(denominator == 0)
	n = 0 / 0; /* intentionally do a division by zero */
    for(n = numerator, d = denominator, x = 1; n >= d && ((d &
	0x8000000000000000) == 0); x <<= 1, d <<= 1)
	;
    for(answer = 0; x != 0; x >>= 1, d >>= 1)
    {
	if(n >= d)
	{
	    n -= d;
	    answer |= x;
	}
    }
    *quotient = answer;
    *remainder = n;
}

static void __do_div64(int64_t numerator, int64_t denominator, 
    int64_t *quotient, int64_t *remainder)
{
    int64_t n, d, x, answer;

    if(denominator == 0)
	n = 0 / 0; /* intentionally do a division by zero */
    for(n = numerator, d = denominator, x = 1; n >= d && ((d &
	0x8000000000000000) == 0); x <<= 1, d <<= 1)
	;
    for(answer = 0; x != 0; x >>= 1, d >>= 1)
    {
	if(n >= d)
	{
	    n -= d;
	    answer |= x;
	}
    }
    *quotient = answer;
    *remainder = n;
}

uint64_t __udivdi3(uint64_t numerator, uint64_t denominator)
{
    uint64_t quotient, remainder;

    __do_udiv64(numerator, denominator, &quotient, &remainder);
    return quotient;
}

uint64_t __umoddi3(uint64_t numerator, uint64_t denominator)
{
    uint64_t quotient, remainder;

    __do_udiv64(numerator, denominator, &quotient, &remainder);
    return remainder;
}

int64_t __divdi3(int64_t numerator, int64_t denominator)
{
    int64_t quotient, remainder;

    __do_div64(numerator, denominator, &quotient, &remainder);
    return quotient;
}
