/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/handle.h>
#include <kernel/counters.h>

#include <errno.h>
#include <wchar.h>


#define COUNTERS_BEGIN()
#define COUNTER(var, desc)    extern int var;
#define COUNTERS_END()

#include <kernel/counters_def.h>

#undef COUNTERS_BEGIN
#undef COUNTER
#undef COUNTERS_END
#define COUNTERS_BEGIN() \
    static const counter_t counters[] = {
#define COUNTER(var, desc) \
    { __S(L, #var), desc, &var },
#define COUNTERS_END() \
    };

#include <kernel/counters_def.h>

static handle_class_t counter_class = HANDLE_CLASS_FREE('cntr');

counter_handle_t *CounterOpen(const wchar_t *name)
{
    unsigned i;
    counter_handle_t *counter;

    for (i = 0; i < _countof(counters); i++)
        if (_wcsicmp(name, counters[i].name) == 0)
            break;

    if (i >= _countof(counters))
    {
        errno = ENOTFOUND;
        return NULL;
    }

    counter = malloc(sizeof(*counter));
    if (counter == NULL)
        return NULL;

    HndInit(&counter->hdr, &counter_class);
    counter->counter = counters + i;
    return counter;
}


int CounterSample(counter_handle_t *counter)
{
    return *counter->counter->var;
}
