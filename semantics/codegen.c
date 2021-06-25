#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "semantics.h"

static void emit_code(struct bytecode *code, struct tree_node *root);
static void emit_byte(struct bytecode *code, struct tree_node *root, uint8_t byte);
static void emit_two_bytes(struct bytecode *code, struct tree_node *root, uint8_t byte0, uint8_t byte1);
static void emit_constant(struct bytecode *code, struct tree_node *root, struct value val);
static void codegen_error(struct bytecode *code, struct tree_node *root, char *fmt, ...);
static int parse_boolean_token(struct token token);
static int parse_integer_token(struct token token);

struct bytecode *
generate_bytecode(struct tree_node *parsetree)
{
        struct bytecode *code = malloc(sizeof(struct bytecode));
        bytecode_init(code);
        emit_code(code, parsetree);
        if (code->error_detected) {
                bytecode_free(code);
                return NULL;
        }
        emit_byte(code, parsetree, OP_HALT);
        return code;
}

static void
emit_code(struct bytecode *code, struct tree_node *root)
{
        int codelen, jumplen;
        switch (root->type) {
        case NODE_AND_EXPR:
                emit_code(code, root->left);
                emit_two_bytes(code, root, OP_SKIPF, 0);
                codelen = bytes_len(&code->code);
                emit_byte(code, root, OP_POPV);
                emit_code(code, root->right);
                jumplen = bytes_len(&code->code) - codelen;
                if (jumplen > MAX_JUMP) {
                        codegen_error(code, root, "max jump size (%d) exceeded", MAX_JUMP);
                        return;
                }
                code->code.buffer[codelen - 1] = jumplen;
                break;
        case NODE_OR_EXPR:
                emit_code(code, root->left);
                emit_two_bytes(code, root, OP_SKIPF, 2);
                emit_two_bytes(code, root, OP_SKIP, 0);
                codelen = bytes_len(&code->code);
                emit_byte(code, root, OP_POPV);
                emit_code(code, root->right);
                jumplen = bytes_len(&code->code) - codelen;
                if (jumplen > MAX_JUMP) {
                        codegen_error(code, root, "max jump size (%d) exceeded", MAX_JUMP);
                        return;
                }
                code->code.buffer[codelen - 1] = jumplen;
                break;
        case NODE_NOT_EXPR:
                emit_code(code, root->child);
                emit_byte(code, root, OP_NOT);
                break;
        case NODE_PLUS_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_ADDI);
                break;
        case NODE_MINUS_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_SUBI);
                break;
        case NODE_TIMES_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_MULI);
                break;
        case NODE_DIVIDE_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_DIVI);
                break;
        case NODE_NEG_EXPR:
                emit_byte(code, root, OP_ZERO);
                emit_code(code, root->child);
                emit_byte(code, root, OP_SUBI);
                break;
        case NODE_EQ_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_EQUA);
                break;
        case NODE_GREATEREQ_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_IGRTEQ);
                break;
        case NODE_GREATER_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_IGRT);
                break;
        case NODE_LESSEQ_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_ILEQ);
                break;
        case NODE_LESS_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_ILT);
                break;
        case NODE_NEQ_EXPR:
                emit_code(code, root->left);
                emit_code(code, root->right);
                emit_byte(code, root, OP_EQUA);
                emit_byte(code, root, OP_NOT);
                break;
        case NODE_BOOLEAN_CONST:
                emit_byte(code, root, OP_LOCI);
                emit_constant(code, root, value_from_c_bool(parse_boolean_token(root->value)));
                break;
        case NODE_INTGER_CONST:
                emit_byte(code, root, OP_LOCI);
                emit_constant(code, root, value_from_c_int(parse_integer_token(root->value)));
                break;
        default:
                codegen_error(code, root, "code generation for node not implemented (%s)", nodetypestring(root->type));
        }
}

static void
emit_byte(struct bytecode *code, struct tree_node *root, uint8_t byte)
{
        struct lineinfo linfo;
        linfo.line = root->value.line;
        linfo.linepos = root->value.linepos;
        bytecode_write_byte(code, byte, linfo);
}

static void
emit_two_bytes(struct bytecode *code, struct tree_node *root, uint8_t byte0, uint8_t byte1)
{
        emit_byte(code, root, byte0);
        emit_byte(code, root, byte1);
}

static void
emit_constant(struct bytecode *code, struct tree_node *root, struct value val)
{
        struct lineinfo linfo;
        linfo.line = root->value.line;
        linfo.linepos = root->value.linepos;
        bytecode_write_constant(code, val, linfo);
}

static void
codegen_error(struct bytecode *code, struct tree_node *root, char *fmt, ...)
{
        code->error_detected = 1;
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "code generation error ");
        fprintf(stderr, "[at %d:%d]: ", root->value.line, root->value.linepos);
        fprintf(stderr, "at '%.*s', ", root->value.length, root->value.start);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}

static int
parse_boolean_token(struct token token)
{
        return *token.start == 't';
}
static int
parse_integer_token(struct token token)
{
        int res = 0;
        char *ptr = token.start;
        while (ptr - token.start < token.length) {
                res = res * 10 + *ptr - '0';
                ptr++;
        }
        return res;
}

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
        case OP_IGRTEQ: return "OP_IGRTEQ";
        case OP_ILT: return "OP_ILT";
        case OP_ILEQ: return "OP_ILEQ";
        case OP_EQUA: return "OP_EQUA";
        case OP_NOT: return "OP_NOT";
        case OP_SKIP: return "OP_SKIP";
        case OP_SKIPF: return "OP_SKIPF";
        case OP_ZERO: return "OP_ZERO";
        case OP_ONE: return "OP_ONE";
        case OP_POPV: return "OP_POPV";
        case OP_HALT: return "OP_HALT";
        }
        return "unreachable return in opcodestring";
}

void
bytecode_init(struct bytecode *code)
{
        code->error_detected = 0;
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
bytecode_free(struct bytecode *code)
{
        if (!code)
                return;
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
        print_value(val);
        printf(" ");
        return ip + 1;
}

static int
disassemble_argument(struct bytecode *code, int ip)
{
        uint8_t arg = bytes_at(&code->code, ip);
        printf("%d ", arg);
        return ip + 1;
}

void
disassemble(struct bytecode *code)
{
        int ip = 0;
        while (code && ip < bytes_len(&code->code)) {
                uint8_t instruction = bytes_at(&code->code, ip);
                printf("%s ", opcodestring(instruction));
                ip++;
                switch (instruction) {
                case OP_LOCI:
                case OP_LOCS:
                        ip = disassemble_constant(code, ip);
                        break;
                case OP_SKIP:
                case OP_SKIPF:
                        ip = disassemble_argument(code, ip);
                        break;
                default:
                        break;
                }
                disassemble_lineinfo(code, ip);
                printf("\n");
        }
}
