/* $Id: hash.h,v 1.1 2002/03/13 14:38:30 pavlovskii Exp $
   +++Date last modified: 05-Jul-1997
   Modified: $LOG$
   By: $Author: pavlovskii $
*/

#ifndef HASH__H
#define HASH__H

#include <assert.h>           /* rudimentary DBC help */
#include <stddef.h>           /* For size_t     */
#include <string.h>
#include <stdlib.h>

/*
 A hash table consists of an array of these buckets.  Each bucket
 holds a copy of the key, a pointer to the item associated with the
 key, and a pointer to the next bucket that collided with this one,
 if there was one.
*/


/* const int error_val = -1;*/

typedef struct bucket {
    wchar_t *key;
    void *item;
    struct bucket *next;
    /* struct bucket *previous; */
} bucket;

/*
 This is what you actually declare an instance of to create a table.
 You then call 'construct_table' with the address of this structure,
 and a guess at the size of the table.  Note that more nodes than this
 can be inserted in the table, but performance degrades as this
 happens.  Performance should still be quite adequate until 2 or 3
 times as many nodes have been inserted as the table was created with.
*/

typedef struct hash_table_iterator{
    size_t it_position;
    bucket* it_bucket;
} hash_table_iterator;


typedef struct hash_table {
    size_t size;
    bucket **table;
    hash_table_iterator *iterator;
} hash_table;



typedef int boolean;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif


boolean found; /* was last search successfull? Is that the
                         right place for that declaration? */ 
boolean off; /* am i inside the hash-table? */

boolean insertation_failed;

/*
 This is used to construct the table.  If it doesn't succeed, it sets
 the table's size to 0, and the pointer to the table to NULL.
*/

hash_table* new_hash_table(size_t size);

/*
 Inserts a pointer to 'item' in the table, with a copy of 'key' as its
 key.  Note that this makes a copy of the key, but NOT of the
 associated item.
*/

void insert(struct hash_table *table, void * item, const wchar_t *key);

/*
 Look up a key and set found to 1 if found and adjust values in iterator
*/


void search(struct hash_table *table, const wchar_t *key);

/*
 Deletes an entry from the table.  Returns a pointer to the item that
 was associated with the key so the calling code can dispose of it
 properly.
*/

void del(struct hash_table *table, const wchar_t *key );

/*
 Goes through a hash table and calls the function passed to it
 for each node that has been inserted.  The function is passed
 a pointer to the key, and a pointer to the item associated
 with it.
*/

void enumerate(struct hash_table *table,void (*func)(wchar_t *,void *));


/* access to iterator */
inline void set_iterator_position(hash_table*, size_t);
inline void set_iterator_item(hash_table*, void*);
inline void set_iterator_bucket(hash_table*, bucket*);

inline void* item(hash_table*);
inline size_t position(hash_table*);
inline wchar_t* key (hash_table*);


/* other iteration facilities */
inline void start(hash_table*);
inline void forth(hash_table*);
/* should an off be included ? */


/*
 Frees a hash table.  For each node that was inserted in the table,
 it calls the function whose address it was passed, with a pointer
 to the item that was in the table.  The function is expected to
 free the item.  Typical usage would be:
 free_table(&table, free);
 if the item placed in the table was dynamically allocated, or:
 free_table(&table, NULL);
 if not.  ( If the parameter passed is NULL, it knows not to call
 any function with the item. )
*/

void free_table(hash_table *table); /* , void (*func)(void *)); */

void printer (wchar_t *, void*);

void print_table (hash_table *table);


#endif /* HASH__H */
