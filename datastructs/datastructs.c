#include <stdlib.h>
#include <stdint.h>

#include "datastructs.h"
#include "../semantics/semantics.h"

/* list */

#define LIST_DEFINE(name, type) \
        LIST_INIT(name, type) \
        LIST_PUSH(name, type) \
        LIST_POP(name, type) \
        LIST_AT(name, type) \
        LIST_LEN(name, type) \
        LIST_FREE(name, type)

#define NEW_ARRAY_CAP(cap) ((cap) < 8 ? 8 : (cap) * 2)

#define GROW_ARRAY(type, buffer, oldcap, newcap) \
        ((type *) grow_array((void *) (buffer), oldcap, newcap, sizeof(type)))

#define LIST_INIT(name, type) \
        void name##_init(struct name *list) \
        { \
                list->len = list->cap = 0; \
                list->buffer = NULL; \
        } \

#define LIST_PUSH(name, type) \
        int name##_push(struct name *list, type data) \
        { \
                if (list->len + 1 > list->cap) { \
                        int newcap = NEW_ARRAY_CAP(list->cap); \
                        list->buffer = GROW_ARRAY(type, list->buffer, list->cap, newcap); \
                        list->cap = newcap; \
                } \
                list->buffer[list->len++] = data; \
                return list->len; \
        }

#define LIST_POP(name, type) \
        type name##_pop(struct name *list) \
        { \
                return list->buffer[--list->len]; \
        }

#define LIST_AT(name, type) \
        type name##_at(struct name *list, int i) \
        { \
                return list->buffer[i]; \
        }

#define LIST_LEN(name, type) \
        int name##_len(struct name *list) \
        { \
                return list->len; \
        }

#define LIST_FREE(name, type) \
        void name##_free(struct name *list) \
        { \
                list->buffer = GROW_ARRAY(type, list->buffer, list->cap, 0); \
        }

static void *
grow_array(void *buffer, int oldcap, int newcap, size_t size)
{
        if (newcap == 0 && buffer != NULL) {
                free(buffer);
                return NULL;
        } else if (oldcap == 0) {
                return malloc(size * newcap);
        } else {
                return realloc(buffer, size * newcap);
        }
}

LIST_DEFINE(bytes, uint8_t)
LIST_DEFINE(linelist, struct lineinfo)
LIST_DEFINE(valuelist, struct value)

/* hash table */

#define TABLE_RESIZE_THRESHOLD 1

#define TABLE_GROW_CAP(cap) ((cap) < 100 ? 100 : (int) (2 * (cap)))

#define TABLE_DEFINE(name, keytype, valtype) \
        TABLE_INIT(name, keytype, valtype) \
        TABLE_TABLE_RESIZE(name, keytype, valtype) \
        TABLE_TABLE_NODE(name, keytype, valtype) \
        TABLE_HAS(name, keytype, valtype) \
        TABLE_GET(name, keytype, valtype) \
        TABLE_SET(name, keytype, valtype) \
        TABLE_FREE(name, keytype, valtype)

#define TABLE_INIT(name, keytype, valtype) \
        void name##_init(struct name *table, int (*hash)(keytype, int), int (*keyequal)(keytype, keytype)) \
        { \
                table->cap = table->nnodes = 0; \
                table->buckets = NULL; \
                table->hash = hash; \
                table->keyequal = keyequal; \
        }

#define TABLE_TABLE_RESIZE(name, keytype, valtype) \
        void name##_table_resize(struct name *table) \
        { \
                int newcap = TABLE_GROW_CAP(table->cap); \
                struct name##_table_node **newbuckets; \
                newbuckets = calloc(newcap, sizeof(struct name##_table_node *)); \
                int i, index; \
                for (i = 0; i < table->cap; i++) { \
                        struct name##_table_node *node, *next; \
                        node = table->buckets[i]; \
                        while (node != NULL) { \
                                index = (*table->hash)(node->key, newcap); \
                                next = node->next; \
                                node->next = newbuckets[index]; \
                                newbuckets[index] = node; \
                                node = next; \
                        } \
                } \
                if (table->buckets != NULL) \
                        free(table->buckets); \
                table->buckets = newbuckets; \
                table->cap = newcap; \
        }

#define TABLE_TABLE_NODE(name, keytype, valtype) \
        struct name##_table_node *name##_node_get(struct name *table, keytype key) \
        { \
                struct name##_table_node *node = table->cap == 0 ? NULL : table->buckets[(*(table->hash))(key, table->cap)]; \
                while (node != NULL) { \
                        if ((*table->keyequal)(key, node->key)) { \
                                break; \
                        } \
                        node = node->next; \
                } \
                return node; \
        }


#define TABLE_HAS(name, keytype, valtype) \
        int name##_has(struct name *table, keytype key) \
        { \
                struct name##_table_node *node = name##_node_get(table, key); \
                return node != NULL; \
        }

#define TABLE_GET(name, keytype, valtype) \
        valtype name##_get(struct name *table, keytype key) \
        { \
                struct name##_table_node *node = name##_node_get(table, key); \
                return node->value; \
        }

#define TABLE_SET(name, keytype, valtype) \
        int name##_set(struct name *table, keytype key, valtype value) \
        { \
                if (name##_has(table, key)) \
                        return 0; \
                table->nnodes++; \
                if (table->cap == 0 || (double) table->nnodes / (double) table->cap > TABLE_RESIZE_THRESHOLD) { \
                        name##_table_resize(table); \
                } \
                int index = (*table->hash)(key, table->cap); \
                struct name##_table_node *node = malloc(sizeof(struct name##_table_node)); \
                node->next = table->buckets[index]; \
                node->key = key; \
                node->value = value; \
                table->buckets[index] = node; \
                return 1; \
        }


#define TABLE_FREE(name, keytype, valtype) \
        void name##_free(struct name *table) \
        { \
                struct name##_table_node *node, *next; \
                int i; \
                for (i = 0; i < table->cap; i++) { \
                        node = table->buckets[i]; \
                        while (node != NULL) { \
                                next = node->next; \
                                free(node); \
                                node = next; \
                        } \
                } \
                if (table->buckets != NULL) \
                        free(table->buckets); \
        }
