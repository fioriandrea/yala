#include <stdlib.h>
#include <stdint.h>

#include "datastructs.h"
#include "../semantics/semantics.h"

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
                GROW_ARRAY(type, list->buffer, list->cap, 0); \
        }

static void *
grow_array(void *buffer, int oldcap, int newcap, size_t size)
{
        if (newcap == 0 && buffer != NULL) {
                free(buffer);
                return NULL;
        } else {
                return realloc(buffer, size * newcap);
        }
}

LIST_DEFINE(bytes, uint8_t)
LIST_DEFINE(linelist, struct lineinfo)
LIST_DEFINE(valuelist, struct value)