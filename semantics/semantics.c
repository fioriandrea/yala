#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantics.h"

static struct semantic_type emit_cond_expression(struct environment *env, struct tree_node *root);
static struct semantic_type emit_indexing_expression(struct environment *env, struct tree_node *root);
static struct semantic_type emit_module_call(struct environment *env, struct tree_node *root);
static void emit_for_statement(struct environment *env, struct tree_node *root);
static int environment_local_search_check_write(struct environment *env, struct token name, struct tree_node *root, struct local_position *localpos);
static struct semantic_type emit_op_set_local_lhs_type(struct environment *env, struct local_position localpos, struct tree_node *lhs);
static void emit_op_set_local(struct environment *env, struct tree_node *node, struct local_position localpos, struct semantic_type rhs_type);
static void emit_op_local_long(struct environment *env, struct tree_node *node, enum opcode op, struct local_position localpos);
static struct local_position environment_local_push(struct environment *env, struct local topush);
static void emit_assign_statement(struct environment *env, struct tree_node *root);
static void emit_repeat_statement(struct environment *env, struct tree_node *root);
static void emit_while_statement(struct environment *env, struct tree_node *root);
static void emit_if_statement(struct environment *env, struct tree_node *root);
static int emit_declare_local_default(struct environment *env, struct tree_node *current, struct semantic_type type, uint8_t perms, struct local_position *localpos);
static void emit_function_declaration(struct environment *env, struct tree_node *root);
static struct semantic_type build_function_semantic_type(struct environment *env, struct tree_node *root);
static void emit_body(struct environment *env, struct tree_node *statements_node, struct tree_node *return_type_node, int arity);
static void emit_variable_default(struct environment *env, struct tree_node *node, struct semantic_type type);
static void push_loop(struct environment *env);
static void pop_loop(struct environment *env);
static void emit_break(struct environment *env, struct tree_node *node);
static void patch_breaks(struct environment *env, struct tree_node *root);
static void emit_pop_scope(struct environment *env, struct tree_node *node);
static void emit_push_scope(struct environment *env, struct tree_node *node);
static int emit_skip_back_long(struct environment *env, struct tree_node *root, int codelen);
static void emit_constant(struct environment *env, struct tree_node *root, union value val);
static void emit_load_constant(struct environment *env, struct tree_node *root, enum value_type type, union value val);
static struct semantic_type emit_vector_constant(struct environment *env, struct tree_node *root, int depth);
static void emit_popv(struct environment *env, struct tree_node *node, struct semantic_type type);
static void emit_byte(struct environment *env, struct tree_node *root, uint8_t byte);
static void emit_two_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1);
static void emit_three_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1, uint8_t byte2);
static struct semantic_type compute_indexed_semantic_type(int index_count, struct semantic_type indexed_type);
static struct semantic_type emit_indexing_prelude(struct environment *env, struct semantic_type indexed_type, struct tree_node *indexing_node);
static int emit_unpatched_skip_long(struct environment *env, struct tree_node *root, enum opcode op);
static int patch_skip_long(struct environment *env, struct tree_node *root, int codelen);
static void environment_init(struct environment *env, struct environment *parent, struct bytecode *code);
static void environment_free(struct environment *env);
static int environment_local_search(struct environment *env, struct token name, struct local_position *localpos);
struct local environment_local_get(struct environment *env, struct local_position localpos);
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
        environment_init(&env, NULL, code);
        emit_statement(&env, parsetree);
        environment_free(&env);
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
                        if (!emit_declare_local_default(env, node, type_node_to_type(env, root->right), LOCAL_PERM_RW, NULL))
                                break;
                        node = node->next;
                }
                break;
        case NODE_WRITE_STAT:
        case NODE_WRITELN_STAT:
                count = 0;
                node = root->child;
                while (node != NULL) {
                        if (count == MAX_ARITY) {
                                semantic_error(env, node, "maximum arity (%d) exceeded", MAX_ARITY);
                                break;
                        }
                        struct semantic_type type = emit_expression(env, node);
                        emit_two_bytes(env, node, OP_PUSH_BYTE, type.id);
                        emit_two_bytes(env, node, OP_PUSH_BYTE, type.base); /* eventually remove */
                        node = node->next;
                        count++;
                }
                emit_two_bytes(env, root, OP_WRITE, count);
                if (root->type == NODE_WRITELN_STAT)
                        emit_byte(env, root, OP_NEWLINE);
                break;
        case NODE_READ_STAT:
                count = 0;
                node = root->child;
                while (node != NULL) {
                        if (count == MAX_ARITY) {
                                semantic_error(env, node, "maximum arity (%d) exceeded", MAX_ARITY);
                                break;
                        }
                        struct tree_node *var = lhs_variable(node);
                        struct local_position localpos;
                        if (!environment_local_search_check_write(env, var->value, root, &localpos))
                                break;
                        struct semantic_type lhs_type = emit_op_set_local_lhs_type(env, localpos, root->left);
                        if (lhs_type.id == VAL_VECTOR) {
                                semantic_error(env, node, "reading vectors is not supported");
                                break;
                        }
                        emit_read_type(env, node, lhs_type);
                        emit_op_set_local(env, node, localpos, lhs_type);
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
                emit_popv(env, root, emit_expression(env, root->child));
                break;
        case NODE_FUNCTION_DECL:
                emit_function_declaration(env, root);
                break;
        case NODE_EXIT_STAT:
                emit_byte(env, root, OP_HALT);
                break;
        case NODE_BREAK_STAT:
                emit_break(env, root);
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
        struct local_position localpos;
        int codelen;
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
                lefttype = emit_expression(env, root->right);
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
                lefttype = emit_expression(env, root->right);
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
                emit_three_bytes(env, root, OP_EQUA, lefttype.id, lefttype.base);
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_EQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_type_equal(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be of the same type");
                }
                emit_three_bytes(env, root, OP_EQUA, lefttype.id, lefttype.base);
                return booltype;
        case NODE_GREATEREQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_two_bytes(env, root, OP_GRTEQ, lefttype.id);
                return booltype;
        case NODE_GREATER_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_two_bytes(env, root, OP_GRT, lefttype.id);
                return booltype;
        case NODE_LESSEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_two_bytes(env, root, OP_LEQ, lefttype.id);
                return booltype;
        case NODE_LESS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!semantic_types_comparable(lefttype, righttype)) {
                        semantic_error(env, root, "operands must be integers or strings");
                }
                emit_two_bytes(env, root, OP_LT, lefttype.id);
                return booltype;
        case NODE_COND_EXPR:
                return emit_cond_expression(env, root);
        case NODE_BOOLEAN_CONST:
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantic_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_load_constant(env, root, VAL_BOOLEAN, value_from_c_bool(parse_boolean_token(root->value)));
                return booltype;
        case NODE_INTGER_CONST:
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantic_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_load_constant(env, root, VAL_INTEGER, value_from_c_int(parse_integer_token(root->value)));
                return inttype;
        case NODE_STRING_CONST:
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantic_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_load_constant(env, root, VAL_STRING, value_from_token(root->value));
                return strtype;
        case NODE_VECTOR_CONST:
                return emit_vector_constant(env, root, 0);
        case NODE_ID:
                if (!environment_local_search(env, root->value, &localpos)) {
                        semantic_error(env, root, "undefined variable");
                        break;
                }
                emit_op_local_long(env, root, OP_GET_LOCAL_LONG, localpos);
                return environment_local_get(env, localpos).type;
        case NODE_INDEXING:
                return emit_indexing_expression(env, root);
        case NODE_MODULE_CALL:
                return emit_module_call(env, root);
        default:
                semantic_error(env, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
        }
        return inttype;
}

