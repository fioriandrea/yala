#include <stdio.h>

#include "semantics.h"

char *
opcodestring(enum opcode code)
{
        switch (code) {
        case OP_LOCI: return "OP_LOCI";
        case OP_LOCS: return "OP_LOCS";
        case OP_ADDI: return "OP_ADDI";
        case OP_SUBI: return "OP_SUBI";
        case OP_MULI: return "OP_MULI";
        case OP_DIVI: return "OP_DIVI";
        case OP_IGRT: return "OP_IGRT";
        case OP_ILET: return "OP_ILET";
        case OP_NEQUA: return "OP_NEQUA";
        case OP_EQUA: return "OP_EQUA";
        case OP_AND: return "OP_AND";
        case OP_OR: return "OP_OR";
        case OP_NOT: return "OP_NOT";
        }
        return "unreachable return in opcodestring";
}

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
        int addr = valuelist_push(&code->constants, val);
        return bytecode_write_byte(code, addr, linfo);
}

void
bytecode_free(struct bytecode *code)
{
        bytes_free(&code->code);
        linelist_free(&code->lines);
        valuelist_free(&code->constants);
}

static void
disassemble_lineinfo(struct bytecode *code, int ip)
{
        printf("(%d:%d)", linelist_at(&code->lines, ip).line, linelist_at(&code->lines, ip).linepos);
}

static int
disassemble_constant(struct bytecode *code, int ip)
{
        uint8_t constantaddr = bytes_at(&code->code, ip);
        struct value val = valuelist_at(&code->constants, constantaddr);
        printf("TODO_CONSTANT");
        disassemble_lineinfo(code, ip);
        return ip + 1;
}

void
disassemble(struct bytecode *code)
{
        int ip = 0;
        while (ip < bytes_len(&code->code)) {
                uint8_t instruction = bytes_at(&code->code, ip);
                printf("%s", opcodestring(instruction));
                disassemble_lineinfo(code, ip);
                printf(" ");
                ip++;
                switch (instruction) {
                case OP_LOCI:
                case OP_LOCS:
                ip = disassemble_constant(code, ip);
                break;
                default:
                break;
                }
                printf("\n");
        }
}