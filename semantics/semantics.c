#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantics.h"

static struct semantic_type emit_cond_expression(struct environment *env, struct tree_node *root);
static struct semantic_type emit_indexing_expression(struct environment *env, struct tree_node *root);
static void emit_for_statement(struct environment *env, struct tree_node *root);
static int environment_local_get_check_write(struct environment *env, struct token name, struct tree_node *root);
static struct semantic_type compute_lhs_type(struct environment *env, int localindex, struct tree_node *lhs);
static void emit_op_set_local(struct environment *env, struct tree_node *node, int localindex, struct semantic_type rhs_type);
static void emit_assign_statement(struct environment *env, struct tree_node *root);
static void emit_repeat_statement(struct environment *env, struct tree_node *root);
static void emit_while_statement(struct environment *env, struct tree_node *root);
static void emit_if_statement(struct environment *env, struct tree_node *root);
static int emit_declare_local(struct environment *env, struct tree_node *current, struct semantic_type type, uint8_t perms);
static void emit_vector_type(struct environment *env, struct tree_node *root, struct semantic_type type);
static void emit_variable_default(struct environment *env, struct tree_node *node, struct semantic_type type);
static void emit_pop_scope(struct environment *env, struct tree_node *node);
static void emit_push_scope(struct environment *env, struct tree_node *node);
static int emit_skip_back_long(struct environment *env, struct tree_node *root, int codelen);
static void emit_constant(struct environment *env, struct tree_node *root, struct value val);
static void emit_load_constant(struct environment *env, struct tree_node *root, struct value val);
static struct semantic_type emit_vector_constant(struct environment *env, struct tree_node *root, int depth);
static void emit_byte(struct environment *env, struct tree_node *root, uint8_t byte);
static void emit_two_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1);
static void emit_three_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1, uint8_t byte2);
static void emit_four_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3);
static struct semantic_type compute_indexed_semantic_type(int index_count, struct semantic_type indexed_type);
static int patch_skip_long(struct environment *env, struct tree_node *root, int codelen);
static void environment_init(struct environment *env, struct bytecode *code);
static int environment_local_get(struct environment *env, struct token name);
static void init_local(struct local *loc, struct token name, struct semantic_type type, int depth, uint8_t perms);
static void emit_read_type(struct environment *env, struct tree_node *node, struct semantic_type lhs_type);
static int parse_boolean_token(struct token token);
static int parse_integer_token(struct token token);
static struct semantic_type type_node_to_type(struct environment *env, struct tree_node *node);
static struct semantic_type vector_type_node_to_type(struct environment *env, struct tree_node *node);
static void semantic_error(struct environment *env, struct tree_node *root, char *fmt, ...);

struct bytecode *
generate_bytecode(struct tree_node *parsetree)
{
        struct bytecode *code = malloc(sizeof(struct bytecode));
        bytecode_init(code);
        struct environment env;
        environment_init(&env, code);
        emit_statement(&env, parsetree);
        if (env.error) {
                /* OK leak, the OS will take care of this */
                return NULL;
        }
        emit_byte(&env, parsetree, OP_HALT);
        return code;
}

