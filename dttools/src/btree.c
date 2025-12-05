
#include "btree.h"
#include "debug.h"
#include "xxmalloc.h"

#include <stdlib.h>

/*
A complete tree consists of a root node and a default cursor.
Most operations on the tree as a whole are delegated to either
recursive node operations or cursor operations, so that tree
operations only return user data instead of tree nodes.
*/

struct btree {
	struct btree_node *root;
	struct btree_cursor *default_cursor;
	unsigned size;
};

/*
btree_cursor keeps track of an iteration position within a tree.
The node pointer gives the exact node, while the visited value indicates
the highest key already visited, so we can determine whether to go
left, right, or up next.
*/

struct btree_cursor {
	struct btree *tree;
	struct btree_node *node;
	btree_key_t visited;
};

/*
btree_node is the heard of the structure, keeping track of child nodes
as well as a parent pointer (to facilitate removal).  Each node is described
by an integer key for sorting, and an opaque data value.
*/  

struct btree_node {
	struct btree_node *parent;
	struct btree_node *left;
	struct btree_node *right;
	btree_key_t key;
	void *value;
};

/* Private: Create a new binary tree node. */

static struct btree_node * btree_node_create( struct btree_node *parent, struct btree_node *left, struct btree_node *right, btree_key_t key, void *value )
{
	struct btree_node *n = malloc(sizeof(*n));
	n->parent = parent;
	n->left = left;
	n->right = right;
	n->key = key;
	n->value = value;
	return n;
}

/* Private: Delete a binary tree node, along with all of its children recursively. */

static void btree_node_delete( struct btree_node *n )
{
	if(!n) return;
	btree_node_delete(n->left);
	btree_node_delete(n->right);
	free(n);
}

/*
Private: Insert a new node into the tree by descending to an unused leaf.
For efficiency, work iterately instead of recursively.
*/

static void btree_node_insert( struct btree_node *n, struct btree_node *newnode )
{
	while(1) {		
		if(newnode->key>n->key) {
			if(n->left) {
				n = n->left;
			} else {
				n->left = newnode;
				newnode->parent = n;
				return;
			}
	        } else if(newnode->key<n->key) {
			if(n->right) {
				n = n->right;
			} else {
				n->right = newnode;
				newnode->parent = n;
				return;
			}
		} else {
			fatal("btree_node_insert: insert of duplicate key %llx",n->key);
		}
	}
}

/*
Private: Lookup a given value in the tree by descending to the proper node.
For efficiency, work iterately instead of recursively.
*/

static struct btree_node * btree_node_lookup( struct btree_node *n, btree_key_t key )
{
	if(!n) return 0;
	while(1) {
		if(key>n->key) {
			n = n->left;
		} else if(key<n->key) {
			n = n->right;
		} else {
			return n;
		}
	}
}

/*
Private: Remove a node from the tree by disconnecting its child and parent nodes,
and fixing up the children accordingly.  Note that this does *not*
delete the node itself, which should be done by btree_node_delete.
*/

static void btree_node_unlink( struct btree *tree, struct btree_node *n )
{
	struct btree_node **pparent;
	
	/* First determine where my parent's pointer to me is located. */
	
	if(!n->parent) {
		pparent = &tree->root;
	} else if(n->parent->left==n) {
		pparent = &n->parent->left;
	} else if(n->parent->right==n) {
		pparent = &n->parent->right;
	} else {
		fatal("btree_node_unlink: inconsistent parent link!");
	}

	if(!n->left && !n->right) {
		/* no children, remove me and replace with null*/
		*pparent = 0;
	} else if(n->left && !n->right) {
		/* one left child, replace me with left */
		*pparent = n->left;
		n->left->parent = n->parent;
	} else if(n->right && !n->left) {
		/* one right child, replace me with right */
		*pparent = n->right;
		n->right->parent = n->parent;
	} else {
		/* two children, replace me with next largest node */
		struct btree_node *r = n->right;
		while(r->left) r = r->left;

		/* disconnect that node from the tree */
		btree_node_unlink(tree,r);

		/* move the content of that node here */
		n->key = r->key;
		n->value = r->value;

		/* now toss that node */
		btree_node_delete(r);

		/* do NOT fall through here because n is preserved */
		return;
	}
	
	/* Null out this node's links so that deletion will only affect the removed node. */
	n->parent = 0;
	n->left = 0;
	n->right = 0;
}

/* Public: create a new empty binary tree with a default cursor */

