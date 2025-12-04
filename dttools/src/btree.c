
#include "btree.h"
#include "debug.h"
#include "xxmalloc.h"

#include <stdlib.h>

struct btree {
	struct btree_node *root;
	struct btree_cursor *default_cursor;
	unsigned size;
};

struct btree_cursor {
	struct btree *tree;
	struct btree_node *node;
	btree_key_t visited;
};

struct btree_node {
	struct btree_node *parent;
	struct btree_node *left;
	struct btree_node *right;
	btree_key_t key;
	void *data;
};

static struct btree_node * btree_node_create( struct btree_node *parent, struct btree_node *left, struct btree_node *right, btree_key_t key, void *data )
{
	struct btree_node *n = malloc(sizeof(*n));
	n->parent = parent;
	n->left = left;
	n->right = right;
	n->key = key;
	n->data = data;
	return n;
}

static void btree_node_delete( struct btree_node *n )
{
	if(!n) return;
	btree_node_delete(n->left);
	btree_node_delete(n->right);
	free(n);
}

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

static void btree_node_unlink( struct btree *tree, struct btree_node *n )
{
	struct btree_node **pp;
	
	/* First determine where my parent's pointer to me is located. */
	
	if(!n->parent) {
		pp = &tree->root;
	} else if(n->parent->left==n) {
		pp = &n->parent->left;
	} else if(n->parent->right==n) {
		pp = &n->parent->right;
	} else {
		fatal("btree_node_unlink: inconsistent parent link!");
	}

	if(!n->left && !n->right) {
		/* no children, remove me and replace with null*/
		*pp = 0;
	} else if(n->left && !n->right) {
		/* one left child, replace me with left */
		*pp = n->left;
		n->left->parent = n->parent;
	} else if(n->right && !n->left) {
		/* one right child, replace me with right */
		*pp = n->right;
		n->right->parent = n->parent;
	} else {
		/* two children, replace me with next largest node */
		struct btree_node *r = n->right;
		while(r->left) r = r->left;

		/* disconnect that node from the tree */
		btree_node_unlink(tree,r);

		/* move the content of that node here */
		n->key = r->key;
		n->data = r->data;

		/* now toss that node */
		btree_node_delete(r);
	}
	
	/* Null out this node's links so they are not deleted when this node is. */
	n->parent = 0;
	n->left = 0;
	n->right = 0;
}

struct btree * btree_create()
{
	struct btree *t = xxmalloc(sizeof(*t));
	t->root = 0;
	t->size = 0;
	t->default_cursor = btree_cursor_create(t);
	return t;
}

void btree_delete( struct btree *t )
{
	if(!t) return;
	btree_cursor_delete(t->default_cursor);
	btree_node_delete(t->root);
	free(t);
}

void btree_insert( struct btree *t, btree_key_t key, void *data )
{
	struct btree_node *newnode = btree_node_create(0,0,0,key,data);

	if(!t->root) {
		t->root = newnode;
	} else {
		btree_node_insert(t->root,newnode);
	}

	t->size++;
}

void *btree_remove( struct btree *tree, int64_t key )
{
	struct btree_node *n = btree_node_lookup(tree->root,key);
	if(n) {
		void *data = n->data;
		btree_node_unlink(tree,n);
		btree_node_delete(n);
		tree->size--;
		return data;
	} else {
		return 0;
	}		
}

struct btree_cursor * btree_cursor_create( struct btree *tree )
{
	struct btree_cursor *c = malloc(sizeof(*c));
	c->tree = tree;
	c->node = 0;
	c->visited = 0;
	return c;
}

void btree_cursor_delete( struct btree_cursor *c )
{
	free(c);
}

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
			return c->node->data;
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

void * btree_cursor_value( struct btree_cursor *c )
{
	if(!c || !c->node) return 0;
	return c->node->data;
}

btree_key_t btree_cursor_key( struct btree_cursor *c )
{
	if(!c) return 0;
	return c->visited;
}

void * btree_cursor_remove( struct btree_cursor *c )
{
	if(!c) return 0;

	void *data = 0;
	
	if(c->node) {
		data = c->node->data;

		btree_node_unlink(c->tree,c->node);
		btree_node_delete(c->node);
		
		/* drop the pointer to the deleted item */
		/* btree_cursor_next will recover using c->visited. */
		c->node = 0;
	} else {
		data = 0;
	}

	return data;
}

void btree_first_item( struct btree *t )
{
	if(!t) return;
	btree_cursor_first(t->default_cursor);
}
	
void *btree_next_item( struct btree *t )
{
	if(!t) return 0;
	return btree_cursor_next(t->default_cursor);
}

void *btree_remove_item( struct btree *t )
{
	if(!t) return 0;
	return btree_cursor_remove(t->default_cursor);
}	
