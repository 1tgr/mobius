#include <time.h>

struct tm _tm;

struct tm *localtime(const time_t *t)
{
    return &_tm;
}
