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
    char * value;
    BTREE_ITERATE(t,value) {
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

    struct btree_cursor *c = btree_cursor_create(t);
    printf("\nIterate again with cursor and remove even-keyed items via cursor_remove():\n");
    btree_cursor_first(c);
    while ((value = btree_cursor_next(c))) {
        if(btree_cursor_key(c)%2) {
            char *r = btree_cursor_remove(c);
            assert(r==value);
        } else {
            printf("  keep: %s\n", value);
        }
    }
    btree_cursor_delete(c);

    printf("\nFinal sweep: remove remaining items via a cursor and free their value:\n");
    c = btree_cursor_create(t);
    btree_cursor_first(c);
    while ((value = btree_cursor_next(c))) {
        void *r = btree_cursor_remove(c);
        printf("  freeing: %s\n", (char *)r);
        free(r);
    }
    btree_cursor_delete(c);

    btree_delete(t);

    printf("\nTest complete.\n");
    return 0;
}