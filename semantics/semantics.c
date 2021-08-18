#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantics.h"

static struct type emit_cond_expr(struct environment *env, struct tree_node *root);
static void emit_for_statement(struct environment *env, struct tree_node *root);
static void emit_assign_statement(struct environment *env, struct tree_node *root);
static void emit_repeat_statement(struct environment *env, struct tree_node *root);
static void emit_while_statement(struct environment *env, struct tree_node *root);
static void emit_if_statement(struct environment *env, struct tree_node *root);
static int emit_declare_local(struct environment *env, struct tree_node *current, struct type type, uint8_t perms);
static void emit_variable_default(struct environment *env, struct tree_node *node, struct type type);
static void emit_pop_scope(struct environment *env, struct tree_node *node);
static void emit_push_scope(struct environment *env, struct tree_node *node);
static int emit_skip_back_long(struct environment *env, struct tree_node *root, int codelen);
static void emit_constant(struct environment *env, struct tree_node *root, struct value val);
static struct type emit_vector_constant(struct environment *env, struct tree_node *root, int depth);
static void emit_byte(struct environment *env, struct tree_node *root, uint8_t byte);
static void emit_two_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1);
static void emit_three_bytes(struct environment *env, struct tree_node *root, uint8_t byte0, uint8_t byte1, uint8_t byte2);
static int patch_skip_long(struct environment *env, struct tree_node *root, int codelen);
static void environment_init(struct environment *env, struct bytecode *code);
static int environment_local_get(struct environment *env, struct token name, struct local *local);
static void init_local(struct local *loc, struct token name, struct type type, int depth, uint8_t perms);
static uint8_t right_byte(uint16_t word);
static uint8_t left_byte(uint16_t word);
static int parse_boolean_token(struct token token);
static int parse_integer_token(struct token token);
static struct type type_node_to_type(struct environment *env, struct tree_node *node);
static struct type vector_type_node_to_type(struct environment *env, struct tree_node *node);
static void semantics_error(struct environment *env, struct tree_node *root, char *fmt, ...);

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
                return;
        case NODE_VAR_DECL:
                node = root->left->child;
                while (node != NULL) {
                        emit_declare_local(env, node, type_node_to_type(env, root->right), LOCAL_PERM_RW);
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
                emit_assign_statement(env, root);
                return;
        case NODE_IF_STAT:
                emit_if_statement(env, root);
                return;
        case NODE_WHILE_STAT:
                emit_while_statement(env, root);
                return;
        case NODE_REPEAT_STAT:
                emit_repeat_statement(env, root);
                return;
        case NODE_FOR_STAT:
                emit_for_statement(env, root);
                return;
        case NODE_EXPR_STAT:
                emit_expression(env, root->child);
                emit_byte(env, root, OP_POPV);
                return;
        default:
                semantics_error(env, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
        }
}

