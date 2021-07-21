#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "semantics.h"

static void
semantics_error(struct environment *env, struct tree_node *root, char *fmt, ...)
{
        env->error = 1;
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "semantic error ");
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
        case OP_SKIP_LONG: return "OP_SKIP_LONG";
        case OP_SKIPF_LONG: return "OP_SKIPF_LONG";
        case OP_ZERO: return "OP_ZERO";
        case OP_ONE: return "OP_ONE";
        case OP_POPV: return "OP_POPV";
        case OP_WRITE: return "OP_WRITE";
        case OP_NEWLINE: return "OP_NEWLINE";
        case OP_SET_LOCAL_LONG: return "OP_SET_LOCAL_LONG";
        case OP_GET_LOCAL_LONG: return "OP_GET_LOCAL_LONG";
        case OP_HALT: return "OP_HALT";
        }
        return "unreachable return in opcodestring";
}

static void
disassemble_lineinfo(struct bytecode *code, int ip)
{
        printf("[%d:%d]", linelist_at(&code->lines, ip - 1).line, linelist_at(&code->lines, ip - 1).linepos);
}

static int
disassemble_constant(struct bytecode *code, int ip)
{
        uint8_t constantaddr = bytes_at(&code->code, ip++);
        struct value val = valuelist_at(&code->constants, constantaddr);
        value_print(val);
        printf(" ");
        return ip;
}

static int
disassemble_argument(struct bytecode *code, int ip)
{
        uint8_t arg = bytes_at(&code->code, ip++);
        printf("%d ", arg);
        return ip;
}

static int
disassemble_argument_long(struct bytecode *code, int ip)
{
        uint8_t arg0 = bytes_at(&code->code, ip++);
        uint8_t arg1 = bytes_at(&code->code, ip++);
        printf("%d ", ((unsigned) arg0 << 8) | arg1);
        return ip;
}

void
disassemble(struct bytecode *code)
{
        int ip = 0;
        while (code && ip < bytes_len(&code->code)) {
                uint8_t instruction = bytes_at(&code->code, ip);
                printf("%d: %s ", ip, opcodestring(instruction));
                ip++;
                switch (instruction) {
                case OP_LOCI:
                case OP_LOCS:
                        ip = disassemble_constant(code, ip);
                        break;
                case OP_SKIP_LONG:
                case OP_SKIPF_LONG:
                case OP_GET_LOCAL_LONG:
                case OP_SET_LOCAL_LONG:
                        ip = disassemble_argument_long(code, ip);
                        break;
                case OP_WRITE:
                        ip = disassemble_argument(code, ip);
                        break;
                default:
                        break;
                }
                disassemble_lineinfo(code, ip);
                printf("\n");
        }
}

static void
emit_byte(struct environment *env, struct tree_node *root, uint8_t byte)
{
        struct bytecode *code = env->code;
        if (env->error)
                return;
        struct lineinfo linfo;
        linfo.line = root->value.line;
        linfo.linepos = root->value.linepos;
        bytecode_write_byte(code, byte, linfo);
}

static void
emit_two_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1)
{
        emit_byte(env, root, byte0);
        emit_byte(env, root, byte1);
}

static void
emit_three_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1, uint8_t byte2)
{
        emit_byte(env, root, byte0);
        emit_byte(env, root, byte1);
        emit_byte(env, root, byte2);
}

static void
emit_constant(struct environment *env, struct tree_node *root, struct value val)
{
        if (env->error)
                return;
        struct bytecode *code = env->code;
        struct lineinfo linfo;
        linfo.line = root->value.line;
        linfo.linepos = root->value.linepos;
        bytecode_write_constant(code, val, linfo);
}

static uint8_t
left_byte(uint16_t word)
{
        return word >> 8;
}

static uint8_t
right_byte(uint16_t word)
{
        return word & 0xff;
}

static int
patch_skip_long(struct environment *env, struct tree_node *root, int codelen)
{
        if (env->error)
                return 0;
        struct bytecode *code = env->code;
        int jumplen;
        uint8_t jumplenfst, jumplenscn;
        jumplen = bytes_len(&code->code) - codelen;
        if (jumplen > MAX_SKIP_LONG) {
                semantics_error(env, root, "max skip size (%d) exceeded", MAX_SKIP_LONG);
                return 0;
        }
        jumplenfst = left_byte(jumplen);
        jumplenscn = right_byte(jumplen);
        code->code.buffer[codelen - 2] = jumplenfst;
        code->code.buffer[codelen - 1] = jumplenscn;
        return 1;
}

struct type
type_node_to_type(struct tree_node *node)
{
        struct type type;
        switch (node->type) {
                case NODE_STRING_TYPE:
                type.type = TYPE_STRING;
                break;
                case NODE_INTEGER_TYPE:
                type.type = TYPE_INTEGER;
                break;
                case NODE_BOOLEAN_TYPE:
                type.type = TYPE_BOOLEAN;
                break;
                default:
                printf("unreachable code in type_node_to_type\n");
        }
        return type;
}

