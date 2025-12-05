#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "btree.h"
#include "stringtools.h"

int main(void)
{
    struct btree *t = btree_create();
    if (!t) {
        fprintf(stderr, "btree_create failed\n");
        return 2;
    }

    printf("Inserting 1..10\n");
    for (int i = 1; i <= 10; ++i) {
        btree_insert(t, (btree_key_t)i, string_format("item-%d", i));
    }

    printf("\nIterate once with a cursor (print all):\n");
    btree_key_t key;
    char * value;
    BTREE_ITERATE(t,key,value) {
       printf("value: %s\n",value); 
    }

    printf("\nRemove key 5 with btree_remove():\n");
    void *removed = btree_remove(t, 5);
    if (removed) {
        printf("  removed key 5 => %s\n", (char *)removed);
        free(removed);
    } else {
        printf("  key 5 not found\n");
    }

    printf("\nIterate again with cursor and remove even-keyed items\n");

    BTREE_ITERATE(t,key,value) {
        if(key%2) {
	    char *r = btree_remove_item(t);
            assert(r==value);
        } else {
            printf("  keep: %s\n", value);
        }
    }

    printf("\nFinal sweep: remove remaining items via a cursor and free their value:\n");
    BTREE_ITERATE(t,key,value) {
	void *r = btree_remove_item(t);
        printf("  freeing: %s\n", (char *)r);
        free(r);
    }

    btree_delete(t);

    printf("\nTest complete.\n");
    return 0;
}
