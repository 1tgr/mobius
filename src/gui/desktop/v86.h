#ifndef V86_H__
#define V86_H__

#ifdef __cplusplus
extern "C"
{
#endif

FARPTR i386LinearToFp(void *ptr);
void ShV86Handler(void);

#ifdef __cplusplus
}
#endif

#endif
