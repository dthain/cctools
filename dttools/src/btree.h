/*
Copyright (C) 2025 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

/** @file btree.h Binary tree data structure.
 * 
 * This binary tree implements a sorted collection of key/value pairs,
 * where keys are 64-bit integers and values are arbitrary C objects.
 * Keys are unique -- you cannot insert two items with the same key.
 * 
 * The fundamental tree operations are insert, lookup, and remove,
 * using the key.  These are all run in O(log n) time on average.
 * 
 * In addition, btree supports efficient iteration over the set of
 * objects in key order from highest to lowest.   This is done by
 * creating a cursor, positioning it at the first item, and then
 * calling btree_cursor_next to obtain each successive item.
 * It is permitted to remove objects while iterating.  If the object
 * at the cursor position is removed, the cursor will relocate to
 * the next item by key value automatically.
 * 
 * To simplify the most common use cases, the tree has a default iterator
 * built in, which can be accessed using first_item, next_item, and remove_item:asm
 
 <pre>
btree_first_item( tree );  // positions the cursor before the first item
while((value = btree_next_item(tree))) {
    printf("%s\n",value);
    if( some_condition ) {
        btree_remove_item( tree ); // removes the item just returned
    }
}
</pre>

 This pattern can also be accomplished with the macro BTREE_ITERATE:
 <pre>
BTREE_ITERATE( tree, value ) {
    printf("%s\n",value);  
    if( some_condition ) {
        btree_remove_item( tree ); // removes the item just returned
    }
}
 </pre>
 */
#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>

struct btree;
struct btree_cursor;

typedef int (*btree_visitor_t) ( void *data );
typedef int64_t btree_key_t;

struct btree * btree_create();
void   btree_delete( struct btree *t );
void   btree_insert( struct btree *t, btree_key_t key, void *data );
void * btree_remove( struct btree *t, btree_key_t key );

struct btree_cursor * btree_cursor_create( struct btree *tree );
btree_key_t btree_cursor_key( struct btree_cursor *c );
void * btree_cursor_value( struct btree_cursor *c );
void   btree_cursor_first( struct btree_cursor *c );
void * btree_cursor_next( struct btree_cursor *c );
void * btree_cursor_remove( struct btree_cursor *c );
void   btree_cursor_delete( struct btree_cursor *c );

void   btree_first_item( struct btree *t );
void * btree_next_item( struct btree *t );
void * btree_remove_item( struct btree *t );

#define BTREE_ITERATE( tree, data ) btree_first_item(tree); while( (data = btree_next_item(tree)) )

#endif
