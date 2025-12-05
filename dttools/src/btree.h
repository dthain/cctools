/*
Copyright (C) 2025 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

/** @file btree.h Binary tree data structure.
This binary tree implements a sorted collection of key/value pairs,
where keys are 64-bit integers and values are arbitrary C objects.
Insert, lookup, and removal are performed in O(log n) time on average.
This requires that keys are unique: you cannot insert two items with the same key.

For example, to insert, lookup, and remove items:
<pre>
struct btree * tree = btree_create();

btree_insert( tree, 10, "ten" );
s = btree_lookup( tree, 10 );
s = btree_remove( tree, 10 );

btree_delete(tree);
</pre>

btree supports efficient iteration over the set of
objects in key order from highest to lowest.
To do this, call @ref btree_first_item to begin,
call @ref btree_cursor_next to obtain each successive
item until null is returned:

<pre>
btree_key_t key;
void *value;
btree_first_item( tree );
while((value = btree_next_item(tree,&key))) {
    printf("%lld %s\n",key,value);  
}
</pre>

This common pattern can also be accomplished with the macro BTREE_ITERATE:
<pre>
btree_key_t key;
void *value;
BTREE_ITERATE( tree, key, value ) {
    printf("%lld %s\n",key,value);  
}
</pre>

Note that you may call @ref btree_remove_item to remove the
item at the current cursor position -- the internal
of an iteration -- the internal cursor will relocate to
the next item in sequence when @ref btree_next_item is called:

<pre>
BTREE_ITERATE( tree, value ) {
    printf("%s\n",value);  
    if(value>100) {
        btree_remove_item(tree);
    }
</pre>

*/
#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>

struct btree;
struct btree_cursor;

typedef int64_t btree_key_t;

/** Create a new binary tree.
@return A pointer to a new binary tree.
*/

struct btree * btree_create();

/** Insert a value into the binary tree at the key position.
@param t A pointer to a binary tree.
@param key The key at which to insert.  Must be unique.
@param value The value to insert into the tree.
*/

void   btree_insert( struct btree *t, btree_key_t key, void *value );

/** Remove an item from the binary tree at a key position.
@param t A pointer to a binary tree.
@param key The key at which to remove.
@return The value at that position, or null if nothing.
*/

void * btree_remove( struct btree *t, btree_key_t key );


/** Delete a binary tree.
Note that this function will not delete all of the objects contained within in the tree.
@param t The binary tree to delete
*/

void   btree_delete( struct btree *t );

/** Begin iteration over all items.
This function begins a new iteration over a binary tree,
allowing you to visit every key and value in order.
Next, invoke @ref btree_next_item to retrieve each value in order.
@param t A pointer to a binary tree.
*/

void   btree_first_item( struct btree *t );

/** Continue iteration over all keys.
This function returns the next key and value in the iteration.
@param t A pointer to a binary tree.
@param key A pointer to a key which will be filled in.
@return The value at the next position, or null otherwise.
*/

void * btree_next_item( struct btree *t, btree_key_t *key );

/** Remove an item at the current iterator position.
After calling this function, the iterator points
to "nothing" and will advance to the next value
in the tree when @ref btree_next_item is called next.
@param t A pointer to a binary tree.
@return The value at the current position.
*/

void * btree_remove_item( struct btree *t );

/** Utility macro to simplify common case of iterating over a binary tree.  Use as follows:
<pre>
btree_key_t key;
void *value;
BTREE_ITERATE(tree,key,value) {
    printf("%lld %s\n",key,value);  
}
</pre>
*/

#define BTREE_ITERATE( tree, key, value ) btree_first_item(tree); while( (value = btree_next_item(tree,&key)) )


struct btree_cursor * btree_cursor_create( struct btree *tree );
btree_key_t btree_cursor_key( struct btree_cursor *c );
void * btree_cursor_value( struct btree_cursor *c );
void   btree_cursor_first( struct btree_cursor *c );
void * btree_cursor_next( struct btree_cursor *c );
void * btree_cursor_remove( struct btree_cursor *c );
void   btree_cursor_delete( struct btree_cursor *c );

#endif

