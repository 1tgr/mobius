/* $Id: ctype.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_ctype_h_
#define __dj_include_ctype_h_

#ifdef __cplusplus
extern "C" {
#endif

int	isalnum(int c);
int	isalpha(int c);
int	iscntrl(int c);
int	isdigit(int c);
int	isgraph(int c);
int	islower(int c);
int	isprint(int c);
int	ispunct(int c);
int	isspace(int c);
int	isupper(int c);
int	isxdigit(int c);
int	tolower(int c);
int	toupper(int c);

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_ctype_h_ */