struct btree * btree_create()
{
	struct btree *t = xxmalloc(sizeof(*t));
	t->root = 0;
	t->size = 0;
	t->default_cursor = btree_cursor_create(t);
	return t;
}

/* Public: Delete a tree and all of its internal structure.  (But not the contained values.) */

void btree_delete( struct btree *t )
{
	if(!t) return;
	btree_cursor_delete(t->default_cursor);
	btree_node_delete(t->root);
	free(t);
}

/* Public: Insert an object into the tree at the given key position. */

void btree_insert( struct btree *t, btree_key_t key, void *value )
{
	struct btree_node *newnode = btree_node_create(0,0,0,key,value);

	if(!t->root) {
		t->root = newnode;
	} else {
		btree_node_insert(t->root,newnode);
	}

	t->size++;
}

/* Public: Remove an item from the tree at the key position, if it exists. */

void *btree_remove( struct btree *tree, int64_t key )
{
	struct btree_node *n = btree_node_lookup(tree->root,key);
	if(n) {
		void *value = n->value;
		btree_node_unlink(tree,n);
		btree_node_delete(n);
		tree->size--;
		return value;
	} else {
		return 0;
	}		
}

/* Create a new cursor, pointing to nothing. */

struct btree_cursor * btree_cursor_create( struct btree *tree )
{
	struct btree_cursor *c = malloc(sizeof(*c));
	c->tree = tree;
	c->node = 0;
	c->visited = 0;
	return c;
}

/* Destroy a cursor. */

void btree_cursor_delete( struct btree_cursor *c )
{
	free(c);
}

/* Set the cursor just prior to the first item. */

void btree_cursor_first( struct btree_cursor *c )
{
	c->node = c->tree->root;

	if(c->node) {
		while(c->node->left) c->node = c->node->left;
		/* start just before the first item */
		c->visited = c->node->key + 1;
	} else {
		c->visited = 0;
	}	
}

/*
Advance the cursor to the next item and return its value.
Note that this works even if c->node is lost by recovering position from c->visited.
*/

void * btree_cursor_next( struct btree_cursor *c )
{
	if(!c) return 0;

	/* If we lost the cursor pointer via a delete, start again at the root. */

	if(!c->node) c->node = c->tree->root;

	/* And now let the visited field navigate to the next item. */
	
	while(c->node) {
		if(c->node->left && c->visited > c->node->left->key) {
			/* There are unvisited nodes to the left, go there. */
			c->node = c->node->left;
		} else if(c->visited > c->node->key) {
			/* Time to visit this node and return its key. */
			c->visited = c->node->key;
			return c->node->value;
		} else if(c->node->right && c->visited > c->node->right->key ) {
			/* There are unvisited nodes to the right, go there */
			c->node = c->node->right;
		} else {
			/* We have visited everything, go back up. */
			c->node = c->node->parent;
		}
	}

	/* If we get to the root, we are done, return null to indicate end */
	return 0;
}

/* Return the value at the current cursor position. */

void * btree_cursor_value( struct btree_cursor *c )
{
	if(!c || !c->node) return 0;
	return c->node->value;
}

/* Return the key at the current cursor position. */

btree_key_t btree_cursor_key( struct btree_cursor *c )
{
	if(!c) return 0;
	return c->visited;
}

/*
Remove the value at the current cursor position and return it.
Once removed, the cursor will be "between" objects and return
nothing until btree_cursor_next is called again.
*/

void * btree_cursor_remove( struct btree_cursor *c )
{
	if(!c) return 0;

	void *value = 0;
	
	if(c->node) {
		value = c->node->value;

		btree_node_unlink(c->tree,c->node);
		btree_node_delete(c->node);

		c->tree->size--;
		
		/* drop the pointer to the deleted item */
		/* btree_cursor_next will recover using c->visited. */
		c->node = 0;
	} else {
		value = 0;
	}

	return value;
}

/* Finally, we implement the iteration operations as actions on the default cursor */

void btree_first_item( struct btree *t )
{
	if(!t) return;
	btree_cursor_first(t->default_cursor);
}
	
void *btree_next_item( struct btree *t, btree_key_t * key )
{
	if(!t) return 0;
	void * value = btree_cursor_next(t->default_cursor);
	if(value) *key = btree_cursor_key(t->default_cursor);
	return value;
}

void *btree_remove_item( struct btree *t )
{
	if(!t) return 0;
	return btree_cursor_remove(t->default_cursor);
}	
