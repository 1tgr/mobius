#include <string.h>

void *memcpy(void *dst_ptr, const void *src_ptr, size_t count) /* string.h */
{
	void *ret_val = dst_ptr;
	const unsigned char *src = (const unsigned char *)src_ptr;
	unsigned char *dst = (unsigned char *)dst_ptr;

/* copy up */
	for(; count != 0; count--)
		*dst++ = *src++;
	return ret_val;
}
