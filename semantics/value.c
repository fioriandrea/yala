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
bytecode_write_long(struct bytecode *code, uint16_t l, struct lineinfo linfo)
{
        bytecode_write_byte(code, left_byte(l), linfo);
        return bytecode_write_byte(code, right_byte(l), linfo);
}

int
bytecode_write_constant(struct bytecode *code, struct value val, struct lineinfo linfo)
{
        int addr = valuelist_push(&code->constants, val) - 1;
        return bytecode_write_long(code, addr, linfo);
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
bytecode_constant_at(struct bytecode *code, uint16_t address)
{
        return valuelist_at(&code->constants, address);
}

void
value_print(struct value v)
{
        switch (v.type.id) {
        case VAL_INTEGER:
                printf("%d", v.as.integer);
                return;
        case VAL_BOOLEAN:
                printf("%s", v.as.boolean ? "true" : "false");
                return;
        case VAL_STRING:
                printf("\"%.*s\"", v.as.string.length, v.as.string.str);
                return;
        case VAL_VECTOR:
                printf("[");
                for (int i = 0; i < v.type.size; i++) {
                        value_print(*(v.as.vector.astackent - v.type.size + i));
                        printf(i == v.type.size - 1 ? "" : ", ");
                }
                printf("]");
                return;
        }
        printf("unreachable value type %d", v.type.id);
}

struct value
value_from_c_int(int i)
{
        struct value v;
        v.type = run_type_scalar(VAL_INTEGER);
        v.as.integer = i;
        return v;
}

struct value
value_from_c_bool(int b)
{
        struct value v;
        v.type = run_type_scalar(VAL_BOOLEAN);
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
        v.type = run_type_scalar(VAL_STRING);
        v.as.string = value_string_from_token(token);
        return v;
}

struct value
value_from_c_string(char *str)
{
        struct value v;
        v.type = run_type_scalar(VAL_STRING);
        v.as.string = copy_string(str, strlen(str));
        return v;
}

struct semantic_type
semantic_type_scalar(enum value_type vt)
{

        struct semantic_type type;
        type.id = vt;
        type.base = vt;
        type.rank = 0;
        type.size = 1;
        return type;
}

struct run_type
run_type_scalar(enum value_type vt)
{
        struct run_type type;
        type.id = vt;
        type.rank = 0;
        type.size = 1;
        type.dimensions = NULL;
        return type;
}

int
values_equal(struct value val0, struct value val1)
{
        switch (val0.type.id) {
        case VAL_INTEGER:
                return val0.as.integer == val1.as.integer;
        case VAL_BOOLEAN:
                return val0.as.boolean == val1.as.boolean;
        case VAL_STRING:
                if (val0.as.string.hash != val1.as.string.hash)
                        return 0;
                return val0.as.string.length == val1.as.string.length && memcmp(val0.as.string.str, val1.as.string.str, val0.as.string.length) == 0;
        case VAL_VECTOR:
                for (int i = 0; i < val0.type.size; i++) {
                        struct value ent0 = val0.as.vector.astackent[-i - 1];
                        struct value ent1 = val1.as.vector.astackent[-i - 1];
                        if (!values_equal(ent0, ent1))
                                return 0;
                }
                return 1;
        }
        return -1;
}

int
types_comparable(struct semantic_type lefttype, struct semantic_type righttype)
{
        if (lefttype.id != righttype.id)
                return 0;
        if (lefttype.id != VAL_STRING && lefttype.id != VAL_INTEGER)
                return 0;
        return 1;
}

int
compare_values(struct value val0, struct value val1)
{
        switch (val0.type.id) {
                case VAL_STRING:
                        return memcmp(val0.as.string.str, val1.as.string.str, val0.as.string.length);
                case VAL_INTEGER:
                        return val0.as.integer - val1.as.integer;
                default:
                        printf("unreachable code at compare_values (val0 type %d)", val0.type.id);
                        return 0;
        }
}

int
semantic_type_equal(struct semantic_type type0, struct semantic_type type1)
{
        return type0.id == type1.id && type0.base == type1.base && type0.rank == type1.rank && memcmp(type0.dimensions, type1.dimensions, sizeof(int) * type0.rank) == 0;
}

char *
value_type_to_string(enum value_type vt)
{
        switch (vt) {
                case VAL_STRING: return "VAL_STRING";
                case VAL_BOOLEAN: return "VAL_BOOLEAN";
                case VAL_INTEGER: return "VAL_INTEGER";
                case VAL_VECTOR: return "VAL_VECTOR";
        }
        return "unreachable code in value_type_to_string";
}

void
semantic_type_print(struct semantic_type type)
{
        printf("%s", value_type_to_string(type.id));
        switch (type.id) {
                case VAL_VECTOR: {
                        printf(" ");
                        for (int i = 0; i < type.rank; i++) {
                                printf("%d ", type.dimensions[i]);
                        }
                        printf("of ");
                        semantic_type_print(semantic_type_scalar(type.base));
                        return;
                }
                default:
                        break;
        }
}

void
run_type_print(struct run_type type)
{
        printf("%s", value_type_to_string(type.id));
}

struct run_type
semantic_type_to_run_type(struct semantic_type st)
{
        struct run_type rt;
        rt.id = st.id;
        switch (st.id) {
                case VAL_VECTOR:
                        rt.rank = st.rank;
                        rt.size = st.size;
                        break;
                default:
                        break;
        }
        return rt;
}

int
index_flattened(int *dimensions, int *indices, int length)
{
        int res = 0;
        for (int i = 0; i < length; i++) {
                int term = indices[i];
                for (int j = i + 1; j < length; j++) {
                        term *= dimensions[j];
                }
                res += term;
        }
        return res;
}

struct value
vector_value_get_element_at(struct value vec, int i)
{
        return *(vec.as.vector.astackent - vec.type.size + i);
}

void
vector_value_set_element_at(struct value vec, int i, struct value val)
{
        *(vec.as.vector.astackent - vec.type.size + i) = val;
}

uint8_t
left_byte(uint16_t word)
{
        return word >> 8;
}

uint8_t
right_byte(uint16_t word)
{
        return word & 0xff;
}

uint16_t
join_bytes(uint8_t left, uint8_t right)
{
        uint16_t res = left;
        res = ((uint16_t) res) << 8;
        res = res | right;
        return res;
}
