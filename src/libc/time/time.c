#include <time.h>

time_t time(time_t *t)
{
	if (t)
		*t = 0;
	return 0;
}
