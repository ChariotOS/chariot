/*
  Copyright (c) 2002, 2004, Christopher Clark
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  
  * Neither the name of the original author; nor the names of any contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.
  
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* Modifications made by Jack Lange <jarusl@cs.northwestern.edu> */
#pragma once

struct nk_hashtable;

/* Example of use:
 *
 *      struct hashtable  *h;
 *      struct some_key   *k;
 *      struct some_value *v;
 *
 *      static unsigned         hash_from_key_fn( void *k );
 *      static int                  keys_equal_fn ( void *key1, void *key2 );
 *
 *      h = create_hashtable(16, hash_from_key_fn, keys_equal_fn);
 *      k = (struct some_key *)     malloc(sizeof(struct some_key));
 *      v = (struct some_value *)   malloc(sizeof(struct some_value));
 *
 *      (initialise k and v to suitable values)
 * 
 *      if (! hashtable_insert(h,k,v) )
 *      {     exit(-1);               }
 *
 *      if (NULL == (found = hashtable_search(h,k) ))
 *      {    printf("not found!");                  }
 *
 *      if (NULL == (found = hashtable_remove(h,k) ))
 *      {    printf("Not found\n");                 }
 *
 */

/* Macros may be used to define type-safe(r) hashtable access functions, with
 * methods specialized to take known key and value types as parameters.
 * 
 * Example:
 *
 * Insert this at the start of your file:
 *
 * DEFINE_HASHTABLE_INSERT(insert_some, struct some_key, struct some_value);
 * DEFINE_HASHTABLE_SEARCH(search_some, struct some_key, struct some_value);
 * DEFINE_HASHTABLE_REMOVE(remove_some, struct some_key, struct some_value);
 *
 * This defines the functions 'insert_some', 'search_some' and 'remove_some'.
 * These operate just like hashtable_insert etc., with the same parameters,
 * but their function signatures have 'struct some_key *' rather than
 * 'void *', and hence can generate compile time errors if your program is
 * supplying incorrect data as a key (and similarly for value).
 *
 * Note that the hash and key equality functions passed to create_hashtable
 * still take 'void *' parameters instead of 'some key *'. This shouldn't be
 * a difficult issue as they're only defined and passed once, and the other
 * functions will ensure that only valid keys are supplied to them.
 *
 * The cost for this checking is increased code size and runtime overhead
 * - if performance is important, it may be worth switching back to the
 * unsafe methods once your program has been debugged with the safe methods.
 * This just requires switching to some simple alternative defines - eg:
 * #define insert_some hashtable_insert
 *
 */

#define DEFINE_HASHTABLE_INSERT(fnname, keytype, valuetype)		\
    static int fnname (struct nk_hashtable * htable, keytype key, valuetype value) { \
	return nk_htable_insert(htable, (unsigned long)key, (unsigned long)value);	\
    }

#define DEFINE_HASHTABLE_SEARCH(fnname, keytype, valuetype)		\
    static valuetype * fnname (struct nk_hashtable * htable, keytype  key) { \
	return (valuetype *) (nk_htable_search(htable, (unsigned long)key));	\
    }

#define DEFINE_HASHTABLE_REMOVE(fnname, keytype, valuetype, free_key)	\
    static valuetype * fnname (struct nk_hashtable * htable, keytype key) { \
	return (valuetype *) (nk_htable_remove(htable, (unsigned long)key, free_key)); \
    }





/* These cannot be inlined because they are referenced as fn ptrs */
unsigned long nk_hash_long(unsigned long val, unsigned bits);
unsigned long nk_hash_buffer(unsigned char * msg, unsigned length);



struct nk_hashtable * nk_create_htable(unsigned min_size,
				    unsigned (*hashfunction) (unsigned long key),
				    int (*key_eq_fn) (unsigned long key1, unsigned long key2));

void nk_free_htable(struct nk_hashtable * htable, int free_values, int free_keys);

/*
 * returns non-zero for successful insertion
 *
 * This function will cause the table to expand if the insertion would take
 * the ratio of entries to table size over the maximum load factor.
 *
 * This function does not check for repeated insertions with a duplicate key.
 * The value returned when using a duplicate key is undefined -- when
 * the hashtable changes size, the order of retrieval of duplicate key
 * entries is reversed.
 * If in doubt, remove before insert.
 */
int nk_htable_insert(struct nk_hashtable * htable, unsigned long key, unsigned long value);

int nk_htable_change(struct nk_hashtable * htable, unsigned long key, unsigned long value, int free_value);


// returns the value associated with the key, or NULL if none found
unsigned long nk_htable_search(struct nk_hashtable * htable, unsigned long key);

// returns the value associated with the key, or NULL if none found
unsigned long nk_htable_remove(struct nk_hashtable * htable, unsigned long key, int free_key);

unsigned nk_htable_count(struct nk_hashtable * htable);

// Specialty functions for a counting hashtable 
int nk_htable_inc(struct nk_hashtable * htable, unsigned long key, unsigned long value);
int nk_htable_dec(struct nk_hashtable * htable, unsigned long key, unsigned long value);


/* ************ */
/* ITERATOR API */
/* ************ */

#define DEFINE_HASHTABLE_ITERATOR_SEARCH(fnname, keytype)		\
   static int fnname (struct nk_hashtable_itr * iter, struct nk_hashtable * htable, keytype * key) { \
	return (nk_htable_iter_search(iter, htable, key));		\
    }



/*****************************************************************************/
/* This struct is only concrete here to allow the inlining of two of the
 * accessor functions. */
struct nk_hashtable_iter {
    struct nk_hashtable * htable;
    struct nk_hash_entry * entry;
    struct nk_hash_entry * parent;
    unsigned index;
};


struct nk_hashtable_iter * nk_create_htable_iter(struct nk_hashtable * htable);
void nk_destroy_htable_iter(struct nk_hashtable_iter * iter);

/* - return the value of the (key,value) pair at the current position */
//extern inline 
unsigned long nk_htable_get_iter_key(struct nk_hashtable_iter * iter);
/* {
   return iter->entry->key;
   }
*/


/* value - return the value of the (key,value) pair at the current position */
//extern inline 
unsigned long nk_htable_get_iter_value(struct nk_hashtable_iter * iter);
/* {
   return iter->entry->value;
   }
*/



/* returns zero if advanced to end of table */
int nk_htable_iter_advance(struct nk_hashtable_iter * iter);

/* remove current element and advance the iterator to the next element
 *          NB: if you need the value to free it, read it before
 *          removing. ie: beware memory leaks!
 *          returns zero if advanced to end of table 
 */
int nk_htable_iter_remove(struct nk_hashtable_iter * iter, int free_key);


/* search - overwrite the supplied iterator, to point to the entry
 *          matching the supplied key.
 *          returns zero if not found. */
int nk_htable_iter_search(struct nk_hashtable_iter * iter, struct nk_hashtable * htable, unsigned long key);























/*
 * Copyright (c) 2002, Christopher Clark
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