void
emit_statement(struct environment *env, struct tree_node *root)
{
        struct tree_node *node;
        int count;
        switch (root->type) {
        case NODE_STAT_LIST:
                emit_push_scope(env, root);
                node = root->child;
                while (node != NULL) {
                        emit_statement(env, node);
                        node = node->next;
                }
                emit_pop_scope(env, root);
                break;
        case NODE_VAR_DECL:
                node = root->left->child;
                while (node != NULL) {
                        emit_declare_local(env, node, type_node_to_type(env, root->right), LOCAL_PERM_RW);
                        node = node->next;
                }
                break;
        case NODE_WRITE_STAT:
        case NODE_WRITELN_STAT:
                count = 0;
                node = root->child->child;
                while (node != NULL) {
                        if (count == MAX_ARITY) {
                                semantic_error(env, node, "maximum arity (%d) exceeded", MAX_ARITY);
                                break;
                        }
                        emit_expression(env, node);
                        node = node->next;
                        count++;
                }
                emit_two_bytes(env, root, OP_WRITE, count);
                if (root->type == NODE_WRITELN_STAT)
                        emit_byte(env, root, OP_NEWLINE);
                break;
        case NODE_READ_STAT:
                count = 0;
                node = root->child->child;
                while (node != NULL) {
                        if (count == MAX_ARITY) {
                                semantic_error(env, node, "maximum arity (%d) exceeded", MAX_ARITY);
                                break;
                        }
                        struct tree_node *var = lhs_variable(node);
                        int localindex = environment_local_get_check_write(env, var->value, root);
                        if (localindex < 0)
                                break;
                        struct semantic_type lhs_type = compute_lhs_type(env, localindex, root->left);
                        if (lhs_type.id == VAL_VECTOR) {
                                semantic_error(env, node, "reading vectors is not supported");
                                break;
                        }
                        emit_read_type(env, node, lhs_type);
                        emit_op_set_local(env, node, localindex, lhs_type);
                        node = node->next;
                        count++;
                }
                break;
        case NODE_ASSIGN_STAT:
                emit_assign_statement(env, root);
                break;
        case NODE_IF_STAT:
                emit_if_statement(env, root);
                break;
        case NODE_WHILE_STAT:
                emit_while_statement(env, root);
                break;
        case NODE_REPEAT_STAT:
                emit_repeat_statement(env, root);
                break;
        case NODE_FOR_STAT:
                emit_for_statement(env, root);
                break;
        case NODE_EXPR_STAT:
                emit_expression(env, root->child);
                emit_byte(env, root, OP_POPV);
                break;
        default:
                semantic_error(env, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
                break;
        }
        env->panic = 0;
}

struct semantic_type
emit_expression(struct environment *env, struct tree_node *root)
{
        struct semantic_type lefttype, righttype;
        struct semantic_type inttype, booltype, strtype;
        struct bytecode *code = env->code;
        int codelen, localindex;
        inttype = semantic_type_scalar(VAL_INTEGER);
        booltype = semantic_type_scalar(VAL_BOOLEAN);
        strtype = semantic_type_scalar(VAL_STRING);
        switch (root->type) {
        case NODE_AND_EXPR:
                lefttype = emit_expression(env, root->left);
                emit_three_bytes(env, root, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, root, OP_POPV);
                righttype = emit_expression(env, root->right);
                if (lefttype.id != VAL_BOOLEAN || righttype.id != VAL_BOOLEAN) {
                        semantic_error(env, root, "operands must be booleans");
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
                if (lefttype.id != VAL_BOOLEAN || righttype.id != VAL_BOOLEAN) {
                        semantic_error(env, root, "operands must be booleans");
                }
                patch_skip_long(env, root, codelen);
                return booltype;
        case NODE_NOT_EXPR:
                lefttype = emit_expression(env, root->left);
                if (lefttype.id != VAL_BOOLEAN) {
                        semantic_error(env, root, "operand must be a boolean");
                }
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_PLUS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.id != VAL_INTEGER || righttype.id != VAL_INTEGER) {
                        semantic_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_ADDI);
                return inttype;
        case NODE_MINUS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.id != VAL_INTEGER || righttype.id != VAL_INTEGER) {
                        semantic_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_SUBI);
                return inttype;
        case NODE_TIMES_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.id != VAL_INTEGER || righttype.id != VAL_INTEGER) {
                        semantic_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_MULI);
                return inttype;
        case NODE_DIVIDE_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.id != VAL_INTEGER || righttype.id != VAL_INTEGER) {
                        semantic_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_DIVI);
                return inttype;
        case NODE_NEG_EXPR:
                emit_byte(env, root, OP_ZERO);
                lefttype = emit_expression(env, root->left);
                if (lefttype.id != VAL_INTEGER) {
                        semantic_error(env, root, "operand must be an integer");
                }
                emit_byte(env, root, OP_SUBI);
                return inttype;
        case NODE_NEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_type_equal(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be of the same type");
                }
                emit_byte(env, root, OP_EQUA);
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_EQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_type_equal(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be of the same type");
                }
                emit_byte(env, root, OP_EQUA);
                return booltype;
        case NODE_GREATEREQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_IGRTEQ);
                return booltype;
        case NODE_GREATER_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_IGRT);
                return booltype;
        case NODE_LESSEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_ILEQ);
                return booltype;
        case NODE_LESS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_ILT);
                return booltype;
        case NODE_COND_EXPR:
                return emit_cond_expression(env, root);
        case NODE_BOOLEAN_CONST:
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantic_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_load_constant(env, root, value_from_c_bool(parse_boolean_token(root->value)));
                return booltype;
        case NODE_INTGER_CONST:
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantic_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_load_constant(env, root, value_from_c_int(parse_integer_token(root->value)));
                return inttype;
        case NODE_STRING_CONST:
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantic_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_load_constant(env, root, value_from_token(root->value));
                return strtype;
        case NODE_VECTOR_CONST:
                return emit_vector_constant(env, root, 0);
        case NODE_ID:
                if ((localindex = environment_local_get(env, root->value)) < 0) {
                        semantic_error(env, root, "undefined variable");
                        break;
                }
                emit_three_bytes(env, root, OP_GET_LOCAL_LONG, left_byte(localindex), right_byte(localindex));
                return env->locals[localindex].type;
        case NODE_INDEXING:
                return emit_indexing_expression(env, root);
        default:
                semantic_error(env, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
        }
        return inttype;
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
emit_four_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3)
{
        emit_byte(env, root, byte0);
        emit_byte(env, root, byte1);
        emit_byte(env, root, byte2);
        emit_byte(env, root, byte3);
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

static void
emit_load_constant(struct environment *env, struct tree_node *root, struct value val)
{
        emit_byte(env, root, OP_LOC_LONG);
        emit_constant(env, root, val);
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
                semantic_error(env, root, "max skip size (%d) exceeded", MAX_SKIP_LONG);
                return 0;
        }
        jumplenfst = left_byte(jumplen);
        jumplenscn = right_byte(jumplen);
        code->code.buffer[codelen - 2] = jumplenfst;
        code->code.buffer[codelen - 1] = jumplenscn;
        return 1;
}

