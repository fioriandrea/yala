#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./semantics.h"

/* list */

#define LIST_DECLARE(name, type) \
        struct name \
        { \
                int len; \
                int cap; \
                type *buffer; \
        }; \
        void name##_init(struct name *list); \
        int name##_push(struct name *list, type data); \
        type name##_pop(struct name *list); \
        type name##_at(struct name *list, int i); \
        int name##_len(struct name *list); \
        void name##_free(struct name *list);

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

void
bytecode_init(struct bytecode *code)
{
        bytes_init(&code->code);
        linelist_init(&code->lines);
        valuelist_init(&code->constants);
}

int
bytecode_write_byte(struct bytecode *code, uint8_t byte, struct lineinfo linfo)
{
        linelist_push(&code->lines, linfo);
        return bytes_push(&code->code, byte);
}

int
bytecode_write_constant(struct bytecode *code, struct value val, struct lineinfo linfo)
{
        int addr = valuelist_push(&code->constants, val) - 1;
        return bytecode_write_byte(code, addr, linfo);
}

uint8_t
bytecode_byte_at(struct bytecode *code, int i)
{
        return bytes_at(&code->code, i);
}

struct lineinfo
bytecode_lineinfo_at(struct bytecode *code, int i)
{
        return linelist_at(&code->lines, i);
}

struct value
bytecode_constant_at(struct bytecode *code, uint8_t address)
{
        return valuelist_at(&code->constants, address);
}

void
value_print(struct value v)
{
        switch (v.type) {
        case VAL_INTEGER:
                printf("%d", v.as.integer);
                return;
        case VAL_BOOLEAN:
                printf("%s", v.as.boolean ? "true" : "false");
                return;
        case VAL_STRING:
                printf("%.*s", v.as.string.length, v.as.string.str);
                return;
        }
        printf("unreachable value type %d", v.type);
}

struct value
value_from_c_int(int i)
{
        struct value v;
        v.type = VAL_INTEGER;
        v.as.integer = i;
        return v;
}

struct value
value_from_c_bool(int b)
{
        struct value v;
        v.type = VAL_BOOLEAN;
        v.as.boolean = !!b;
        return v;
}

/* http://www.cse.yorku.ca/~oz/hash.html */
unsigned long
hash_string(char *str, int length)
{
        unsigned long hash = 5381;
        int c;
        for (int i = 0; i < length; i++) {
                c = str[i];
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        return hash;
}

struct value_string
copy_string(char *str, int length)
{
        struct value_string vs;
        char *cp = malloc(sizeof(char) * length);
        memcpy(cp, str, length);
        vs.length = length;
        vs.str = cp;
        vs.hash = hash_string(str, length);
        return vs;

}

struct value_string
value_string_from_token(struct token token)
{
        return copy_string(token.start, token.length);
}

struct value
value_from_token(struct token token)
{
        struct value v;
        v.type = VAL_STRING;
        v.as.string = value_string_from_token(token);
        return v;
}

struct value
value_from_c_string(char *str)
{
        struct value v;
        v.type = VAL_STRING;
        v.as.string = copy_string(str, strlen(str));
        return v;
}

int
values_equal(struct value val0, struct value val1)
{
        switch (val0.type) {
        case VAL_INTEGER:
                return val0.as.integer == val1.as.integer;
        case VAL_BOOLEAN:
                return val0.as.boolean == val1.as.boolean;
        case VAL_STRING:
                if (val0.as.string.hash != val1.as.string.hash)
                        return 0;
                return val0.as.string.length == val1.as.string.length && memcmp(val0.as.string.str, val1.as.string.str, val0.as.string.length) == 0;
        }
        return -1;
}

int
compare_values(struct value val0, struct value val1)
{
        switch (val0.type) {
                case VAL_STRING:
                        return memcmp(val0.as.string.str, val1.as.string.str, val0.as.string.length);
                case VAL_INTEGER:
                        return val0.as.integer - val1.as.integer;
        }
}