static void
emit_op_local_long(struct environment *env, struct tree_node *node, enum opcode op, struct local_position localpos)
{
        emit_three_bytes(env, node, op, left_byte(localpos.offset), right_byte(localpos.offset));
        emit_two_bytes(env, node, left_byte(localpos.index), right_byte(localpos.index));
}

static void
emit_popv(struct environment *env, struct tree_node *node, struct semantic_type type)
{
        if (type.id == VAL_VECTOR)
                emit_byte(env, node, OP_POPA);
        else
                emit_byte(env, node, OP_POPV);
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
emit_constant(struct environment *env, struct tree_node *root, union value val)
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
emit_load_constant(struct environment *env, struct tree_node *root, enum value_type type, union value val)
{
        enum opcode op;
        switch (type) {
                case VAL_INTEGER: op = OP_LOCI_LONG; break;
                case VAL_BOOLEAN: op = OP_LOCB_LONG; break;
                case VAL_STRING: op = OP_LOCS_LONG; break;
                case VAL_VECTOR: op = OP_LOCV_LONG; break;
                case VAL_FUNCTION: op = OP_LOCF_LONG; break;
        }
        emit_byte(env, root, op);
        emit_constant(env, root, val);
}

static int
emit_unpatched_skip_long(struct environment *env, struct tree_node *root, enum opcode op)
{
    emit_three_bytes(env, root, op, 0, 0);
    return bytes_len(&env->code->code);
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
                if (type.rank == MAX_VECTOR_DIMENSIONS - 1) {
                        semantic_error(env, node, "maximum vector rank exceeded");
                } else {
                        type.size *= inside.size;
                        for (int i = 0; i < inside.rank; i++) {
                                type.dimensions[type.rank++] = inside.dimensions[i];
                        }
                }
        }
        return type; 
}

