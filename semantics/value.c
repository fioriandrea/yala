#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./semantics.h"

/* list */

#define LIST_DEFINE(name, type) \
        LIST_INIT(name, type) \
        LIST_PUSH(name, type) \
        LIST_POP(name, type) \
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
LIST_DEFINE(valuelist, union value)
LIST_DEFINE(locals, struct local)
LIST_DEFINE(break_likes, struct break_like)
LIST_DEFINE(arg_types, struct semantic_type)
LIST_DEFINE(intlist, int)

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
bytecode_write_constant(struct bytecode *code, union value val, struct lineinfo linfo)
{
        int addr = valuelist_push(&code->constants, val) - 1;
        return bytecode_write_long(code, addr, linfo);
}

uint8_t
bytecode_byte_at(struct bytecode *code, int i)
{
        return LIST_AT(&code->code, i);
}

struct lineinfo
bytecode_lineinfo_at(struct bytecode *code, int i)
{
        return LIST_AT(&code->lines, i);
}

union value
bytecode_constant_at(struct bytecode *code, uint16_t address)
{
        return LIST_AT(&code->constants, address);
}

void
value_print(union value v, enum value_type type, enum value_type base)
{
        switch (type) {
        case VAL_INTEGER:
                printf("%d", v.integer);
                return;
        case VAL_BOOLEAN:
                printf("%s", v.boolean ? "true" : "false");
                return;
        case VAL_STRING:
                printf("%.*s", v.string.length, v.string.str);
                return;
        case VAL_VECTOR:
                printf("[");
                for (int i = 0; i < v.vector.size; i++) {
                        value_print(v.vector.astackent[i], base, base);
                        printf(i == v.vector.size - 1 ? "" : ", ");
                }
                printf("]");
                return;
        case VAL_FUNCTION:
                printf("(");
                disassemble_helper(v.function.code, 1);
                printf(")");
                return;
        default:
                printf("unreachable value type %d in value_print", type);
                exit(100);
        }
}

union value
value_from_c_int(int i)
{
        union value v;
        v.integer = i;
        return v;
}

union value
value_from_c_bool(int b)
{
        union value v;
        v.boolean = !!b;
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

union value
value_from_token(struct token token)
{
        union value v;
        v.string = value_string_from_token(token);
        return v;
}

union value
value_from_c_string(char *str)
{
        union value v;
        v.string = copy_string(str, strlen(str));
        return v;
}

struct semantic_type
semantic_type_return_value(struct semantic_type type)
{
        return LIST_AT(type.arg_types, type.ret_type_index);
}

struct semantic_type
semantic_type_argument_at(struct semantic_type type, int i)
{
        return LIST_AT(type.arg_types, type.param_types_start_index + i);
}

int
semantic_type_dimension_at(struct semantic_type type, int i)
{
        return LIST_AT(type.dimensions, i + type.dimensions_start_index);
}

struct semantic_type
semantic_type_scalar(enum value_type vt)
{

        struct semantic_type type;
        type.id = vt;
        type.base = vt;
        type.rank = 0;
        type.size = 1;
        type.param_types_start_index = 0;
        type.ret_type_index = -1;
        type.arg_types = NULL;
        type.dimensions = NULL;
        type.dimensions_start_index = -1;
        type.modifier = ARG_MOD_IN;
        return type;
}

int
values_equal(union value val0, union value val1, enum value_type type, enum value_type base)
{
        switch (type) {
        case VAL_INTEGER:
                return val0.integer == val1.integer;
        case VAL_BOOLEAN:
                return val0.boolean == val1.boolean;
        case VAL_STRING:
                if (val0.string.hash != val1.string.hash)
                        return 0;
                return val0.string.length == val1.string.length && memcmp(val0.string.str, val1.string.str, val0.string.length) == 0;
        case VAL_VECTOR:
                for (int i = 0; i < val0.vector.size; i++) {
                        union value ent0 = val0.vector.astackent[i];
                        union value ent1 = val1.vector.astackent[i];
                        if (!values_equal(ent0, ent1, base, base))
                                return 0;
                }
                return 1;
        case VAL_FUNCTION:
                return 0;
        default:
                exit(100);
        }
        return 0;
}

int
semantic_types_comparable(struct semantic_type lefttype, struct semantic_type righttype)
{
        if (lefttype.id != righttype.id)
                return 0;
        if (lefttype.id != VAL_STRING && lefttype.id != VAL_INTEGER)
                return 0;
        return 1;
}

int
compare_values(union value val0, union value val1, enum value_type type)
{
        switch (type) {
                case VAL_STRING:
                        return memcmp(val0.string.str, val1.string.str, val0.string.length);
                case VAL_INTEGER:
                        return val0.integer - val1.integer;
                default:
                        exit(100);
                        return 0;
        }
}

int
semantic_type_equal(struct semantic_type type0, struct semantic_type type1)
{
        if (type0.id != type1.id)
                return 0;
        if (type0.id == VAL_VOID || (type0.id != VAL_FUNCTION && type0.id != VAL_VECTOR))
                return 1;
        if (type0.id == VAL_VECTOR)
                return type0.base == type1.base && type0.rank == type1.rank && memcmp(type0.dimensions->buffer + type0.dimensions_start_index, type1.dimensions->buffer + type1.dimensions_start_index, sizeof(int) * type0.rank) == 0;
        /* functions */ 
        if (type0.rank != type1.rank)
                return 0;
        for (int i = 0; i < type0.rank; i++) {
                if (!semantic_type_equal(semantic_type_argument_at(type0, i), semantic_type_argument_at(type1, i)))
                        return 0;
        }
        return 1;
}

char *
value_type_to_string(enum value_type vt)
{
        switch (vt) {
                case VAL_BOOLEAN: return "VAL_BOOLEAN";
                case VAL_FUNCTION: return "VAL_FUNCTION";
                case VAL_INTEGER: return "VAL_INTEGER";
                case VAL_STRING: return "VAL_STRING";
                case VAL_VECTOR: return "VAL_VECTOR";
                case VAL_VOID: return "VAL_VOID";
        }
        exit(100);
        return "";
}

void
semantic_type_print(struct semantic_type type)
{
        printf("%s", value_type_to_string(type.id));
        switch (type.id) {
                case VAL_VECTOR: {
                        printf(" ");
                        for (int i = 0; i < type.rank; i++) {
                                printf("%d ", semantic_type_dimension_at(type, i));
                        }
                        printf("of ");
                        semantic_type_print(semantic_type_scalar(type.base));
                        return;
                }
                case VAL_FUNCTION: {
                        printf("(");
                        for (int i = 0; i < type.rank; i++) {
                                printf("%s", i == 0 ? "" : ", ");
                                semantic_type_print(semantic_type_argument_at(type, i));
                        }
                        printf("): ");
                        if (type.ret_type_index >= 0)
                                semantic_type_print(semantic_type_return_value(type));
                        else
                                printf("void");
                }
                default:
                        break;
        }
}

struct semantic_type
semantic_type_void()
{
        return semantic_type_scalar(VAL_VOID);
}

union value
value_void()
{
        return value_from_c_int(0);
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

int
is_add_overflow(int a, int x)
{
        return (((x > 0) && (a > INT_MAX - x)) || ((x < 0) && (a < INT_MIN - x)));
}

int
is_mult_overflow(int a, int x)
{
        return (((a == -1) && (x == INT_MIN)) || ((x == -1) && (a == INT_MIN)) || (a > INT_MAX / x) || (a < INT_MIN / x));
}