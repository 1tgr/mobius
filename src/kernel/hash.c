//////////////////////////////////////////////////////////////////////////////
// Project: Redpants Kernel                                                 //
// Date:    5-12-2000                                                       //
// Module:  hashtable.c                                                     //
// Purpose: This portion of the shell contains the code which implements    //
//          hashtables to allow the console to keep track of commands.      //
//                                                                          //
//                    Created 2000, by Ben L. Titzer                        //
//////////////////////////////////////////////////////////////////////////////

#include <kernel/hash.h>
#include <wchar.h>
#include <stdlib.h>

#define DEFAULT_SIZE 31

//hashtable_t preinit;
//hashelem_t pretable[DEFAULT_SIZE];

hashtable_t *hashCreate(dword size)
{
  word cntr;
  hashtable_t *h;

// allocate the hashtable structure
  h = (hashtable_t *)malloc(sizeof(hashtable_t));

  if ( h == NULL ) return NULL; // if allocation failed, return NULL

// try to allocate the table
  h->table = (hashelem_t *)malloc(sizeof(hashelem_t)*size);

  if ( h->table == NULL ) // if we failed
     {
     free(h); // free the hashtable structure
     return NULL;    // return NULL
     }

  h->size = size;  // store the size of the table
  h->used = 0;     // none are used yet

  for ( cntr = 0; cntr < size; cntr++ ) // clear all data in hashtable
    {
    h->table[cntr].str = NULL;
    h->table[cntr].data = NULL;
    }

  return h;
}

dword hashInsert(hashtable_t *h, hashelem_t *e)
{
  dword hval;
  dword probe;
  hashelem_t *f;

  if ( e->str == NULL ) return 0; // don't insert empty strings

  if ( (f = hashFind(h, e->str)) != NULL ) // if duplicate
     {
     f->data = e->data;  // update data
     return 1;
     }

  hval = hash(e->str) % h->size;

  if ( h->table[hval].str != NULL ) // if spot is already taken
     {
     for ( probe = 2; probe < 10; probe++ ) // do quadratic probing
        {
        hval = (hval+probe*probe)%h->size;
        if ( h->table[hval].str == NULL ) break;
        }
     if ( probe == 10 ) return 0; // couldn't find a spot after
     }

  h->table[hval] = *e;
  h->used++;

  return 1;
}

dword hash(const wchar_t* s)
{
 dword hash = 0;
 wchar_t c;

  while ( (c = *s++) ) // loop through string
      hash = c + (hash << 6) + (hash << 16) - hash; // update hash value

  return hash;
}

hashelem_t *hashFind(hashtable_t *h, const wchar_t* s)
{
  dword probe = 2;
  dword hval;

  if ( s == NULL ) return NULL;

  hval = hash(s) % h->size;

  while ( (h->table[hval].str == NULL) || (wcsicmp(s, h->table[hval].str) != 0) )
     {
     if ( probe == 9 ) return NULL;     // we didnt find it
     hval = (hval+probe*probe)%h->size; // quadratic probe
     probe++;
     }

  return h->table + hval;
}

void hashList(hashtable_t *h, void (*callback)(hashelem_t *e))
{
  dword cntr;

  for ( cntr = 0; cntr < h->size; cntr++ )
     {
     if ( h->table[cntr].str != NULL ) callback(h->table + cntr);
     }
}