static void
environment_init(struct environment *env, struct environment *parent, struct bytecode *code)
{
        env->code = code;
        env->parent = parent;
        locals_init(&env->locals);
        env->error = 0;
        env->depth = 0;
        env->panic = 0;
        break_likes_init(&env->break_likes);
        env->loopdepth = 0;
        arg_types_init(&env->arg_types);
}

static void
environment_free(struct environment *env)
{
        locals_free(&env->locals);
        break_likes_free(&env->break_likes);
        arg_types_free(&env->arg_types);
}

static int
environment_local_search_helper(struct environment *env, struct token name, struct local_position *localpos, int currenvoffset) {
        if (env == NULL)
                return 0;
        int i;
        for (i = locals_len(&env->locals) - 1; i >= 0; i--) {
                if (token_equal(locals_at(&env->locals, i).name, name)) {
                        break;
                }
        }
        if (localpos) {
                localpos->offset = currenvoffset;
                localpos->index = i;
        }
        if (i < 0)
                return environment_local_search_helper(env->parent, name, localpos, currenvoffset + 1);
        return 1;
}

static int
environment_local_search(struct environment *env, struct token name, struct local_position *localpos)
{
        return environment_local_search_helper(env, name, localpos, 0);
}

struct local
environment_local_get(struct environment *env, struct local_position localpos)
{
        if (localpos.offset == 0)
                return locals_at(&env->locals, localpos.index);
        localpos.offset--;
        return environment_local_get(env->parent, localpos);
}

static void
emit_push_scope(struct environment *env, struct tree_node *node)
{
        env->depth++;
}

static void
emit_pop_scope(struct environment *env, struct tree_node *node)
{
        while (locals_len(&env->locals) > 0 && locals_at(&env->locals, locals_len(&env->locals) - 1).depth == env->depth) {
                emit_popv(env, node, locals_at(&env->locals, locals_len(&env->locals) - 1).type);
                locals_pop(&env->locals);
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
                emit_byte(env, node, OP_LOC_ALINK_LONG);
                union value val;
                val.vector.size = type.size;
                emit_constant(env, node, val);
                break;
        }
        }
}

static void
emit_read_type(struct environment *env, struct tree_node *node, struct semantic_type lhs_type)
{
        emit_byte(env, node, OP_READ);
        emit_byte(env, node, lhs_type.id);
}

static void
push_loop(struct environment *env)
{
        env->loopdepth++;
}

static void
pop_loop(struct environment *env)
{
        while (break_likes_len(&env->break_likes) > 0 && break_likes_at(&env->break_likes, break_likes_len(&env->break_likes) - 1).loopdepth == env->loopdepth) {
                break_likes_pop(&env->break_likes);
        }
        env->loopdepth--;
}

static void
emit_break(struct environment *env, struct tree_node *node)
{
        if (env->loopdepth == 0) {
                semantic_error(env, node, "cannot use break outside a loop");
                return;
        }
        struct break_like br;
        br.codelen = emit_unpatched_skip_long(env, node, OP_SKIP_LONG);
        br.loopdepth = env->loopdepth;
        break_likes_push(&env->break_likes, br);
}

