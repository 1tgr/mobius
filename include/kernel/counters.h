/* $Id$ */
#ifndef __KERNEL_COUNTERS_H
#define __KERNEL_COUNTERS_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct counter_t counter_t;
struct counter_t
{
	const wchar_t *name;
	const wchar_t *desc;
	int *var;
};


typedef struct counter_handle_t counter_handle_t;
struct counter_handle_t
{
	handle_hdr_t hdr;
	const counter_t *counter;
};

counter_handle_t *CounterOpen(const wchar_t *name);
int CounterSample(counter_handle_t *counter);

#undef COUNTERS_BEGIN
#undef COUNTER
#undef COUNTERS_END

#ifdef __cplusplus
}
#endif

#endif