static int
emit_skip_back_long(struct environment *env, struct tree_node *root, int codelen)
{
        if (env->error)
                return 0;
        emit_three_bytes(env, root, OP_SKIP_BACK_LONG, 0, 0);
        struct bytecode *code = env->code;
        int jumplen;
        uint8_t jumplenfst, jumplenscn;
        jumplen = bytes_len(&code->code) - codelen;
        if (jumplen > MAX_SKIP_LONG) {
                semantic_error(env, root, "max skip size (%d) exceeded", MAX_SKIP_LONG);
                return 0;
        }
        jumplenfst = left_byte(jumplen);
        jumplenscn = right_byte(jumplen);
        code->code.buffer[bytes_len(&code->code) - 2] = jumplenfst;
        code->code.buffer[bytes_len(&code->code) - 1] = jumplenscn;
        return 1;
}

static struct semantic_type
type_node_to_type(struct environment *env, struct tree_node *node)
{
        struct semantic_type type;
        switch (node->type) {
        case NODE_STRING_TYPE:
                type = semantic_type_scalar(VAL_STRING);
                break;
        case NODE_INTEGER_TYPE:
                type = semantic_type_scalar(VAL_INTEGER);
                break;
        case NODE_BOOLEAN_TYPE:
                type = semantic_type_scalar(VAL_BOOLEAN);
                break;
        case NODE_VECTOR_TYPE:
                type = vector_type_node_to_type(env, node);
                break;
        default:
                printf("unreachable code in type_node_to_type\n");
        }
        return type;
}