static void
patch_breaks(struct environment *env, struct tree_node *root)
{
        for (int i = break_likes_len(&env->break_likes) - 1; i >= 0 && break_likes_at(&env->break_likes, i).loopdepth == env->loopdepth; i--) {
                patch_skip_long(env, root, break_likes_at(&env->break_likes, i).codelen);
        }
}

static void
init_local(struct local *loc, struct token name, struct semantic_type type, int depth, uint8_t perms)
{
        loc->name = name;
        loc->type = type;
        loc->perms = perms;
        loc->depth = depth;
}

static struct local_position
environment_local_push(struct environment *env, struct local topush)
{
        struct local_position toret;
        toret.offset = 0;
        toret.index = locals_push(&env->locals, topush) - 1;
        return toret;
}

static int
declare_local_in_env(struct environment *env, struct tree_node *current, struct semantic_type type, uint8_t perms, struct local_position *localpos)
{
        if (locals_len(&env->locals) == MAX_LOCALS) {
                semantic_error(env, current, "maximum number of local variables exceeded");
                return 0;
        }
        struct local_position localpostmp;
        if (environment_local_search(env, current->value, &localpostmp) && localpostmp.offset == 0 && environment_local_get(env, localpostmp).depth == env->depth) {
                semantic_error(env, current, "variable already declared");
                return 0;
        }
        struct local topush;
        init_local(&topush, current->value, type, env->depth, perms);
        localpostmp = environment_local_push(env, topush);
        if (localpos)
                *localpos = localpostmp;
        return 1;
}

static int
emit_declare_local_default(struct environment *env, struct tree_node *current, struct semantic_type type, uint8_t perms, struct local_position *localpos)
{
        if (!declare_local_in_env(env, current, type, perms, localpos))
                return 0;
        emit_variable_default(env, current, type);
        return 1;
}

static void
emit_if_statement(struct environment *env, struct tree_node *root)
{
        int toendlens[MAX_CONDITIONAL_LEN];
        int codelen, *toendp;
        toendp = toendlens;
        struct tree_node *child;
        struct semantic_type type1;
        child = root->child;
        while (child != NULL && child->type == NODE_CONDITION_AND_STATEMENT) {
                type1 = emit_expression(env, child->left);
                if (type1.id != VAL_BOOLEAN) {
                        semantic_error(env, child->left, "if condition must be boolean");
                        return;
                }
                codelen = emit_unpatched_skip_long(env, child->left, OP_SKIPF_LONG);
                emit_byte(env, child->left, OP_POPV);
                emit_statement(env, child->right);
                if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                        semantic_error(env, child, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
                        return;
                }
                *toendp++ = emit_unpatched_skip_long(env, child, OP_SKIP_LONG);
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
        push_loop(env);

        int codelen, startlen;
        struct semantic_type type1;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);
        type1 = emit_expression(env, root->left);
        if (type1.id != VAL_BOOLEAN) {
                semantic_error(env, root->left, "while condition must be boolean");
                return;
        }
        codelen = emit_unpatched_skip_long(env, root->left, OP_SKIPF_LONG);
        emit_byte(env, root->left, OP_POPV);
        emit_statement(env, root->right);
        emit_skip_back_long(env, root->right, startlen);
        patch_skip_long(env, root, codelen);
        emit_byte(env, root->left, OP_POPV);

        patch_breaks(env, root);

        pop_loop(env);
}

static void
emit_repeat_statement(struct environment *env, struct tree_node *root)
{
        push_loop(env);

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

        patch_breaks(env, root);

        pop_loop(env);
}

static int
environment_local_search_check_write(struct environment *env, struct token name, struct tree_node *root, struct local_position *localpos)
{
        struct local_position localpostmp;
        if (!environment_local_search(env, name, &localpostmp)) {
                semantic_error(env, root->left, "undefined variable");
                return 0;
        }
        struct local loc = environment_local_get(env, localpostmp);
        if ((loc.perms & LOCAL_PERM_W) == 0) {
                semantic_error(env, root->left, "cannot assign read-only variable");
                return 0;
        }
        if (localpos)
                *localpos = localpostmp;
        return 1;
}

