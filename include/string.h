#ifndef __STRING_H
#define __STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/* Wide-character functions */
size_t wcslen(const wchar_t* str);
wchar_t *wcscpy(wchar_t *strDestination, const wchar_t *strSource);
wchar_t *wcsncpy(wchar_t *strDestination, const wchar_t *strSource, size_t count);
int wcscmp(const wchar_t* str1, const wchar_t* str2);
int wcsicmp(const wchar_t* str1, const wchar_t* str2);
wchar_t *wcschr(const wchar_t* str, int c);
wchar_t *wcsrchr(const wchar_t* str, int c);
wchar_t *wcscat(wchar_t* dest, const wchar_t* src);
wchar_t *wcsdup(const wchar_t* str);
wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2);

/* ANSI-character functions */
size_t strlen(const char* str);
int stricmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t count);
char *strcpy(char *strDestination, const char *strSource);
int strcmp(const char *string1, const char *string2);
char *strtok(char *string, const char *control);
char *strpbrk(const char *s1, const char *s2); 

/* ANSI/wide conversion functions */
size_t mbstowcs(wchar_t *wcstr, const char *mbstr, size_t count);
size_t wcstombs(char *mbstr, const wchar_t *wcstr, size_t count);

/* Buffer manipulation functions */
void *memset(void *dest, int c, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void* dest, const void *src, size_t count);
int memcmp(const void *buf1, const void *buf2, size_t count);

wchar_t *_wcserror(int errcode);

#ifdef _MSC_VER
#pragma intrinsic(strcpy, strcmp, strlen, memcpy, memset, memcmp)
#endif

#ifdef __cplusplus
}
#endif

#endif