static struct semantic_type
vector_type_node_to_type(struct environment *env, struct tree_node *node)
{
        struct semantic_type type;
        type.id = VAL_VECTOR;
        type.size = parse_integer_token(node->left->value);
        if (type.size <= 0) {
                semantic_error(env, node->left, "cannot use a value <= 0 as a vector dimension");
        }
        type.dimensions[0] = type.size;
        type.rank = 1;

        struct semantic_type inside = type_node_to_type(env, node->right);
        type.base = inside.base;
        if (inside.id == VAL_VECTOR) {
                for (int i = 0; i < inside.rank; i++) {
                        if (type.rank == MAX_VECTOR_DIMENSIONS - 1) {
                                semantic_error(env, node, "maximum vector rank exceeded");
                                break;
                        }
                        type.dimensions[type.rank++] = inside.dimensions[i];
                        type.size += inside.dimensions[i];
                }
        }
        return type; 
}

static void
environment_init(struct environment *env, struct bytecode *code)
{
        env->code = code;
        env->count = 0;
        env->error = 0;
        env->depth = 0;
        env->panic = 0;
}

static int
environment_local_get(struct environment *env, struct token name)
{
        int i;
        for (i = env->count - 1; i >= 0; i--) {
                if (token_equal(env->locals[i].name, name)) {
                        break;
                }
        }
        return i;
}

static void
emit_push_scope(struct environment *env, struct tree_node *node)
{
        env->depth++;
}

static void
emit_pop_scope(struct environment *env, struct tree_node *node)
{
        while (env->count > 0 && env->locals[env->count - 1].depth == env->depth) {
                env->count--;
                emit_byte(env, node, OP_POPV);
        }
        env->depth--;
}

static void
emit_variable_default(struct environment *env, struct tree_node *node, struct semantic_type type)
{
        switch (type.id) {
        case VAL_BOOLEAN:
                emit_byte(env, node, OP_FALSE);
                break;
        case VAL_INTEGER:
                emit_byte(env, node, OP_ZERO);
                break;
        case VAL_STRING:
                emit_byte(env, node, OP_EMPTY_STRING);
                break;
        case VAL_VECTOR: {
                for (int i = 0; i < type.size; i++) {
                        emit_variable_default(env, node, semantic_type_scalar(type.base));
                        emit_byte(env, node, OP_POP_TO_ASTACK);
                }
                emit_byte(env, node, OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG);
                struct value val;
                val.type = semantic_type_to_run_type(type);
                emit_constant(env, node, val);
                emit_vector_type(env, node, type);
                break;
        }
        }
}

static void
emit_read_type(struct environment *env, struct tree_node *node, struct semantic_type lhs_type)
{
        emit_byte(env, node, OP_READ);
        enum read_format fmt;
        switch (lhs_type.id) {
                case VAL_BOOLEAN:
                       fmt = RF_BOOLEAN;
                       break;
                case VAL_INTEGER:
                        fmt = RF_INTEGER;
                        break;
                case VAL_STRING:
                        fmt = RF_STRING;
                        break; 
        }
        emit_byte(env, node, fmt);
}

static void
init_local(struct local *loc, struct token name, struct semantic_type type, int depth, uint8_t perms)
{
        loc->name = name;
        loc->type = type;
        loc->perms = perms;
        loc->depth = depth;
}

static int
emit_declare_local(struct environment *env, struct tree_node *current, struct semantic_type type, uint8_t perms)
{
        if (env->count == MAX_LOCALS) {
                semantic_error(env, current, "maximum number of local variables exceeded");
                return 0;
        }
        int localindex;
        if ((localindex = environment_local_get(env, current->value)) >= 0 && env->locals[localindex].depth == env->depth) {
                semantic_error(env, current, "variable already declared");
                return 0;
        }
        emit_variable_default(env, current, type);
        init_local(&env->locals[env->count], current->value, type, env->depth, perms);
        return env->count++;
}