static struct semantic_type
emit_op_set_local_lhs_type(struct environment *env, struct local_position localpos, struct tree_node *lhs)
{
        struct local local = environment_local_get(env, localpos);
        struct semantic_type toret;
        switch (local.type.id) {
        case VAL_VECTOR:
                toret = emit_indexing_prelude(env, local.type, lhs);
                break;
        default:
                toret = local.type;
                break;
        }
        return toret;
}

static struct semantic_type
emit_indexing_prelude(struct environment *env, struct semantic_type indexed_type, struct tree_node *indexing_node)
{
        struct tree_node *indices_node = indexing_node->right;
        int index_count = 0;

        /* emit indices */
        for (struct tree_node *node = indices_node; node != NULL; node = node->next) {
                index_count++;
                if (emit_expression(env, node).id != VAL_INTEGER) {
                        semantic_error(env, node, "cannot index array with non integer");
                        break;
                }
        }

        /* emit dimensions */
        for (int i = 0; i < indexed_type.rank; i++) {
                emit_two_bytes(env, indexing_node, OP_PUSH_BYTE, indexed_type.dimensions[i]);
        }

        return compute_indexed_semantic_type(index_count, indexed_type);
}

static void
emit_op_set_local(struct environment *env, struct tree_node *node, struct local_position localpos, struct semantic_type rhs_type)
{
        struct local loc = environment_local_get(env, localpos);
        switch (loc.type.id) {
                case VAL_VECTOR:
                        emit_op_local_long(env, node, OP_SET_INDEX_LOCAL_LONG, localpos);
                        emit_byte(env, node, loc.type.rank - rhs_type.rank);
                        emit_byte(env, node, loc.type.rank);
                        break;
                default:
                        emit_op_local_long(env, node, OP_SET_LOCAL_LONG, localpos);
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

        struct local_position localpos;
        if (!environment_local_search_check_write(env, var->value, root, &localpos))
                return;

        right_type = emit_expression(env, rhs);

        left_type = emit_op_set_local_lhs_type(env, localpos, lhs);

        if (!semantic_type_equal(left_type, right_type)) {
                semantic_error(env, root, "mismatching types in assignment (%s = %s)", value_type_to_string(left_type.id), value_type_to_string(right_type.id));
        }

        emit_op_set_local(env, lhs, localpos, right_type);
}

static void
emit_for_statement(struct environment *env, struct tree_node *root)
{
        push_loop(env);

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
        struct local_position incpos;
        emit_declare_local_default(env, assign->left, inttype, LOCAL_PERM_RW, &incpos);
        emit_assign_statement(env, assign);
        env->locals.buffer[locals_len(&env->locals) - 1].perms = LOCAL_PERM_R;

        struct local_position forcondpos;
        emit_declare_local_default(env, &forcond_node, inttype, LOCAL_PERM_R, &forcondpos);
        struct semantic_type type1;
        type1 = emit_expression(env, condition->right);
        if (type1.id != VAL_INTEGER) {
                semantic_error(env, condition->right, "for loop upper range must be an integer");
                return;
        }
        emit_op_set_local(env, &forcond_node, forcondpos, inttype);

        int codelen, startlen;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);
        emit_op_local_long(env, root, OP_GET_LOCAL_LONG, incpos);
        emit_op_local_long(env, root, OP_GET_LOCAL_LONG, forcondpos);

        emit_two_bytes(env, condition, OP_LEQ, VAL_INTEGER);
        codelen = emit_unpatched_skip_long(env, condition, OP_SKIPF_LONG);
        emit_byte(env, condition, OP_POPV);
        emit_statement(env, statlist);
        emit_op_local_long(env, root, OP_GET_LOCAL_LONG, incpos);
        emit_byte(env, root, OP_ONE);
        emit_byte(env, root, OP_ADDI);
        emit_op_set_local(env, root, incpos, inttype);
        emit_skip_back_long(env, statlist, startlen);
        patch_skip_long(env, root, codelen);
        emit_byte(env, condition, OP_POPV);

        patch_breaks(env, root);

        emit_pop_scope(env, root);

        pop_loop(env);
}

