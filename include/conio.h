#ifndef __CONIO_H
#define __CONIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <wchar.h>

wint_t _wgetch();
wint_t _wgetche();
int _kbhit();
void delay(int ms);

#define kbhit _kbhit

#ifdef __cplusplus
}
#endif

#endif