static void
emit_if_statement(struct environment *env, struct tree_node *root)
{
        int toendlens[MAX_CONDITIONAL_LEN];
        int codelen, *toendp;
        toendp = toendlens;
        struct tree_node *child;
        struct semantic_type type1;
        struct bytecode *code = env->code;
        child = root->child;
        while (child != NULL && child->type == NODE_CONDITION_AND_STATEMENT) {
                type1 = emit_expression(env, child->left);
                if (type1.id != VAL_BOOLEAN) {
                        semantic_error(env, child->left, "if condition must be boolean");
                        return;
                }
                emit_three_bytes(env, child->left, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, child->left, OP_POPV);
                emit_statement(env, child->right);
                emit_three_bytes(env, child, OP_SKIP_LONG, 0, 0);
                if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                        semantic_error(env, child, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
                        return;
                }
                *toendp++ = bytes_len(&code->code);
                patch_skip_long(env, child, codelen);
                emit_byte(env, child, OP_POPV);
                child = child->next;
        }
        if (child != NULL)
                emit_statement(env, child);
        while (toendp > toendlens) {
                if (!patch_skip_long(env, root, *--toendp))
                        return;
        }
}

static void
emit_while_statement(struct environment *env, struct tree_node *root)
{
        int codelen, startlen;
        struct semantic_type type1;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);
        type1 = emit_expression(env, root->left);
        if (type1.id != VAL_BOOLEAN) {
                semantic_error(env, root->left, "while condition must be boolean");
                return;
        }
        emit_three_bytes(env, root->left, OP_SKIPF_LONG, 0, 0);
        codelen = bytes_len(&code->code);
        emit_byte(env, root->left, OP_POPV);
        emit_statement(env, root->right);
        emit_skip_back_long(env, root->right, startlen);
        emit_byte(env, root->left, OP_POPV);
        patch_skip_long(env, root, codelen);
}

static void
emit_repeat_statement(struct environment *env, struct tree_node *root)
{
        int startlen;
        struct semantic_type type1;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);

        emit_statement(env, root->left);

        type1 = emit_expression(env, root->right);
        if (type1.id != VAL_BOOLEAN) {
                semantic_error(env, root->right, "until condition must be boolean");
                return;
        }

        emit_three_bytes(env, root->right, OP_SKIPF_LONG, 0, 3);
        emit_skip_back_long(env, root->right, startlen);
}

static int
environment_local_get_check_write(struct environment *env, struct token name, struct tree_node *root)
{
        int localindex = environment_local_get(env, name);
        if (localindex < 0) {
                semantic_error(env, root->left, "undefined variable");
        }
        struct local loc = env->locals[localindex];
        if ((loc.perms & LOCAL_PERM_W) == 0) {
                semantic_error(env, root->left, "cannot assign read-only variable");
        }
        return localindex;
}

static struct semantic_type
compute_lhs_type(struct environment *env, int localindex, struct tree_node *lhs)
{
        struct local local = env->locals[localindex];
        struct semantic_type toret;
        switch (local.type.id) {
        case VAL_VECTOR: {
                struct semantic_type indexed_type = local.type;
                struct tree_node *indices_node = lhs->right;
                int index_count = 0;
                for (struct tree_node *node = indices_node; node != NULL; node = node->next) {
                        index_count++;
                        if (emit_expression(env, node).id != VAL_INTEGER) {
                                semantic_error(env, node, "cannot index array with non integer");
                                break;
                        }
                }
                toret = compute_indexed_semantic_type(index_count, indexed_type);
                break;
        }
        default:
                toret = local.type;
        }
        return toret;
}

static void
emit_op_set_local(struct environment *env, struct tree_node *node, int localindex, struct semantic_type rhs_type)
{
        if (localindex < 0)
                return;
        struct local loc = env->locals[localindex];
        switch (loc.type.id) {
                case VAL_VECTOR:
                        emit_four_bytes(env, node, OP_SET_INDEXED_LOCAL_LONG, left_byte(localindex), right_byte(localindex), loc.type.rank - rhs_type.rank);
                        break;
                default:
                        emit_three_bytes(env, node, OP_SET_LOCAL_LONG, left_byte(localindex), right_byte(localindex));
                        break;
        }
}

