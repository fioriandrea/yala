#ifndef semantics_h
#define semantics_h

#include <stdint.h>

#include "../frontend/frontend.h"

#define MAX_JUMP UINT8_MAX
#define MAX_SKIP_LONG UINT16_MAX
#define MAX_CONDITIONAL_LEN 400
#define MAX_ARITY UINT8_MAX

enum opcode {
        OP_LOC_LONG, /* constants */
        OP_PUSH_BYTE,

        OP_ADDI, /* integer arithmetic */
        OP_SUBI,
        OP_MULI,
        OP_DIVI,

        OP_GRT, /* comparison */
        OP_GRTEQ,
        OP_LT,
        OP_LEQ,
        OP_EQUA,

        OP_NOT, /* boolean logic */

        OP_SKIP_LONG, /* jumps */
        OP_SKIPF_LONG,
        OP_SKIP_BACK_LONG,

        OP_ZERO, /* constants */
        OP_ONE,
        OP_FALSE,
        OP_EMPTY_STRING,

        OP_POPV, /* value stack manipulation */

        OP_GET_LOCAL_LONG,
        OP_SET_LOCAL_LONG,

        OP_WRITE,
        OP_NEWLINE,

        OP_POP_TO_ASTACK, /* array stack manipulation */
        OP_POPA,
        OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG,

        OP_GET_INDEX,
        OP_SET_INDEXED_LOCAL_LONG,

        OP_READ,

        OP_HALT,
};

char *opcodestring(enum opcode code);

enum value_type {
        VAL_INTEGER,
        VAL_BOOLEAN,
        VAL_STRING,
        VAL_VECTOR,
};

#define MAX_VECTOR_DIMENSIONS 50

struct semantic_type {
        enum value_type id;
        enum value_type base;
        int dimensions[MAX_VECTOR_DIMENSIONS];
        int rank;
        int size;
};

struct value_string {
        char *str;
        int length;
        unsigned long hash;
};

struct value_vector {
        union value *astackent;
        int size;
};

union value {
        int integer;
        int boolean;
        struct value_string string;
        struct value_vector vector;
};

struct run_type run_type_scalar(enum value_type id);
void value_print(union value v, enum value_type type, enum value_type base);
union value value_from_c_int(int i);
union value value_from_c_bool(int b);
struct value_string copy_string(char *str, int length);
unsigned long hash_string(char *str, int length);
union value value_from_token(struct token token);
union value value_from_c_string(char *str);
int values_equal(union value val0, union value val1, enum value_type type, enum value_type base);
int semantic_types_comparable(struct semantic_type lefttype, struct semantic_type righttype);
int compare_values(union value val0, union value val1, enum value_type type);
int semantic_type_equal(struct semantic_type type0, struct semantic_type type1);
struct run_type semantic_type_to_run_type(struct semantic_type st);
struct semantic_type semantic_type_scalar(enum value_type vt);
void semantic_type_print(struct semantic_type semantic_type);
void run_type_print(struct run_type type);
char * value_type_to_string(enum value_type vt);
int index_flattened(int *dimensions, int *indices, int length);
union value vector_value_get_element_at(union value vec, int i);
void vector_value_set_element_at(union value vec, int i, union value val);
uint8_t left_byte(uint16_t word);
uint8_t right_byte(uint16_t word);
uint16_t join_bytes(uint8_t left, uint8_t right);

struct lineinfo {
        int line;
        int linepos;
};

#define MAX_CONSTANTS UINT8_MAX

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

LIST_DECLARE(bytes, uint8_t)
LIST_DECLARE(linelist, struct lineinfo)
LIST_DECLARE(valuelist, union value)

struct bytecode {
        struct bytes code;
        struct linelist lines;
        struct valuelist constants;
};

void bytecode_init(struct bytecode *code);
int bytecode_write_byte(struct bytecode *code, uint8_t byte, struct lineinfo linfo);
int bytecode_write_long(struct bytecode *code, uint16_t l, struct lineinfo linfo);
int bytecode_write_constant(struct bytecode *code, union value val, struct lineinfo linfo);
uint8_t bytecode_byte_at(struct bytecode *code, int i);
struct lineinfo bytecode_lineinfo_at(struct bytecode *code, int i);
union value bytecode_constant_at(struct bytecode *code, uint16_t address);
void bytecode_free(struct bytecode *code);
void disassemble(struct bytecode *code);

struct bytecode *generate_bytecode(struct tree_node *parsetree);

#define MAX_LOCALS 200

#define LOCAL_PERM_R (1 << 0)
#define LOCAL_PERM_W (1 << 1)
#define LOCAL_PERM_RW (LOCAL_PERM_R | LOCAL_PERM_W)

struct local {
        struct token name;
        struct semantic_type type;
        int depth;
        uint8_t perms;
};

struct environment {
        struct bytecode *code;
        struct local locals[MAX_LOCALS];
        int depth;
        int count;
        int error;
        int panic;
};

void emit_statement(struct environment *env, struct tree_node *root);
struct semantic_type emit_expression(struct environment *env, struct tree_node *root);

#endif