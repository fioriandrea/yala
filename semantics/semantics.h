#ifndef semantics_h
#define semantics_h

#include <stdint.h>

#include "../frontend/frontend.h"

#define MAX_JUMP UINT8_MAX
#define MAX_SKIP_LONG UINT16_MAX
#define MAX_CONDITIONAL_LEN 400
#define MAX_ARITY UINT8_MAX

enum opcode {
        OP_LOCI, /* constants */
        OP_LOCS,

        OP_ADDI, /* integer arithmetic */
        OP_SUBI,
        OP_MULI,
        OP_DIVI,

        OP_IGRT, /* comparison */
        OP_IGRTEQ,
        OP_ILT,
        OP_ILEQ,
        OP_EQUA,

        OP_NOT, /* boolean logic */

        OP_SKIP_LONG, /* jumps */
        OP_SKIPF_LONG,
        OP_SKIP_BACK_LONG,

        OP_ZERO, /* constants */
        OP_ONE,

        OP_POPV, /* value stack manipulation */

        OP_GET_LOCAL_LONG,
        OP_SET_LOCAL_LONG,

        OP_WRITE,
        OP_NEWLINE,

        OP_HALT,
};

char *opcodestring(enum opcode code);

enum value_type {
        VAL_INTEGER,
        VAL_BOOLEAN,
        VAL_STRING,
};

struct value_string {
        char *str;
        int length;
};

struct value {
        enum value_type type;
        union {
                int integer;
                int boolean;
                struct value_string string;
        } as;
};

void value_print(struct value v);
struct value value_from_c_int(int i);
struct value value_from_c_bool(int b);
// TODO value_from_c_string
int values_equal(struct value val0, struct value val1);

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
LIST_DECLARE(valuelist, struct value)

struct bytecode {
        struct bytes code;
        struct linelist lines;
        struct valuelist constants;
};

void bytecode_init(struct bytecode *code);
int bytecode_write_byte(struct bytecode *code, uint8_t byte, struct lineinfo linfo);
int bytecode_write_constant(struct bytecode *code, struct value val, struct lineinfo linfo);
uint8_t bytecode_byte_at(struct bytecode *code, int i);
struct lineinfo bytecode_lineinfo_at(struct bytecode *code, int i);
struct value bytecode_constant_at(struct bytecode *code, uint8_t address);
void bytecode_free(struct bytecode *code);
void disassemble(struct bytecode *code);

struct bytecode *generate_bytecode(struct tree_node *parsetree);

enum type_type {
        TYPE_INTEGER,
        TYPE_BOOLEAN,
        TYPE_STRING,
};

struct type {
        enum type_type type;
        union {
        } additional;
};

#define MAX_LOCALS 200

struct local {
        struct token name;
        struct type type;
        int depth;
};

struct environment {
        struct bytecode *code;
        struct local locals[MAX_LOCALS];
        int depth;
        int count;
        int error;
};

void emit_statement(struct environment *env, struct tree_node *root);
struct type emit_expression(struct environment *env, struct tree_node *root);

#endif