extern unsigned long _seed; /* in rand.c */
/*****************************************************************************
*****************************************************************************/

//! Seeds the random number generator.
/*!
 *	Each random number seed produces a reproducible random number sequence. 
 *		The value returned from sysUpTime() is suitable for seeding at 
 *		the start of a program since it is unlikely to be the same on each 
 *		invokation.
 *	\param	new_seed	The new value with which the random number generator is
 *		seeded.
 */
void srand(unsigned long new_seed) /* stdlib.h */
{
	_seed = new_seed;
}
