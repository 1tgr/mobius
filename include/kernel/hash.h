//////////////////////////////////////////////////////////////////////////////
// Project: Redpants Kernel                                                 //
// Date:    5-12-2000                                                       //
// Module:  shell/hashtable.h                                               //
// Purpose: This is the kernel, the main portion of the operating system,   //
//          that handles all the duties of a modern operating system.       //
//                                                                          //
//                    Created 2000, by Ben L. Titzer                        //
//////////////////////////////////////////////////////////////////////////////

#ifndef __KERNEL_HASHTABLE_H
#define __KERNEL_HASHTABLE_H

#include <sys/types.h>

typedef struct hashelem_t hashelem_t;
struct hashelem_t
{
	const wchar_t* str;    // string key
	void *data;  // the data
};

typedef struct hashtable_t hashtable_t;
struct hashtable_t
{
	dword size, used;
	hashelem_t *table;
};

hashtable_t*	hashCreate(dword size);
dword			hashInsert(hashtable_t*, hashelem_t *);
void			hashResize(hashtable_t*);
hashelem_t*		hashFind(hashtable_t*, const wchar_t*);
void			hashList(hashtable_t*, void (*callback)(hashelem_t *e));

dword			hash(const wchar_t*); // hash function

#endif