static void
emit_assign_statement(struct environment *env, struct tree_node *root)
{
        struct tree_node *lhs = root->left;
        struct tree_node *rhs = root->right;
        struct tree_node *var = lhs_variable(lhs);
        struct semantic_type left_type, right_type;

        int localindex = environment_local_get_check_write(env, var->value, root);
        if (localindex < 0)
                return;

        right_type = emit_expression(env, rhs);

        left_type = compute_lhs_type(env, localindex, lhs);

        if (!semantic_type_equal(left_type, right_type)) {
                semantic_error(env, root, "mismatching types in assignment (%s = %s)", value_type_to_string(left_type.id), value_type_to_string(right_type.id));
        }

        emit_op_set_local(env, lhs, localindex, right_type);
}

static void
emit_for_statement(struct environment *env, struct tree_node *root)
{
        struct tree_node *assign = root->left;
        struct tree_node *condition = assign->next;
        struct tree_node *statlist = root->right;

        struct token forcond_token;
        forcond_token.type = TOKEN_ID;
        forcond_token.start = "0forcond";
        forcond_token.length = 8;
        forcond_token.line = condition->right->value.line;
        forcond_token.linepos = condition->right->value.linepos;

        struct tree_node forcond_node;
        forcond_node.type = NODE_ID;
        forcond_node.child = NULL;
        forcond_node.left = NULL;
        forcond_node.right = NULL;
        forcond_node.next = NULL;
        forcond_node.value = forcond_token;
        
        emit_push_scope(env, root);

        struct semantic_type inttype = semantic_type_scalar(VAL_INTEGER);
        int incindex = emit_declare_local(env, assign->left, inttype, LOCAL_PERM_RW);
        emit_assign_statement(env, assign);
        env->locals[env->count - 1].perms = LOCAL_PERM_R;

        int forcond_index = emit_declare_local(env, &forcond_node, inttype, LOCAL_PERM_R);
        struct semantic_type type1;
        type1 = emit_expression(env, condition->right);
        if (type1.id != VAL_INTEGER) {
                semantic_error(env, condition->right, "for loop upper range must be an integer");
                return;
        }
        emit_op_set_local(env, &forcond_node, forcond_index, inttype);

        int codelen, startlen;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);
        emit_three_bytes(env, root, OP_GET_LOCAL_LONG, left_byte(incindex), right_byte(incindex));
        emit_three_bytes(env, root, OP_GET_LOCAL_LONG, left_byte(forcond_index), right_byte(forcond_index));
        emit_byte(env, condition, OP_ILEQ);
        emit_three_bytes(env, condition, OP_SKIPF_LONG, 0, 0);
        codelen = bytes_len(&code->code);
        emit_byte(env, condition, OP_POPV);
        emit_statement(env, statlist);
        emit_three_bytes(env, root, OP_GET_LOCAL_LONG, left_byte(incindex), right_byte(incindex));
        emit_byte(env, root, OP_ONE);
        emit_byte(env, root, OP_ADDI);
        emit_op_set_local(env, root, incindex, inttype);
        emit_skip_back_long(env, statlist, startlen);
        emit_byte(env, condition, OP_POPV);
        patch_skip_long(env, root, codelen);

        emit_pop_scope(env, root);
}

