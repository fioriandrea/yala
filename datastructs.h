#ifndef datastructs_h
#define datastructs_h

#define LIST_FUNCTIONS(name, type) \
        _INIT_LIST(name, type) \
        _PUSH(name, type) \
        _POP(name, type) \
        _FREE_LIST(name, type)

#define NEW_ARRAY_CAP(cap) ((cap) < 8 ? 8 : (cap) * 2)

#define GROW_ARRAY(type, buffer, oldcap, newcap) \
        ((type *) grow_array((void *) (buffer), oldcap, newcap, sizeof(type)))

#define _LIST_STRUCTURE_DEF(name, type) \
        struct name \
        { \
                int len; \
                int cap; \
                type *buffer; \
        };

#define _INIT_LIST(name, type) \
        void name##_init(struct name *list) \
        { \
                list->len = list->cap = 0; \
                list->buffer = NULL; \
        } \

#define _PUSH(name, type) \
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

#define _POP(name, type) \
        type name##_pop(struct name *list) \
        { \
                return list->buffer[--list->len]; \
        }

#define _FREE_LIST(name, type) \
        void name##_free(struct name *list) \
        { \
                GROW_ARRAY(type, list->buffer, list->cap, 0); \
        }

void *grow_array(void *buffer, int oldcap, int newcap, size_t size);

_LIST_STRUCTURE_DEF(bytes, uint8_t)
void bytes_init(struct bytes *list);
int bytes_push(struct bytes *list, uint8_t data);
uint8_t bytes_pop(struct bytes *list);
void bytes_free(struct bytes *list);

#endif