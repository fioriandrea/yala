#include <stdlib.h>

#include "serialization.h"

static void serialize_code(struct bytecode *code, FILE *outfile);
static void serialize_loc(struct bytecode *code, FILE *outfile, enum opcode op, uint16_t address);
static uint16_t read_address(struct bytecode *code, int *ip);
static void serialize_constants(struct bytecode *code, FILE *outfile);

void
serialize_bytecode(struct bytecode *code, FILE *outfile)
{
        serialize_code(code, outfile);
        serialize_constants(code, outfile);
}

static void
serialize_code(struct bytecode *code, FILE *outfile)
{
        for (int ip = 0; ip < bytes_len(&code->code); ip++) {
                uint8_t byte = bytes_at(&code->code, ip);
                struct lineinfo linfo = linelist_at(&code->lines, ip);
                fprintf(outfile, "%s%d(%d:%d)", ip == 0 ? "" : " ", byte, linfo.line, linfo.linepos);
        }
        fprintf(outfile, "\n");
}

static void
serialize_loc(struct bytecode *code, FILE *outfile, enum opcode op, uint16_t address)
{
        union value val = valuelist_at(&code->constants, address);
        switch (op) {
        case OP_LOCI_LONG:
                fprintf(outfile, "CI %d", val.integer);
                break;
        case OP_LOCB_LONG:
                fprintf(outfile, "CB %d", val.boolean);
                break;
        case OP_LOCS_LONG:
                fprintf(outfile, "CS %.*s", val.string.length, val.string.str);
                {char n = 0x00; fwrite(&n, sizeof(char), 1, outfile);}
                break;
        case OP_LOC_ALINK_LONG:
                fprintf(outfile, "CV %d", val.vector.size);
                break;
        case OP_LOCVO_LONG:
                fprintf(outfile, "CVO");
                break;
        case OP_LOCF_LONG:
                fprintf(outfile, "CF ");
                serialize_bytecode(val.function.code, outfile);
                fprintf(outfile, "ENDCF");
                break;
        default:
                exit(100);
                break;
        }
        fprintf(outfile, "\n");
}

static uint16_t
read_address(struct bytecode *code, int *ip)
{
        uint8_t left = bytes_at(&code->code, *ip);
        (*ip)++;
        uint8_t right = bytes_at(&code->code, *ip);
        (*ip)++;
        return join_bytes(left, right);
}

static void
serialize_constants(struct bytecode *code, FILE *outfile)
{
        for (int ip = 0; ip < bytes_len(&code->code); ) {
                uint8_t op = bytes_at(&code->code, ip);
                ip++;
                switch (op) {
                case OP_LOCI_LONG:
                case OP_LOCB_LONG:
                case OP_LOCS_LONG:
                case OP_LOCVO_LONG:
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
                }
        }
}