static struct semantic_type
emit_cond_expression(struct environment *env, struct tree_node *root)
{
        int toendlens[MAX_CONDITIONAL_LEN];
        int codelen, *toendp;
        toendp = toendlens;
        struct tree_node *child;
        struct semantic_type type0, type1;
        struct bytecode *code = env->code;
        child = root->child;
        while (child != NULL && child->type == NODE_CONDITION_AND_EXPRESSION) {
                type1 = emit_expression(env, child->left);
                if (type1.id != VAL_BOOLEAN) {
                        semantic_error(env, child->left, "if condition must be boolean");
                        return type0;
                }
                emit_three_bytes(env, child->left, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, child->left, OP_POPV);
                type1 = emit_expression(env, child->right);
                if (child == root->child)
                        type0 = type1;
                if (type0.id != type1.id) {
                        semantic_error(env, child, "conditional expression types must be the same");
                        return type0;
                }
                emit_three_bytes(env, child, OP_SKIP_LONG, 0, 0);
                if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                        semantic_error(env, child, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
                        return type0;
                }
                *toendp++ = bytes_len(&code->code);
                patch_skip_long(env, child, codelen);
                emit_byte(env, child, OP_POPV);
                child = child->next;
        }
        type1 = emit_expression(env, child);
        if (type0.id != type1.id) {
                semantic_error(env, child, "conditional expression types must be the same");
                return type0;
        }
        while (toendp > toendlens) {
                if (!patch_skip_long(env, root, *--toendp))
                        return type0;
        }
        return type0;
}

static struct semantic_type
emit_indexing_expression(struct environment *env, struct tree_node *root)
{
        struct tree_node *index = root->right;
        struct tree_node *indexed = root->left;
        struct semantic_type indexed_type, index_type;
        int index_count = 0;

        indexed_type = emit_expression(env, indexed);
        if (indexed_type.id != VAL_VECTOR) {
                semantic_error(env, indexed, "cannot index a non vector");
        }

        while (index != NULL) {
                index_type = emit_expression(env, index);
                if (index_type.id != VAL_INTEGER) {
                        semantic_error(env, index, "cannot use a non integer as an index");
                }
                index_count++;
                if (index_count > indexed_type.rank) {
                        semantic_error(env, index, "maximum number of indices exceeded for array exceeded (%d)", indexed_type.rank);
                }
                index = index->next;
        }

        struct semantic_type toret = compute_indexed_semantic_type(index_count, indexed_type);

        emit_two_bytes(env, root, OP_GET_INDEX, index_count);

        return toret;
}

static struct semantic_type
compute_indexed_semantic_type(int index_count, struct semantic_type indexed_type)
{
        struct semantic_type toret;
        if (index_count == indexed_type.rank) {
                toret = semantic_type_scalar(indexed_type.base);
        } else {
                struct semantic_type toret;
                toret.id = VAL_VECTOR;
                toret.base = indexed_type.base;
                toret.rank = indexed_type.rank - index_count;

                toret.size = 0;
                for (int i = index_count; i < indexed_type.rank; i++) {
                        toret.size += indexed_type.dimensions[i];
                        toret.dimensions[i - index_count] = indexed_type.dimensions[i];
                }
        }
        return toret;
}

static struct semantic_type
emit_vector_constant(struct environment *env, struct tree_node *root, int depth)
{
        struct semantic_type toret;
        if (root->type != NODE_VECTOR_CONST) {
                toret = emit_expression(env, root);
                emit_byte(env, root, OP_POP_TO_ASTACK);
                return toret;
        }

        struct semantic_type type;
        type.base = VAL_INTEGER;
        int size = 0;
        if (root->child != NULL) {
                type = emit_vector_constant(env, root->child, depth + 1);
                toret.rank = type.rank + 1;
                memcpy(toret.dimensions, type.dimensions, MAX_VECTOR_DIMENSIONS);
                toret.base = type.base;
                size = type.size;
                toret.dimensions[toret.rank - 1] = 1;

                for (struct tree_node *node = root->child->next; node != NULL; node = node->next) {
                        struct semantic_type current_type = emit_vector_constant(env, node, depth + 1);
                        if (!semantic_type_equal(type, current_type)) {
                                semantic_error(env, node, "vector elements must be homogeneous");
                                break;
                        }
                        size += type.size;
                        toret.dimensions[toret.rank - 1]++;
                }       
        }

        toret.id = VAL_VECTOR;
        toret.size = size;

        if (depth != 0)
                return toret;
        

        emit_byte(env, root, OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG);
        struct value val;
        val.type = semantic_type_to_run_type(toret);
        val.as.vector.astackent = NULL;
        emit_constant(env, root, val);

        emit_vector_type(env, root, toret);

        return toret;
}

