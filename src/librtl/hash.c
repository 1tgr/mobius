/* $Id: hash.c,v 1.2 2002/04/20 12:47:28 pavlovskii Exp $
   +++Date last modified: 05-Jul-1997  
   Modified: $LOG$
   By: $Author: pavlovskii $
*/

/*#include <stdio.h>*/


#include <os/hash.h>
#include <wchar.h>
#include <assert.h>

/* forward declarations */
inline static void set_iterator(hash_table*, bucket*, size_t);
inline static void next_bucket (hash_table*);
inline static void skip_empty_slots(hash_table*);




/*
  public domain code by Jerry Coffin, with improvements by HenkJan Wolthuis.
  
  Tested with Visual C 1.0 and Borland C 3.1.
  Compiles without warnings, and seems like it should be pretty
  portable.
*/





/* Initialize the hash_table to the size asked for.  Allocates space
   for the correct number of pointers and sets them to NULL.  If it
   can't allocate sufficient memory, signals error by returnin NULL

*/



hash_table *new_hash_table(size_t size)
{
      size_t i;
      bucket **temp;
      hash_table *result;
      hash_table_iterator *t_it;
      /*off=TRUE; found=FALSE;*/
      
      
      /* default initialization, I think I've to do that by hand in C*/ 
      
      
      result = malloc(sizeof(hash_table));
      if (!result){
        return NULL;
      }
      
      t_it = malloc(sizeof(hash_table_iterator));
      if (!t_it){
        free(result);
        return NULL;
      }
      /* t_b = malloc (sizeof(bucket)); */
/*       if (!t_b){ */
/*           free(t_it); */
/*           free(result); */
/*           return NULL; */
/*       } */
      result->iterator = t_it;
      set_iterator(result,NULL,(size_t)0);
      
      result -> size  = size;
      temp = (bucket * *)malloc(sizeof(bucket *) * size);
      
      if (!temp){
          free(result->iterator);
          free(result);
          return NULL;
      }
      
      for (i=0;i<size;i++)
          temp[i] = NULL;
      
      result->table = temp;
      result->off = TRUE;
      result->found = FALSE;
      return result;
}




/*
 Hashes a string to produce an unsigned short, which should be
 sufficient for most purposes.
*/

unsigned hash(const wchar_t *string)
{
      unsigned int ret_val = 0;
      int i;

      while (*string)
      {
            i = (int)*string;
            ret_val ^= i;
            ret_val <<= 1;
            string ++;
      }
      return ret_val;
}

/*
 Insert 'key' into hash table.
 Returns pointer to old item associated with the key, or the pointer to the
 new item if the key wasn't in the table before and NULL if Memory allocation 
 failes
*/

void insert(hash_table *table, void *item, const wchar_t *key)
{
    unsigned val = hash(key) % table->size;
    bucket *ptr;

    /*
      NULL means this bucket hasn't been used yet.  We'll simply
       allocate space for our new bucket and put our item there, with
       the table pointing at it.
    */
    
    if (NULL == (table->table)[val])
    {
        (table->table)[val] = (bucket *)malloc(sizeof(bucket));
        if (NULL==(table->table)[val]){
            table->insertation_failed =  TRUE;
            return;
        }
        
        (table->table)[val] -> key = _wcsdup(key);
        (table->table)[val] -> next = NULL;
        (table->table)[val] -> item = item;
        table->insertation_failed = FALSE;
        return;
        /* return (table->table)[val] -> item; */
    }
    
    for (ptr = (table->table)[val];NULL != ptr; ptr = ptr -> next){
        if (0 == _wcsicmp(key, ptr->key)){
            ptr -> item = item;
            table->insertation_failed = FALSE;
            return;
        }
    }
    /*
      This key must not be in the table yet.  We'll add it to the head of
      the list at this spot in the hash table.  Speed would be
      slightly improved if the list was kept sorted instead.  In this case,
      this code would be moved into the loop above, and the insertion would
      take place as soon as it was determined that the present key in the
      list was larger than this one.
    */
    
    ptr = (bucket *)malloc(sizeof(bucket));
    if (NULL==ptr){
        table->insertation_failed = TRUE;
        return;
    }
    ptr -> key = _wcsdup(key);
    ptr -> item = item;
    ptr -> next = (table->table)[val];
    (table->table)[val] = ptr;
    
}


/*
  Look up a key and set found to TRUE if found and adjust values in iterator
*/

void search(hash_table *table, const wchar_t *key) {
    
    unsigned val = hash(key) % table->size;
    bucket *ptr;
    /* standard not found */
    table->found = FALSE;
    table->off = TRUE;
    set_iterator_position(table,(size_t)val);
    
    if (NULL == (table->table)[val]){
        return;
    }
    
    for ( ptr = (table->table)[val];NULL != ptr; ptr = ptr->next ){
        if (0 == _wcsicmp(key, ptr -> key ) ){
            /* how lucky we're, found ;-) */
            table->found = TRUE;
            table->off = FALSE;
            set_iterator_bucket(table,ptr);
            return;
        }
    }
}