void
environment_init(struct environment *env, struct bytecode *code)
{
        env->code = code;
        env->count = 0;
        env->error = 0;
}

static void
emit_variable_default(struct environment *env, struct tree_node *node, struct type type)
{
        switch (type.type) {
                case TYPE_BOOLEAN:
                emit_constant(env, node, value_from_c_bool(0));
                break;
                case TYPE_INTEGER:
                emit_constant(env, node, value_from_c_int(0));
                break;
        }
}

void
emit_declare_local(struct environment *env, struct tree_node *current, struct type type)
{
        if (env->count == MAX_LOCALS) {
                semantics_error(env, current, "maximum number of local variables exceeded");
                return;
        }
        if (environment_local_get(env, current->value, NULL) >= 0) {
                semantics_error(env, current, "variable already declared");
                return;
        }
        emit_variable_default(env, current, type);
        env->locals[env->count].name = current->value;
        env->locals[env->count].type = type;
        env->count++;

}

int
environment_local_get(struct environment *env, struct token name, struct local *local)
{
        int i;
        for (i = env->count - 1; i >= 0; i--) {
                if (token_equal(env->locals[i].name, name)) {
                        if (local != NULL)
                                *local = env->locals[i];
                        break;
                }
        }
        return i;
}

void
emit_statement(struct environment *env, struct tree_node *root)
{
        struct tree_node *node;
        struct type lefttype, righttype;
        struct local local;
        int localindex, count;
        switch (root->type) {
        case NODE_STAT_LIST:
                node = root->child;
                while (node != NULL) {
                        emit_statement(env, node);
                        node = node->next;
                }
                return;
        case NODE_VAR_DECL:
                node = root->left->child;
                while (node != NULL) {
                        emit_declare_local(env, node, type_node_to_type(root->right));
                        node = node->next;
                }
                return;
        case NODE_WRITE_STAT:
                count = 0;
                node = root->child->child;
                while (node != NULL) {
                        if (count == MAX_ARITY) {
                                semantics_error(env, node, "maximum arity (%d) exceeded", MAX_ARITY);
                                break;
                        }
                        emit_expression(env, node);
                        node = node->next;
                        count++;
                }
                emit_two_bytes(env, root, OP_WRITE, count);
                return;
        case NODE_WRITELN_STAT:
                count = 0;
                node = root->child->child;
                while (node != NULL) {
                        if (count == MAX_ARITY) {
                                semantics_error(env, node, "maximum arity (%d) exceeded", MAX_ARITY);
                                break;
                        }
                        emit_expression(env, node);
                        node = node->next;
                        count++;
                }
                emit_two_bytes(env, root, OP_WRITE, count);
                emit_byte(env, root, OP_NEWLINE);
                return;
        case NODE_ASSIGN_STAT:
                righttype = emit_expression(env, root->right);
                /* if not indexing */
                localindex = environment_local_get(env, root->left->value, &local);
                if (localindex < 0) {
                        semantics_error(env, root, "undefined variable");
                }
                if (!env->error && local.type.type != righttype.type) {
                        semantics_error(env, root, "mismatching types in assignment");
                }
                emit_three_bytes(env, root, OP_SET_LOCAL_LONG, left_byte(localindex), right_byte(localindex));
                return;
        case NODE_EXPR_STAT:
                emit_expression(env, root->child);
                emit_byte(env, root, OP_POPV);
                return;
        default:
                semantics_error(env, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
        }
}

static struct type
emit_cond_expr(struct environment *env, struct tree_node *root)
{
        int toendlens[MAX_CONDITIONAL_LEN];
        int codelen, *toendp;
        toendp = toendlens;
        struct tree_node *child, *subchild;
        struct type type0, type1;
        struct bytecode *code = env->code;
        child = root->child;

        type1 = emit_expression(env, child);
        if (type1.type != TYPE_BOOLEAN) {
                semantics_error(env, child, "if condition must be boolean");
        }
        emit_three_bytes(env, child, OP_SKIPF_LONG, 0, 0);
        codelen = bytes_len(&code->code);
        emit_byte(env, child, OP_POPV);
        child = child->next;
        type0 = emit_expression(env, child);
        emit_three_bytes(env, child, OP_SKIP_LONG, 0, 0);
        *toendp++ = bytes_len(&code->code);
        patch_skip_long(env, child, codelen);
        emit_byte(env, child, OP_POPV);
        child = child->next;

        if (child->type == NODE_ELSIF_EXPR_LIST) {
                subchild = child->child;
                while (subchild != NULL) {
                        type1 = emit_expression(env, subchild);
                        if (type1.type != TYPE_BOOLEAN) {
                                semantics_error(env, subchild, "elsif condition must be boolean");
                                return type0;
                        }
                        emit_three_bytes(env, subchild, OP_SKIPF_LONG, 0, 0);
                        codelen = bytes_len(&code->code);
                        emit_byte(env, subchild, OP_POPV);
                        subchild = subchild->next;
                        type1 = emit_expression(env, subchild);
                        if (type0.type != type1.type) {
                                semantics_error(env, subchild, "conditional expression types must be the same");
                                return type0;
                        }
                        emit_three_bytes(env, subchild, OP_SKIP_LONG, 0, 0);
                        if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                                semantics_error(env, subchild, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
                                return type0;
                        }
                        *toendp++ = bytes_len(&code->code);
                        patch_skip_long(env, subchild, codelen);
                        emit_byte(env, subchild, OP_POPV);
                        subchild = subchild->next;
                }
                child = child->next;
        }
        type1 = emit_expression(env, child);
        if (type0.type != type1.type) {
                semantics_error(env, child, "conditional expression types must be the same");
                return type0;
        }
        while (toendp > toendlens) {
                if (!patch_skip_long(env, root, *--toendp))
                        return type0;
        }
        return type0;
}

struct type
emit_expression(struct environment *env, struct tree_node *root)
{
        struct type lefttype, righttype;
        struct type inttype, booltype;
        struct local local;
        struct bytecode *code = env->code;
        int codelen, localindex;
        inttype.type = TYPE_INTEGER;
        booltype.type = TYPE_BOOLEAN;
        switch (root->type) {
        case NODE_AND_EXPR:
                lefttype = emit_expression(env, root->left);
                emit_three_bytes(env, root, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, root, OP_POPV);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_BOOLEAN || righttype.type != TYPE_BOOLEAN)) {
                        semantics_error(env, root, "operands must be booleans");
                }
                patch_skip_long(env, root, codelen);
                return booltype;
        case NODE_OR_EXPR:
                lefttype = emit_expression(env, root->left);
                emit_three_bytes(env, root, OP_SKIPF_LONG, 0, 3);
                emit_three_bytes(env, root, OP_SKIP_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, root, OP_POPV);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_BOOLEAN || righttype.type != TYPE_BOOLEAN)) {
                        semantics_error(env, root, "operands must be booleans");
                }
                patch_skip_long(env, root, codelen);
                return booltype;
        case NODE_NOT_EXPR:
                lefttype = emit_expression(env, root->left);
                if (!env->error && lefttype.type != TYPE_BOOLEAN) {
                        semantics_error(env, root, "operand must be a boolean");
                }
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_PLUS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_ADDI);
                return inttype;
        case NODE_MINUS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_SUBI);
                return inttype;
        case NODE_TIMES_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_MULI);
                return inttype;
        case NODE_DIVIDE_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_DIVI);
                return inttype;
        case NODE_NEG_EXPR:
                emit_byte(env, root, OP_ZERO);
                lefttype = emit_expression(env, root->left);
                if (!env->error && lefttype.type != TYPE_INTEGER) {
                        semantics_error(env, root, "operand must be an integer");
                }
                emit_byte(env, root, OP_SUBI);
                return inttype;
        case NODE_NEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && lefttype.type != righttype.type) {
                        semantics_error(env, root, "operands must be of the same type");
                }
                emit_byte(env, root, OP_EQUA);
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_EQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && lefttype.type != righttype.type) {
                        semantics_error(env, root, "operands must be of the same type");
                }
                emit_byte(env, root, OP_EQUA);
                return booltype;
        case NODE_GREATEREQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_IGRTEQ);
                return booltype;
        case NODE_GREATER_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_IGRT);
                return booltype;
        case NODE_LESSEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_ILEQ);
                return booltype;
        case NODE_LESS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!env->error && (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER)) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_ILT);
                return booltype;
        case NODE_COND_EXPR:
                return emit_cond_expr(env, root);
        case NODE_BOOLEAN_CONST:
                emit_byte(env, root, OP_LOCI);
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantics_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_constant(env, root, value_from_c_bool(parse_boolean_token(root->value)));
                return booltype;
        case NODE_INTGER_CONST:
                emit_byte(env, root, OP_LOCI);
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantics_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_constant(env, root, value_from_c_int(parse_integer_token(root->value)));
                return inttype;
        case NODE_ID:
                if ((localindex = environment_local_get(env, root->value, &local)) < 0) {
                        semantics_error(env, root, "undefined variable");
                        break;
                }
                emit_three_bytes(env, root, OP_GET_LOCAL_LONG, left_byte(localindex), right_byte(localindex));
                return local.type;
        default:
                semantics_error(env, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
        }
        return inttype;
}

struct bytecode *
generate_bytecode(struct tree_node *parsetree)
{
        struct bytecode *code = malloc(sizeof(struct bytecode));
        bytecode_init(code);
        struct environment env;
        environment_init(&env, code);
        emit_statement(&env, parsetree);
        if (env.error) {
                bytecode_free(code);
                return NULL;
        }
        emit_byte(&env, parsetree, OP_HALT);
        return code;
}