static void
emit_function_declaration(struct environment *env, struct tree_node *root)
{
        struct tree_node *function_types_node = root->right;
        struct tree_node *var_decls_node = function_types_node->left;
        struct tree_node *function_name_node = root->left;
        struct tree_node *return_type_node = function_types_node->right;
        struct tree_node *statements_node = root->child;

        struct semantic_type fntype = build_function_semantic_type(env, root);

        struct environment subenv;
        struct bytecode *subcode = malloc(sizeof(struct bytecode));
        bytecode_init(subcode);
        environment_init(&subenv, env, subcode);

        declare_local_in_env(&subenv, function_name_node, fntype, LOCAL_PERM_R, NULL);

        for (struct tree_node *node = var_decls_node; node != NULL; node = node->next) {
                struct tree_node *p = node->left->child;
                while (p != NULL) {
                        if (!declare_local_in_env(&subenv, p, type_node_to_type(env, node->right), LOCAL_PERM_R, NULL))
                                break;
                        p = p->next;
                }
        }

        emit_body(&subenv, statements_node, return_type_node, fntype.rank);

        env->error = env->error || subenv.error;
        env->panic = env->panic || subenv.panic;

        environment_free(&subenv);

        union value fnval;
        fnval.function.code = subcode;
        declare_local_in_env(env, function_name_node, fntype, LOCAL_PERM_R, NULL);
        emit_load_constant(env, root, VAL_FUNCTION, fnval);
}

static void
emit_body(struct environment *env, struct tree_node *statements_node, struct tree_node *return_type_node, int arity)
{
        for (struct tree_node *node = statements_node->child; node != NULL; node = node->next) {
                if (node->type != NODE_RETURN_STAT) {
                        emit_statement(env, node);
                        continue;
                }
                struct tree_node *ret_expr = node->child;
                if (return_type_node != NULL) {
                        struct semantic_type return_type = type_node_to_type(env, return_type_node);
                        struct semantic_type actual_ret_type = emit_expression(env, ret_expr);
                        if (!semantic_type_equal(return_type, actual_ret_type)) {
                                semantic_error(env, node, "mismatching return type in function");
                                return;
                        }
                        if (return_type.id == VAL_VECTOR)
                                emit_byte(env, node, OP_SHIFT_ASTACKENT_TO_BASE);
                } else {
                        emit_byte(env, node, OP_ZERO);
                }
                emit_three_bytes(env, node, OP_RETURN, arity, return_type_node ? 1 : 0);
        }
}

static struct semantic_type
emit_module_call(struct environment *env, struct tree_node *root)
{
        struct tree_node *called = root->left;
        struct semantic_type called_type;
        struct semantic_type dummy = semantic_type_scalar(VAL_INTEGER);

        called_type = emit_expression(env, called);
        if (called_type.id != VAL_FUNCTION) {
                semantic_error(env, called, "cannot call non callable variable");
                return dummy;
        }
        if (called_type.ret_type_index < 0) {
                semantic_error(env, called, "cannot use a procedure as an expression");
                return dummy;
        }
        int argcount = 0;
        for (struct tree_node *expr_node = root->right; expr_node != NULL; expr_node = expr_node->next) {
                argcount++;
                if (argcount > called_type.rank)
                        break;
                struct semantic_type expr_type = emit_expression(env, expr_node);
                if (!semantic_type_equal(semantic_type_argument_at(called_type, argcount - 1), expr_type)) {
                        semantic_error(env, expr_node, "mismatching argument type");
                }
        }
        if (argcount != called_type.rank)
                semantic_error(env, root, "wrong number of arguments");
        emit_two_bytes(env, root, OP_CALL, called_type.rank);
        return semantic_type_return_value(called_type);
}

static struct semantic_type
build_function_semantic_type(struct environment *env, struct tree_node *root)
{
        struct tree_node *function_types_node = root->right;
        struct tree_node *var_decls_node = function_types_node->left;
        struct tree_node *return_type_node = function_types_node->right;

        struct semantic_type fntype;
        fntype.id = VAL_FUNCTION;
        fntype.arg_types = &env->arg_types;
        fntype.ret_type_index = -1;
        if (return_type_node) {
                arg_types_push(&env->arg_types, type_node_to_type(env, return_type_node));
                fntype.ret_type_index = arg_types_len(&env->arg_types) - 1;
        }
        fntype.param_types_start_index = arg_types_len(&env->arg_types);

        fntype.rank = 0;
        for (struct tree_node *var_decl = var_decls_node; var_decl != NULL; var_decl = var_decl->next) {
                struct tree_node *node = var_decl->left->child;
                while (node != NULL) {
                        arg_types_push(&env->arg_types, type_node_to_type(env, var_decl->right));
                        fntype.rank++;
                        if (fntype.rank > MAX_ARITY) {
                                semantic_error(env, node, "max arity exceeded");
                                break;
                        }
                        node = node->next;
                }
        }

        return fntype;
}

