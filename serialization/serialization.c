#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "serialization.h"

static void serialize_code(struct bytecode *code, FILE *outfile);
static void serialize_loc(struct bytecode *code, FILE *outfile, enum opcode op, uint16_t address);
static uint16_t read_address(struct bytecode *code, int *ip);
static void serialize_constants(struct bytecode *code, FILE *outfile);

#define END_FUNCTION_DELIM (-1)

void
serialize_bytecode(struct bytecode *code, FILE *outfile)
{
        serialize_code(code, outfile);
        serialize_constants(code, outfile);
        fprintf(outfile, "%d\n", END_FUNCTION_DELIM);
}

static void
serialize_code(struct bytecode *code, FILE *outfile)
{
        for (int ip = 0; ip < LIST_LEN(&code->code); ip++) {
                uint8_t byte = LIST_AT(&code->code, ip);
                struct lineinfo linfo = LIST_AT(&code->lines, ip);
                fprintf(outfile, "%s%d(%d:%d)", ip == 0 ? "" : " ", byte, linfo.line, linfo.linepos);
        }
        fprintf(outfile, "\n");
}

static void
serialize_loc(struct bytecode *code, FILE *outfile, enum opcode op, uint16_t address)
{
        union value val = LIST_AT(&code->constants, address);
        switch (op) {
        case OP_LOCI_LONG:
                fprintf(outfile, "%d %d", VAL_INTEGER, val.integer);
                fprintf(outfile, "\n");
                break;
        case OP_LOCS_LONG:
                fprintf(outfile, "%d %.*s", VAL_STRING, val.string.length, val.string.str);
                {char n = 0x00; fwrite(&n, sizeof(char), 1, outfile);}
                fprintf(outfile, "\n");
                break;
        case OP_LOC_ALINK_LONG:
                fprintf(outfile, "%d %d", VAL_VECTOR, val.vector.size);
                fprintf(outfile, "\n");
                break;
        case OP_LOCF_LONG:
                fprintf(outfile, "%d ", VAL_FUNCTION);
                serialize_bytecode(val.function.code, outfile);
                break;
        default:
                exit(100);
                break;
        }
}

static uint16_t
read_address(struct bytecode *code, int *ip)
{
        uint8_t left = LIST_AT(&code->code, *ip);
        (*ip)++;
        uint8_t right = LIST_AT(&code->code, *ip);
        (*ip)++;
        return join_bytes(left, right);
}

static void
serialize_constants(struct bytecode *code, FILE *outfile)
{
        for (int ip = 0; ip < LIST_LEN(&code->code); ) {
                enum opcode op = LIST_AT(&code->code, ip);
                ip++;
                switch (op) {
                case OP_LOCI_LONG:
                case OP_LOCS_LONG:
                case OP_LOCF_LONG:
                case OP_LOC_ALINK_LONG:
                        serialize_loc(code, outfile, op, read_address(code, &ip));
                        break;
                case OP_SKIP_BACK_LONG:
                case OP_SKIP_LONG:
                case OP_SKIPF_LONG:
                case OP_GET_INDEX:
                case OP_EQUA:
                case OP_ARGSTACK_LOAD:
                        ip += 2;
                        break;
                case OP_GET_LOCAL_LONG:
                case OP_SET_LOCAL_LONG:
                        ip += 4;
                        break;
                case OP_LT:
                case OP_LEQ:
                case OP_GRT:
                case OP_GRTEQ:
                case OP_PUSH_BYTE:
                case OP_WRITE:
                case OP_CALL:
                case OP_RETURN:
                case OP_READ:
                case OP_ARGSTACK_UNLOAD:
                        ip += 1;
                        break;
                case OP_SET_INDEX_LOCAL_LONG:
                        ip += 6;
                        break;
                default:
                        break;
                }
        }
}

static char *deserialize_constants(struct bytecode *code, char *p);
static char *deserialize_code(struct bytecode *code, char *p);
static char *read_integer(char *p, int *i);
static char *skip_spaces(char *p);
static void link_panic(char *fmt, ...);

char *
deserialize_bytecode(struct bytecode *code, char *p)
{
        bytecode_init(code);
        p = deserialize_code(code, p);
        p = deserialize_constants(code, p);
        return p;
}

static char *
deserialize_code(struct bytecode *code, char *p)
{
        do {
                int byte = 0;
                p = read_integer(p, &byte);
                struct lineinfo linfo;
                if (*p++ != '(') {
                        link_panic("expected (");
                }
                p = read_integer(p, &linfo.line);
                if (*p++ != ':') {
                        link_panic("expected :");
                }
                p = read_integer(p, &linfo.linepos);
                if (*p++ != ')') {
                        link_panic("expected )");
                }
                bytecode_write_byte(code, byte, linfo);
                p = skip_spaces(p);
        } while (*p >= '0' && *p <= '9');
        if (*p++ != '\n') {
                link_panic("expected new line");
        }
        return p;
}

static char *
deserialize_constants(struct bytecode *code, char *p)
{
        union value val;
        for (;;)
        {
                int type;
                p = read_integer(p, &type);
                p = skip_spaces(p);
                switch (type) {
                case VAL_INTEGER:
                        p = read_integer(p, &val.integer);
                        break;
                case VAL_BOOLEAN:
                        val.boolean = *p++ - '0';
                        break;
                case VAL_VOID:
                        val.integer = 0;
                        break;
                case VAL_STRING: {
                        int cap = 8;
                        char *buffer = malloc(sizeof(char) * cap);
                        int len = 0;
                        while (*p != '\0') {
                                buffer[len++] = *p++;
                                if (len == cap - 1) {
                                        cap = cap << 1;
                                        buffer = realloc(buffer, sizeof(char) * cap);
                                }
                        }
                        buffer[len] = '\0';
                        val.string.str = buffer;
                        val.string.length = len;
                        val.string.hash = hash_string(buffer, len);
                        p++; /* skip \0 */
                        break;
                }
                case VAL_VECTOR:
                        p = read_integer(p, &val.vector.size);
                        break;
                case VAL_FUNCTION: {
                        struct bytecode *subcode = malloc(sizeof(struct bytecode));
                        p = deserialize_bytecode(subcode, p);
                        val.function.code = subcode;
                        break;
                }
                case END_FUNCTION_DELIM:
                        goto end;
                default:
                        link_panic("unknown constant type");
                }
                if (*p++ != '\n') {
                        link_panic("expected new line");
                }
                valuelist_push(&code->constants, val);
        }
end:
        return p;
}

static char *
read_integer(char *p, int *i)
{
        int sign = 1;
        if (*p == '-') {
                sign *= -1;
                p++;
        }
        *i = 0;
        while (*p >= '0' && *p <= '9') {
                *i = (*p - '0') + *i * 10;
                p++;
        }
        *i *= sign;
        return p;
}

static char *
skip_spaces(char *p)
{
        while (*p == ' ' || *p == '\t')
                p++;
        return p;
}

static void
link_panic(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "linkage error: ");
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(1);
}
