/* $Id: profile.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */

#ifndef __KERNEL_PROFILE_H
#define __KERNEL_PROFILE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

bool ProLoadProfile(const wchar_t *file, const wchar_t *root);
bool ProGetBoolean(const wchar_t *key, const wchar_t *value, bool _default);
const wchar_t *ProGetString(const wchar_t *key, const wchar_t *value, const wchar_t *_default);
bool ProEnumValues(const wchar_t *key, void *param, 
		   bool (*func)(void *, const wchar_t *, const wchar_t *));

#ifdef __cplusplus
}
#endif

#endif