static struct semantic_type
emit_cond_expression(struct environment *env, struct tree_node *root)
{
        int toendlens[MAX_CONDITIONAL_LEN];
        int codelen, *toendp;
        toendp = toendlens;
        struct tree_node *child;
        struct semantic_type type0, type1;
        child = root->child;
        while (child != NULL && child->type == NODE_CONDITION_AND_EXPRESSION) {
                type1 = emit_expression(env, child->left);
                if (type1.id != VAL_BOOLEAN) {
                        semantic_error(env, child->left, "if condition must be boolean");
                        return type0;
                }
                codelen = emit_unpatched_skip_long(env, child->left, OP_SKIPF_LONG);
                emit_byte(env, child->left, OP_POPV);
                type1 = emit_expression(env, child->right);
                if (child == root->child)
                        type0 = type1;
                if (type0.id != type1.id) {
                        semantic_error(env, child, "conditional expression types must be the same");
                        return type0;
                }
                if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                        semantic_error(env, child, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
                        return type0;
                }
                *toendp++ = emit_unpatched_skip_long(env, child, OP_SKIP_LONG);
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
        struct tree_node *indexed = root->left;
        struct semantic_type indexed_type;

        indexed_type = emit_expression(env, indexed);
        if (indexed_type.id != VAL_VECTOR) {
                semantic_error(env, indexed, "cannot index a non vector");
        }

        struct semantic_type toret = emit_indexing_prelude(env, indexed_type, root);

        emit_three_bytes(env, root, OP_GET_INDEX, indexed_type.rank - toret.rank, indexed_type.rank);

        return toret;
}

static struct semantic_type
compute_indexed_semantic_type(int index_count, struct semantic_type indexed_type)
{
        struct semantic_type toret;
        if (index_count == indexed_type.rank) {
                toret = semantic_type_scalar(indexed_type.base);
        } else {
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

        struct semantic_type type = emit_vector_constant(env, root->child, depth + 1);
        toret.id = VAL_VECTOR;
        toret.rank = type.rank + 1;
        memcpy(toret.dimensions + 1, type.dimensions, MAX_VECTOR_DIMENSIONS);
        toret.base = type.base;
        toret.size = type.size;
        toret.dimensions[0] = 1;

        for (struct tree_node *node = root->child->next; node != NULL; node = node->next) {
                struct semantic_type current_type = emit_vector_constant(env, node, depth + 1);
                if (!semantic_type_equal(type, current_type)) {
                        semantic_error(env, node, "vector elements must be homogeneous");
                        break;
                }
                toret.size += type.size;
                toret.dimensions[0]++;
        } 

        if (depth != 0)
                return toret;

        emit_byte(env, root, OP_LOC_ALINK_LONG);
        union value val;
        val.vector.astackent = NULL;
        val.vector.size = toret.size;
        emit_constant(env, root, val);

        return toret;
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
        case OP_CALL: return "OP_CALL";
        case OP_DIVI: return "OP_DIVI";
        case OP_EMPTY_STRING: return "OP_EMPTY_STRING";
        case OP_EQUA: return "OP_EQUA";
        case OP_FALSE: return "OP_FALSE";
        case OP_GET_INDEX: return "OP_GET_INDEX";
        case OP_GET_LOCAL_LONG: return "OP_GET_LOCAL_LONG";
        case OP_GRTEQ: return "OP_GRTEQ";
        case OP_GRT: return "OP_GRT";
        case OP_HALT: return "OP_HALT";
        case OP_LEQ: return "OP_LEQ";
        case OP_LOC_ALINK_LONG: return "OP_LOC_ALINK_LONG";
        case OP_LOCB_LONG: return "OP_LOCB_LONG";
        case OP_LOCF_LONG: return "OP_LOCF_LONG";
        case OP_LOCI_LONG: return "OP_LOCI_LONG";
        case OP_LOCS_LONG: return "OP_LOCS_LONG";
        case OP_LOCV_LONG: return "OP_LOCV_LONG";
        case OP_LT: return "OP_LT";
        case OP_MULI: return "OP_MULI";
        case OP_NEWLINE: return "OP_NEWLINE";
        case OP_NOT: return "OP_NOT";
        case OP_ONE: return "OP_ONE";
        case OP_POPA: return "OP_POPA";
        case OP_POP_TO_ASTACK: return "OP_POP_TO_ASTACK";
        case OP_POPV: return "OP_POPV";
        case OP_PUSH_BYTE: return "OP_PUSH_BYTE";
        case OP_READ: return "OP_READ";
        case OP_RETURN: return "OP_RETURN";
        case OP_SET_INDEX_LOCAL_LONG: return "OP_SET_INDEX_LOCAL_LONG";
        case OP_SET_LOCAL_LONG: return "OP_SET_LOCAL_LONG";
        case OP_SHIFT_ASTACKENT_TO_BASE: return "OP_SHIFT_ASTACKENT_TO_BASE";
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
disassemble_constant(struct bytecode *code, int ip, enum opcode loctype, int indentation)
{
        uint8_t constantaddr_left = bytes_at(&code->code, ip++);
        uint8_t constantaddr_right = bytes_at(&code->code, ip++);
        uint16_t constantaddr = join_bytes(constantaddr_left, constantaddr_right);
        printf("%d ", constantaddr);
        union value v = bytecode_constant_at(code, constantaddr);
        printf("(");
        switch (loctype) {
                case OP_LOCI_LONG:
                        value_print(v, VAL_INTEGER, VAL_INTEGER);
                        break;
                case OP_LOCB_LONG:
                        value_print(v, VAL_BOOLEAN, VAL_BOOLEAN);
                        break;
                case OP_LOCS_LONG:
                        value_print(v, VAL_STRING, VAL_STRING);
                        break;
                case OP_LOCF_LONG:
                        printf("\n");
                        disassemble_helper(v.function.code, indentation + 1);
                        break;
                default:
                        break;
        }
        printf(")");
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
disassemble_helper(struct bytecode *code, int indentation)
{
        int ip = 0;
        while (code && ip < bytes_len(&code->code)) {
                for (int i = 0; i < indentation; i++) {
                        printf("\t");
                }
                uint8_t instruction = bytes_at(&code->code, ip);
                printf("%d: %s ", ip, opcodestring(instruction));
                ip++;
                switch (instruction) {
                case OP_LOCI_LONG:
                case OP_LOCB_LONG:
                case OP_LOCS_LONG:
                case OP_LOCV_LONG:
                case OP_LOC_ALINK_LONG:
                case OP_LOCF_LONG:
                        ip = disassemble_constant(code, ip, instruction, indentation);
                        break;
                case OP_SKIP_BACK_LONG:
                case OP_SKIP_LONG:
                case OP_SKIPF_LONG:
                        ip = disassemble_argument_long(code, ip);
                        break;
                case OP_GET_LOCAL_LONG:
                case OP_SET_LOCAL_LONG:
                        ip = disassemble_argument_long(code, ip);
                        ip = disassemble_argument_long(code, ip);
                        break;
                case OP_LT:
                case OP_LEQ:
                case OP_GRT:
                case OP_GRTEQ:
                case OP_PUSH_BYTE:
                case OP_WRITE:
                case OP_CALL:
                        ip = disassemble_argument(code, ip);
                        break;
                case OP_READ:
                case OP_GET_INDEX:
                case OP_EQUA:
                case OP_RETURN:
                        ip = disassemble_argument(code, ip);
                        ip = disassemble_argument(code, ip);
                        break;
                case OP_SET_INDEX_LOCAL_LONG:
                        ip = disassemble_argument_long(code, ip);
                        ip = disassemble_argument_long(code, ip);
                        ip = disassemble_argument(code, ip);
                        ip = disassemble_argument(code, ip);
                        break;
                default:
                        break;
                }
                disassemble_lineinfo(code, ip);
                printf("\n");
        }
}

void
disassemble(struct bytecode *code)
{
        disassemble_helper(code, 0);
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