static void
emit_vector_type(struct environment *env, struct tree_node *root, struct semantic_type type)
{
        for (int i = type.rank - 1; i >= 0; i--) {
                emit_byte(env, root, OP_LOC_LONG);
                emit_constant(env, root, value_from_c_int(type.dimensions[i]));
        }
        emit_two_bytes(env, root, OP_INIT_VEC_DIMS, type.rank);
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
        case OP_ADDI: return "OP_ADDI";
        case OP_DIVI: return "OP_DIVI";
        case OP_EMPTY_STRING: return "OP_EMPTY_STRING";
        case OP_EQUA: return "OP_EQUA";
        case OP_FALSE: return "OP_FALSE";
        case OP_GET_INDEX: return "OP_GET_INDEX";
        case OP_GET_LOCAL_LONG: return "OP_GET_LOCAL_LONG";
        case OP_HALT: return "OP_HALT";
        case OP_IGRTEQ: return "OP_IGRTEQ";
        case OP_IGRT: return "OP_IGRT";
        case OP_ILEQ: return "OP_ILEQ";
        case OP_ILT: return "OP_ILT";
        case OP_INIT_VEC_DIMS: return "OP_INIT_VEC_DIMS";
        case OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG: return "OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG";
        case OP_LOC_LONG: return "OP_LOC_LONG";
        case OP_MULI: return "OP_MULI";
        case OP_NEWLINE: return "OP_NEWLINE";
        case OP_NOT: return "OP_NOT";
        case OP_ONE: return "OP_ONE";
        case OP_POP_TO_ASTACK: return "OP_POP_TO_ASTACK";
        case OP_POPV: return "OP_POPV";
        case OP_READ: return "OP_READ";
        case OP_SET_INDEXED_LOCAL_LONG: return "OP_SET_INDEXED_LOCAL_LONG";
        case OP_SET_LOCAL_LONG: return "OP_SET_LOCAL_LONG";
        case OP_SKIP_BACK_LONG: return "OP_SKIP_BACK_LONG";
        case OP_SKIPF_LONG: return "OP_SKIPF_LONG";
        case OP_SKIP_LONG: return "OP_SKIP_LONG";
        case OP_SUBI: return "OP_SUBI";
        case OP_WRITE: return "OP_WRITE";
        case OP_ZERO: return "OP_ZERO";
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
        uint8_t constantaddr_left = bytes_at(&code->code, ip++);
        uint8_t constantaddr_right = bytes_at(&code->code, ip++);
        uint16_t constantaddr = join_bytes(constantaddr_left, constantaddr_right);
        struct value val = valuelist_at(&code->constants, constantaddr);
        printf("%d ", constantaddr);
        if (val.type.id != VAL_VECTOR)
                value_print(val);
        else
                printf("%s", value_type_to_string(val.type.id));
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
                case OP_LOC_LONG:
                case OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG:
                        ip = disassemble_constant(code, ip);
                        break;
                case OP_SKIP_BACK_LONG:
                case OP_SKIP_LONG:
                case OP_SKIPF_LONG:
                case OP_GET_LOCAL_LONG:
                case OP_SET_LOCAL_LONG:
                        ip = disassemble_argument_long(code, ip);
                        break;
                case OP_WRITE:
                case OP_GET_INDEX:
                case OP_INIT_VEC_DIMS:
                case OP_READ:
                        ip = disassemble_argument(code, ip);
                        break;
                case OP_SET_INDEXED_LOCAL_LONG:
                        ip = disassemble_argument_long(code, ip);
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
semantic_error(struct environment *env, struct tree_node *root, char *fmt, ...)
{
        if (env->panic)
                return;
        env->error = 1;
        env->panic = 1;
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "semantic error ");
        fprintf(stderr, "[at %d:%d]: ", root->value.line, root->value.linepos);
        fprintf(stderr, "at '%.*s', ", root->value.length, root->value.start);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}