struct type
emit_expression(struct environment *env, struct tree_node *root)
{
        struct type lefttype, righttype;
        struct type inttype, booltype, strtype;
        struct local local;
        struct bytecode *code = env->code;
        int codelen, localindex;
        inttype = scalar_type(VAL_INTEGER);
        booltype = scalar_type(VAL_BOOLEAN);
        strtype = scalar_type(VAL_STRING);
        booltype.type = VAL_BOOLEAN;
        strtype.type = VAL_STRING;
        switch (root->type) {
        case NODE_AND_EXPR:
                lefttype = emit_expression(env, root->left);
                emit_three_bytes(env, root, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, root, OP_POPV);
                righttype = emit_expression(env, root->right);
                if (lefttype.type != VAL_BOOLEAN || righttype.type != VAL_BOOLEAN) {
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
                if (lefttype.type != VAL_BOOLEAN || righttype.type != VAL_BOOLEAN) {
                        semantics_error(env, root, "operands must be booleans");
                }
                patch_skip_long(env, root, codelen);
                return booltype;
        case NODE_NOT_EXPR:
                lefttype = emit_expression(env, root->left);
                if (lefttype.type != VAL_BOOLEAN) {
                        semantics_error(env, root, "operand must be a boolean");
                }
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_PLUS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.type != VAL_INTEGER || righttype.type != VAL_INTEGER) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_ADDI);
                return inttype;
        case NODE_MINUS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.type != VAL_INTEGER || righttype.type != VAL_INTEGER) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_SUBI);
                return inttype;
        case NODE_TIMES_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.type != VAL_INTEGER || righttype.type != VAL_INTEGER) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_MULI);
                return inttype;
        case NODE_DIVIDE_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (lefttype.type != VAL_INTEGER || righttype.type != VAL_INTEGER) {
                        semantics_error(env, root, "operands must be integers");
                }
                emit_byte(env, root, OP_DIVI);
                return inttype;
        case NODE_NEG_EXPR:
                emit_byte(env, root, OP_ZERO);
                lefttype = emit_expression(env, root->left);
                if (lefttype.type != VAL_INTEGER) {
                        semantics_error(env, root, "operand must be an integer");
                }
                emit_byte(env, root, OP_SUBI);
                return inttype;
        case NODE_NEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!type_equal(lefttype, righttype)) {
                        semantics_error(env, root, "operands must be of the same type");
                }
                emit_byte(env, root, OP_EQUA);
                emit_byte(env, root, OP_NOT);
                return booltype;
        case NODE_EQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!type_equal(lefttype, righttype)) {
                        semantics_error(env, root, "operands must be of the same type");
                }
                emit_byte(env, root, OP_EQUA);
                return booltype;
        case NODE_GREATEREQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantics_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_IGRTEQ);
                return booltype;
        case NODE_GREATER_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantics_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_IGRT);
                return booltype;
        case NODE_LESSEQ_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantics_error(env, root, "operands must be integers or strings");
                }
                emit_byte(env, root, OP_ILEQ);
                return booltype;
        case NODE_LESS_EXPR:
                lefttype = emit_expression(env, root->left);
                righttype = emit_expression(env, root->right);
                if (!types_comparable(lefttype, righttype)) {
                        semantics_error(env, root, "operands must be integers or strings");
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
        case NODE_STRING_CONST:
                emit_byte(env, root, OP_LOCS);
                if (valuelist_len(&code->constants) >= MAX_CONSTANTS) {
                        semantics_error(env, root, "maximum number of constants (%d) exceeded", MAX_CONSTANTS);
                }
                emit_constant(env, root, value_from_token(root->value));
                return strtype;
        case NODE_VECTOR_CONST:
                return emit_vector_constant(env, root, 0);
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
                semantics_error(env, root, "max skip size (%d) exceeded", MAX_SKIP_LONG);
                return 0;
        }
        jumplenfst = left_byte(jumplen);
        jumplenscn = right_byte(jumplen);
        code->code.buffer[bytes_len(&code->code) - 2] = jumplenfst;
        code->code.buffer[bytes_len(&code->code) - 1] = jumplenscn;
        return 1;
}

static struct type
type_node_to_type(struct environment *env, struct tree_node *node)
{
        struct type type;
        switch (node->type) {
        case NODE_STRING_TYPE:
                type = scalar_type(VAL_STRING);
                break;
        case NODE_INTEGER_TYPE:
                type = scalar_type(VAL_INTEGER);
                break;
        case NODE_BOOLEAN_TYPE:
                type = scalar_type(VAL_BOOLEAN);
                break;
        case NODE_VECTOR_TYPE:
                return vector_type_node_to_type(env, node);
        default:
                printf("unreachable code in type_node_to_type\n");
        }
        return type;
}

