#ifndef __KERNEL_CONFIG_H
#define __KERNEL_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/hash.h>

const char* cfgGetToken(const char* str, char* out, size_t max, 
						const char* ctrl);
hashtable_t* cfgParseStrLine(const char** line);
void cfgDeleteTable(hashtable_t* hash);

#ifdef __cplusplus
}
#endif

#endif