/* other iteratio facility (step-by-step) 
   implementation */
inline static void set_iterator(hash_table* table,
                                bucket *new_bucket,
                                size_t new_position){
    
    assert(table); /* act_pos should be there */
    
    set_iterator_bucket(table,new_bucket);
    set_iterator_position(table,new_position);
}


inline void set_iterator_position(hash_table *table, size_t val){
    assert(table->iterator);
    
    table->iterator->it_position = val;
}

inline void set_iterator_item(hash_table *table, void *val){
    assert(table->iterator->it_bucket);
    
    table->iterator->it_bucket->item = val;
}

inline void set_iterator_bucket(hash_table *table, bucket *val){
    assert(table->iterator);
    table->iterator->it_bucket = val;
}


inline void start(hash_table *table){
    set_iterator_position(table, 0);
    skip_empty_slots (table);
}

inline void forth(hash_table *table){
    assert(!table->off); /* don't have to be if the table is empty */
    assert(table->iterator->it_bucket);
    next_bucket(table);
}


inline void* item (hash_table *table){
  assert(table->iterator->it_bucket);
  return table->iterator->it_bucket->item;
}

inline size_t position (hash_table *table){
  assert(table->iterator);
  return table->iterator->it_position;
}

inline wchar_t* key(hash_table *table){
  assert(table->iterator->it_bucket->key);
  return table->iterator->it_bucket->key;
}

inline static void next_bucket (hash_table* table){
    /* find next bucket which is not void 
       adjust values in iterator */
    
    bucket *tb = table->iterator->it_bucket;
    
    if (tb->next != NULL){
        tb = tb-> next;
        assert(tb);
        table->off = FALSE;
        set_iterator_bucket(table,tb);
    }else{ /* must be at the end of the linked list of buckets */
        set_iterator_position(table, position(table) + 1);
        skip_empty_slots(table);   
    }
}



inline static void skip_empty_slots(hash_table  *table){
    /* assert(table);*/
    /* assert(NULL != it); */
    int tp = table->iterator->it_position;
    bucket *tbucket=NULL;
    
    while(tp < table->size){
        tbucket = (table->table)[tp];
        if (tbucket){
            table->off = FALSE;
            break;
        }
        tp++;
    }
    set_iterator(table,tbucket, tp);
    if (tp >= table->size)
        table->off = TRUE;
}





/*
  Frees a complete table by iterating over it and freeing each node.
  the second parameter is the address of a function it will call with a
  pointer to the item associated with each node.  This function is
  responsible for freeing the item, or doing whatever is needed with
  it.
*/

static void free_bucket(bucket* t_bucket){
    /* eif_wean((EIF_OBJ)t_bucket->item); */
    free(t_bucket->key);
    free(t_bucket);
}

static void free_bucket_entries(hash_table *table){
    int i;
    bucket *element_to_delete, *t_item;
    for (i=0; i< table->size; i++){
        t_item = table->table[i];
        while(t_item) {
            element_to_delete = t_item;
            t_item = t_item->next;
            free_bucket(element_to_delete);
        }
        
    }
}

void free_table(hash_table *table){
    
        
    table->off = TRUE;
    table->found = FALSE;
    
    free_bucket_entries(table);        
    free(table->iterator);
    free(table->table);
    free(table);
}



/*
** Delete a key from the hash table and return associated
** item, or NULL if not present.
*/

void del(hash_table *table, const wchar_t *key )
{
      unsigned val = hash(key) % table->size;
      bucket *ptr, *last = NULL;

      if (NULL == (table->table)[val])
            return;
      
      /*
      ** Traverse the list, keeping track of the previous node in the list.
      ** When we find the node to delete, we set the previous node's next
      ** pointer to point to the node after ourself instead.  We then delete
      ** the key from the present node, and return a pointer to the item it
      ** contains.
      */
      
      for (last = NULL, ptr = (table->table)[val];
           NULL != ptr;
           last = ptr, ptr = ptr->next)
      {
          if (0 == _wcsicmp(key, ptr -> key))
          {
              if (last != NULL )
              {
                  last -> next = ptr -> next;
                  free(ptr->key);
                  free(ptr);
                  return;
              }
              
              /*
              ** If 'last' still equals NULL, it means that we need to
              ** delete the first node in the list. This simply consists
              ** of putting our own 'next' pointer in the array holding
              ** the head of the list.  We then dispose of the current
              ** node as above.
              */
              
              else
              {
                  (table->table)[val] = ptr->next;
                  free(ptr->key);
                  free(ptr);
                  return;
              }
          }
      }
      
      /*
      ** If we get here, it means we didn't find the item in the table.
      ** Signal this by returning NULL.
      */
      
      /* return NULL; */
}