static struct type
vector_type_node_to_type(struct environment *env, struct tree_node *node)
{
        struct type type;
        type.type = VAL_VECTOR;
        type.size = parse_integer_token(node->left->value);
        type.dimensions[0] = type.size;
        type.rank = 1;

        struct type inside = type_node_to_type(env, node->right);
        type.base = inside.type;
        if (inside.type == VAL_VECTOR) {
                for (int i = 0; i < inside.rank; i++) {
                        if (type.rank == MAX_VECTOR_DIMENSIONS - 1) {
                                semantics_error(env, node, "maximum vector rank exceeded");
                                break;
                        }
                        type.dimensions[type.rank++] = inside.dimensions[i];
                        type.size += inside.dimensions[i];
                }
                type.base = inside.base;
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
}

static int
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
emit_variable_default(struct environment *env, struct tree_node *node, struct type type)
{
        switch (type.type) {
        case VAL_BOOLEAN:
                emit_byte(env, node, OP_LOCI);
                emit_constant(env, node, value_from_c_bool(0));
                break;
        case VAL_INTEGER:
                emit_byte(env, node, OP_LOCI);
                emit_constant(env, node, value_from_c_int(0));
                break;
        case VAL_STRING:
                emit_byte(env, node, OP_LOCS);
                emit_constant(env, node, value_from_c_string(""));
                break;
        case VAL_VECTOR: {
                emit_byte(env, node, OP_LOCV);
                struct value val;
                val.type = type;
                val.as.vector.size = type.size;
                emit_constant(env, node, val);
                break;
        }
        }
}

static void
init_local(struct local *loc, struct token name, struct type type, int depth, uint8_t perms)
{
        loc->name = name;
        loc->type = type;
        loc->perms = perms;
        loc->depth = depth;
}

static int
emit_declare_local(struct environment *env, struct tree_node *current, struct type type, uint8_t perms)
{
        if (env->count == MAX_LOCALS) {
                semantics_error(env, current, "maximum number of local variables exceeded");
                return 0;
        }
        struct local local;
        if (environment_local_get(env, current->value, &local) >= 0 && local.depth == env->depth) {
                semantics_error(env, current, "variable already declared");
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
        struct type type1;
        struct bytecode *code = env->code;
        child = root->child;
        while (child != NULL && child->type == NODE_CONDITION_AND_STATEMENT) {
                type1 = emit_expression(env, child->left);
                if (type1.type != VAL_BOOLEAN) {
                        semantics_error(env, child->left, "if condition must be boolean");
                        return;
                }
                emit_three_bytes(env, child->left, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, child->left, OP_POPV);
                emit_statement(env, child->right);
                emit_three_bytes(env, child, OP_SKIP_LONG, 0, 0);
                if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                        semantics_error(env, child, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
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
        struct type type1;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);
        type1 = emit_expression(env, root->left);
        if (type1.type != VAL_BOOLEAN) {
                semantics_error(env, root->left, "while condition must be boolean");
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
        struct type type1;
        struct bytecode *code = env->code;
        startlen = bytes_len(&code->code);

        emit_statement(env, root->left);

        type1 = emit_expression(env, root->right);
        if (type1.type != VAL_BOOLEAN) {
                semantics_error(env, root->right, "until condition must be boolean");
                return;
        }

        emit_three_bytes(env, root->right, OP_SKIPF_LONG, 0, 1);
        bytes_len(&code->code);
        emit_skip_back_long(env, root->right, startlen);
}

static void
emit_assign_statement(struct environment *env, struct tree_node *root)
{
        struct type righttype;
        int localindex;
        struct local local;

        righttype = emit_expression(env, root->right);
        /* if not indexing */
        localindex = environment_local_get(env, root->left->value, &local);
        if (localindex < 0) {
                semantics_error(env, root, "undefined variable");
        }
        if (!env->error && (local.perms & LOCAL_PERM_W) == 0) {
                semantics_error(env, root, "cannot assign read-only variable");
        }
        if (!env->error && local.type.type != righttype.type) {
                semantics_error(env, root, "mismatching types in assignment");
        }
        emit_three_bytes(env, root, OP_SET_LOCAL_LONG, left_byte(localindex), right_byte(localindex));
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

        struct type inttype;
        inttype.type = VAL_INTEGER;
        int incindex = emit_declare_local(env, assign->left, inttype, LOCAL_PERM_RW);
        emit_assign_statement(env, assign);
        env->locals[env->count - 1].perms = LOCAL_PERM_R;

        int forcond_index = emit_declare_local(env, &forcond_node, inttype, LOCAL_PERM_R);
        struct type type1;
        type1 = emit_expression(env, condition->right);
        if (type1.type != VAL_INTEGER) {
                semantics_error(env, condition->right, "for loop upper range must be an integer");
                return;
        }
        emit_three_bytes(env, &forcond_node, OP_SET_LOCAL_LONG, left_byte(forcond_index), right_byte(forcond_index));

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
        emit_three_bytes(env, root, OP_SET_LOCAL_LONG, left_byte(incindex), right_byte(incindex));
        emit_skip_back_long(env, statlist, startlen);
        emit_byte(env, condition, OP_POPV);
        patch_skip_long(env, root, codelen);

        emit_pop_scope(env, root);
}

static struct type
emit_cond_expr(struct environment *env, struct tree_node *root)
{
        int toendlens[MAX_CONDITIONAL_LEN];
        int codelen, *toendp;
        toendp = toendlens;
        struct tree_node *child;
        struct type type0, type1;
        struct bytecode *code = env->code;
        child = root->child;
        while (child != NULL && child->type == NODE_CONDITION_AND_EXPRESSION) {
                type1 = emit_expression(env, child->left);
                if (type1.type != VAL_BOOLEAN) {
                        semantics_error(env, child->left, "if condition must be boolean");
                        return type0;
                }
                emit_three_bytes(env, child->left, OP_SKIPF_LONG, 0, 0);
                codelen = bytes_len(&code->code);
                emit_byte(env, child->left, OP_POPV);
                type1 = emit_expression(env, child->right);
                if (child == root->child)
                        type0 = type1;
                if (type0.type != type1.type) {
                        semantics_error(env, child, "conditional expression types must be the same");
                        return type0;
                }
                emit_three_bytes(env, child, OP_SKIP_LONG, 0, 0);
                if (toendp - toendlens > MAX_CONDITIONAL_LEN) {
                        semantics_error(env, child, "maximum if-elsif chain (%d) exceeded", MAX_CONDITIONAL_LEN);
                        return type0;
                }
                *toendp++ = bytes_len(&code->code);
                patch_skip_long(env, child, codelen);
                emit_byte(env, child, OP_POPV);
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

static struct type
emit_vector_constant(struct environment *env, struct tree_node *root, int depth)
{
        struct type toret;
        if (root->type != NODE_VECTOR_CONST) {
                toret = emit_expression(env, root);
                emit_byte(env, root, OP_POP_TO_ASTACK);
                return toret;
        }

        struct type type;
        type.base = VAL_INTEGER;
        int size = 0;
        if (root->child != NULL) {
                type = emit_vector_constant(env, root->child, depth + 1);
                toret.rank = type.rank + 1;
                memcpy(toret.dimensions, type.dimensions, sizeof(toret.dimensions));
                toret.base = type.base;
                size = type.size;
                toret.dimensions[toret.rank - 1] = 1;

                for (struct tree_node *node = root->child->next; node != NULL; node = node->next) {
                        struct type current_type = emit_vector_constant(env, node, depth + 1);
                        if (!type_equal(type, current_type)) {
                                semantics_error(env, node, "vector elements must be homogeneous");
                                break;
                        }
                        size += type.size;
                        toret.dimensions[toret.rank - 1]++;
                }       
        }

        toret.type = VAL_VECTOR;
        toret.size = size;

        if (depth != 0)
                return toret;

        emit_byte(env, root, OP_END_VEC);
        struct value val;
        val.type = toret;
        val.as.vector.size = size;
        val.as.vector.astackent = NULL;
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
        case OP_DIVI: return "OP_DIVI";
        case OP_END_VEC: return "OP_END_VEC";
        case OP_EQUA: return "OP_EQUA";
        case OP_GET_LOCAL_LONG: return "OP_GET_LOCAL_LONG";
        case OP_HALT: return "OP_HALT";
        case OP_IGRTEQ: return "OP_IGRTEQ";
        case OP_IGRT: return "OP_IGRT";
        case OP_ILEQ: return "OP_ILEQ";
        case OP_ILT: return "OP_ILT";
        case OP_LOCI: return "OP_LOCI";
        case OP_LOCS: return "OP_LOCS";
        case OP_LOCV: return "OP_LOCV";
        case OP_MULI: return "OP_MULI";
        case OP_NEWLINE: return "OP_NEWLINE";
        case OP_NOT: return "OP_NOT";
        case OP_ONE: return "OP_ONE";
        case OP_POP_TO_ASTACK: return "OP_POP_TO_ASTACK";
        case OP_POPV: return "OP_POPV";
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
        uint8_t constantaddr = bytes_at(&code->code, ip++);
        struct value val = valuelist_at(&code->constants, constantaddr);
        value_print(val);
        printf(" ");
        return ip;
}

static int
disassemble_constant_vector(struct bytecode *code, int ip)
{
        uint8_t constantaddr = bytes_at(&code->code, ip++);
        struct value val = valuelist_at(&code->constants, constantaddr);
        type_print(val.type);
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
                case OP_LOCV:
                case OP_END_VEC:
                        ip = disassemble_constant_vector(code, ip);
                        break;
                case OP_SKIP_BACK_LONG:
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
semantics_error(struct environment *env, struct tree_node *root, char *fmt, ...)
{
        if (env->error)
                return;
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