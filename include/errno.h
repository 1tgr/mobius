#ifndef __ERRNO_H
#define __ERRNO_H

#ifdef __cplusplus
extern "C"
{
#endif

int *__errno();

#define errno	(*__errno())

#define EDOM		1
#define EILSEQ		2
#define ERANGE		3
#define ENOTIMPL	4
#define EBUFFER		5
#define ENOTFOUND	6
#define EINVALID	7
#define EMFILE		8

#ifdef __cplusplus
}
#endif

#endif