unsigned long _seed;
/*****************************************************************************
*****************************************************************************/

//! Returns a pseudo-random number.
/*!
 *	The number returned by rand() in fact follows a complex series and should
 *		not be used when a completely random number is needed.
 *
 *	Use srand() to seed the random number generator; each random number seed 
 *		produces a reproducible random number sequence.
 *	\return	A pseudo-random number.
 */
long rand(void) /* stdlib.h */
{
	if(_seed == 0)
		_seed = 1;
	if((((_seed << 3) ^ _seed) & 0x80000000L) != 0)
		_seed = (_seed << 1) | 1;
	else
		_seed <<= 1;
	return _seed - 1;
}
