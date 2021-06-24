#ifndef semantics_h
#define semantics_h

#include <stdint.h>

#include "../datastructs/datastructs.h"
#include "../frontend/frontend.h"

enum opcode {
        OP_LOCI, /* constants */
        OP_LOCS,

        OP_ADDI, /* integer arithmetic */
        OP_SUBI,
        OP_MULI,
        OP_DIVI,

        OP_IGRT, /* comparison */
        OP_ILET,
        OP_NEQUA,
        OP_EQUA,

        OP_AND, /* boolean logic */
        OP_OR,
        OP_NOT,

        OP_HALT,
};

char *opcodestring(enum opcode code);

enum value_type {
        VAL_INTEGER,
        VAL_BOOLEAN,
};

struct value {
        enum value_type type;
        union {
                int integer;
                int boolean;
        } as;
};

void print_value(struct value v);
struct value value_from_c_int(int i);
struct value value_from_c_bool(int b);

struct lineinfo {
        int line;
        int linepos;
};

LIST_DECLARE(bytes, uint8_t)
LIST_DECLARE(linelist, struct lineinfo)
LIST_DECLARE(valuelist, struct value)

#define MAX_CONSTANTS UINT8_MAX

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

#endif