#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "frontend.h"

struct parser {
        struct token previous;
        struct token current;
        struct lexer *lexer;
        int panic;
        int error_detected;
};

static struct tree_node *new_tree_node(enum node_type type);
static struct tree_node *new_tree_node_at_current(struct parser *ps, enum node_type type);
static struct tree_node *new_tree_node_at_previous(struct parser *ps, enum node_type type);
static struct tree_node *new_binary_node(struct tree_node *left, struct token op, struct tree_node *right);
static void parse_error(struct parser *ps, struct token tok, char *fmt, ...);
static void error_at_current(struct parser *ps, char *msg);
static enum node_type token_to_bin_node_type(struct token op);
static void advance(struct parser *ps);
static int check(struct parser *ps, enum token_type type);
static int eat(struct parser *ps, enum token_type type);
static int check_error(struct parser *ps, enum token_type type);
static int eat_error(struct parser *ps, enum token_type type);
static void synchronize(struct parser *ps);

static struct tree_node *stat(struct parser *ps);
static struct tree_node *write_stat(struct parser *ps);
static struct tree_node *writeln_stat(struct parser *ps);
static struct tree_node *read_stat(struct parser *ps);
static int is_node_lhs(struct tree_node *lhs);
static struct tree_node *if_stat(struct parser *ps);
static struct tree_node *while_stat(struct parser *ps);
static struct tree_node *repeat_stat(struct parser *ps);
static struct tree_node *for_stat(struct parser *ps);
static struct tree_node *dispatch_id_stat(struct parser *ps);
static struct tree_node *assign_stat_trial(struct parser *ps, struct tree_node *lhs);
static struct tree_node *var_decl_stat_trial(struct parser *ps, struct tree_node *res);
static struct tree_node *id_list_empty(struct parser *ps);
static struct tree_node *type_label(struct parser *ps);
static struct tree_node *expr_stat(struct parser *ps);

static struct tree_node *expr(struct parser *ps);
static struct tree_node *boolean_expr(struct parser *ps);
static struct tree_node *comp_expr(struct parser *ps);
static struct tree_node *add_expr(struct parser *ps);
static struct tree_node *mul_expr(struct parser *ps);
static struct tree_node *term(struct parser *ps);
static struct tree_node *unary_expr(struct parser *ps);
static struct tree_node *const_expr(struct parser *ps);
static struct tree_node *integer_const(struct parser *ps);
static struct tree_node *string_const(struct parser *ps);
static struct tree_node *vector_const(struct parser *ps);
static struct tree_node *boolean_const(struct parser *ps);
static struct tree_node *grouping_expr(struct parser *ps);
static struct tree_node *conditional_expr(struct parser *ps);
struct tree_node *id_expr(struct parser *ps);
static struct tree_node *dispatch_id_expr(struct parser *ps);
struct tree_node *indexing_expr(struct parser *ps, struct tree_node *indexed);
struct tree_node *call_expr(struct parser *ps, struct tree_node *called);
static struct tree_node *expr_list(struct parser *ps);
static struct tree_node *var_decl(struct parser *ps);
static int eat_module_name_error(struct parser *ps, struct token module_name);
static struct tree_node *wrap_expr_in_statement(struct tree_node *exprnode);
static struct tree_node *wrap_expr_in_return_statement(struct parser *ps, struct tree_node *exprnode);
static struct tree_node *program_decl_stat(struct parser *ps);
static struct tree_node *module_decl_stat(struct parser *ps, enum node_type restype, struct tree_node *(*body_parsing_fn)(struct parser *ps));
static struct tree_node *function_or_procedure_decl(struct parser *ps);
static struct tree_node *function_decl_body_fn(struct parser *ps);
static struct tree_node *stat_list_until(struct parser *ps, enum token_type type);
static struct tree_node *var_decl_qualified(struct parser *ps);
static struct tree_node *id_list_qualified(struct parser *ps);
static struct tree_node *id_qualified(struct parser *ps);
static struct tree_node *var_decl_qualified_list(struct parser *ps);
static struct tree_node *var_decl_qualified_list_until(struct parser *ps, enum token_type rightdelim);

struct tree_node *
parse(char *program, int programlen)
{
        struct parser parser;
        struct lexer lexer;
        lexer_init(&lexer, program, programlen);
        parser.lexer = &lexer;
        parser.current = next_token(&lexer);
        parser.panic = 0;
        parser.error_detected = 0;
        struct tree_node *res = program_decl_stat(&parser);
        if (parser.error_detected) {
                tree_node_free(res);
                return NULL;
        }
        return res;
}

static struct tree_node *
procedure_decl_body_fn(struct parser *ps)
{
        struct tree_node *res = stat_list_until(ps, TOKEN_END);
        struct tree_node **pp = &res->child;
        while (*pp != NULL)
                pp = &(*pp)->next;
        *pp = wrap_expr_in_return_statement(ps, NULL);
        return res;
}

static struct tree_node *
function_decl_body_fn(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_STAT_LIST);
        res->child = wrap_expr_in_return_statement(ps, expr(ps));
        return res;
}


static struct tree_node *
program_decl_stat(struct parser *ps)
{
        eat_error(ps, TOKEN_PROGRAM);
        struct tree_node *res = module_decl_stat(ps, NODE_PROGRAM, &procedure_decl_body_fn);
        eat_error(ps, TOKEN_DOT);
        return res;
}

static struct tree_node *
module_decl_stat(struct parser *ps, enum node_type restype, struct tree_node *(*body_parsing_fn)(struct parser *ps))
{
        /* left: name,
        right: param (left) and return type (right),
        child0: fn var decl (left) and  fn module decl (right)
        child1: body (stat list)
        */
        struct tree_node *res = new_tree_node_at_previous(ps, restype);
        res->left = id_expr(ps);
        res->right = new_tree_node_at_current(ps, NODE_FUNCTION_TYPES);
        if (eat(ps, TOKEN_LPAREN) && !ps->error_detected) {
                res->right->left = var_decl_qualified_list_until(ps, TOKEN_RPAREN);
                eat_error(ps, TOKEN_RPAREN);
                if (eat(ps, TOKEN_COLON)) {
                        res->right->right = type_label(ps);
                }
        }
        struct tree_node *var_block = NULL;
        if (!ps->error_detected && !check(ps, TOKEN_FUNCTION) && !check(ps, TOKEN_PROCEDURE) && !check(ps, TOKEN_BEGIN)) {
                struct tree_node **vbp = &var_block;
                do {
                        *vbp = var_decl(ps);
                        vbp = &(*vbp)->next;
                        eat_error(ps, TOKEN_SEMICOLON);
                } while (!check(ps, TOKEN_FUNCTION) && !check(ps, TOKEN_PROCEDURE) && !check(ps, TOKEN_BEGIN) && !check(ps, TOKEN_EOF));
        }
        struct tree_node *module_block = NULL;
        if (!ps->error_detected && !check(ps, TOKEN_BEGIN)) {
                struct tree_node **mbp = &module_block;
                do {
                        *mbp = function_or_procedure_decl(ps);
                        mbp = &(*mbp)->next;
                        eat_error(ps, TOKEN_SEMICOLON);
                } while (!check(ps, TOKEN_BEGIN) && !check(ps, TOKEN_EOF));
        }
        struct tree_node **pp = &res->child;
        *pp = new_tree_node_at_current(ps, NODE_DECLARATION_BLOCKS);
        (*pp)->left = var_block;
        (*pp)->right = module_block;
        pp = &(*pp)->next;
        eat_error(ps, TOKEN_BEGIN);
        eat_module_name_error(ps, res->left->value);
        *pp = (*body_parsing_fn)(ps);
        eat_error(ps, TOKEN_END);
        eat_module_name_error(ps, res->left->value);
        return res;
}

static struct tree_node *
function_or_procedure_decl(struct parser *ps)
{
        if (ps->current.type == TOKEN_FUNCTION) {
                eat_error(ps, TOKEN_FUNCTION);
                return module_decl_stat(ps, NODE_FUNCTION_DECL, &function_decl_body_fn);
        } else {
                eat_error(ps, TOKEN_PROCEDURE);
                return module_decl_stat(ps, NODE_PROCEDURE_DECL, &procedure_decl_body_fn);
        }
}

static struct tree_node *
stat_list_until_list(struct parser *ps, int ntypes, enum token_type *types)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_STAT_LIST);
        struct tree_node **statp = &res->child;
        while (!check(ps, TOKEN_EOF)) {
                for (int i = 0; i < ntypes; i++) {
                        if (check(ps, types[i]))
                                goto after_loop;
                }
                *statp = stat(ps);
                if (*statp != NULL)
                        statp = &(*statp)->next;
                if (ps->panic)
                        synchronize(ps);
                eat_error(ps, TOKEN_SEMICOLON);
        }
        after_loop:
        return res;
}

static struct tree_node *
stat_list_until(struct parser *ps, enum token_type type)
{
        enum token_type types[] = {type};
        return stat_list_until_list(ps, 1, types);
}

static struct tree_node *
stat(struct parser *ps)
{
        switch (ps->current.type) {
        case TOKEN_ID:
                return dispatch_id_stat(ps);
        case TOKEN_IF:
                return if_stat(ps);
        case TOKEN_WHILE:
                return while_stat(ps);
        case TOKEN_REPEAT:
                return repeat_stat(ps);
        case TOKEN_FOR:
                return for_stat(ps);
        case TOKEN_WRITE:
                return write_stat(ps);
        case TOKEN_WRITELN:
                return writeln_stat(ps);
        case TOKEN_READ:
                return read_stat(ps);
        case TOKEN_EXIT:
                eat_error(ps, TOKEN_EXIT);
                return new_tree_node_at_previous(ps, NODE_EXIT_STAT);
        case TOKEN_BREAK:
                eat_error(ps, TOKEN_BREAK);
                return new_tree_node_at_previous(ps, NODE_BREAK_STAT);
        default:
                return expr_stat(ps);
        }
}

static struct tree_node *
while_stat(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_WHILE_STAT);
        advance(ps);
        res->left = expr(ps);
        eat_error(ps, TOKEN_DO);
        res->right = stat_list_until(ps, TOKEN_END);
        eat_error(ps, TOKEN_END);
        return res;
}

static struct tree_node *
repeat_stat(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_REPEAT_STAT);
        advance(ps);
        res->left = stat_list_until(ps, TOKEN_UNTIL);
        eat_error(ps, TOKEN_UNTIL);
        res->right = expr(ps);
        return res;

}

static struct tree_node *
for_stat(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_FOR_STAT);
        advance(ps);

        struct tree_node *assign = new_tree_node_at_current(ps, NODE_ASSIGN_STAT);
        check_error(ps, TOKEN_ID);
        assign->left = id_expr(ps);
        eat_error(ps, TOKEN_ASSIGN);
        assign->right = expr(ps);
        eat_error(ps, TOKEN_TO);

        struct tree_node *limit = expr(ps);
        eat_error(ps, TOKEN_DO);

        struct tree_node *condition = new_tree_node(NODE_LESSEQ_EXPR);
        condition->value = assign->left->value;
        condition->left = assign->left;
        condition->right = limit;

        assign->next = condition;

        res->left = assign;

        res->right = stat_list_until(ps, TOKEN_END);
        eat_error(ps, TOKEN_END);

        return res;
}

static struct tree_node *
write_stat(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_WRITE_STAT);
        eat(ps, TOKEN_WRITE);
        eat_error(ps, TOKEN_LPAREN);
        res->child = expr_list(ps);
        eat_error(ps, TOKEN_RPAREN);
        return res;
}

static struct tree_node *
writeln_stat(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_WRITELN_STAT);
        eat(ps, TOKEN_WRITELN);
        eat_error(ps, TOKEN_LPAREN);
        res->child = expr_list(ps);
        eat_error(ps, TOKEN_RPAREN);
        return res;
}

static struct tree_node *
read_stat(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_READ_STAT);
        eat(ps, TOKEN_READ);
        eat_error(ps, TOKEN_LPAREN);
        res->child = expr_list(ps);
        for (struct tree_node *lhs = res->child->child; lhs != NULL; lhs = lhs->next) {
                if (!is_node_lhs(lhs)) {
                        parse_error(ps, lhs->value, "cannot read into non lhs");
                        break;
                }
        }
        eat_error(ps, TOKEN_RPAREN);
        return res;
}

static int
eat_module_name_error(struct parser *ps, struct token module_name)
{
        if (ps->current.type != TOKEN_ID || !token_equal(ps->current, module_name)) {
                parse_error(ps, ps->current, "module name mismatch (expected \"%.*s\")", module_name.length, module_name.start);
                return 0;
        }
        advance(ps);
        return 1;
}


static struct tree_node *
var_decl(struct parser *ps)
{
        struct tree_node *res = id_expr(ps);
        return var_decl_stat_trial(ps, res);
}

static struct tree_node *
var_decl_qualified_list(struct parser *ps)
{
        struct tree_node *res = NULL;
        struct tree_node **pp = &res;

        *pp = var_decl_qualified(ps);
        pp = &(*pp)->next;
        while (eat(ps, TOKEN_COMMA)) {
                *pp = var_decl_qualified(ps);
                pp = &(*pp)->next;
        }
        return res;
}

static struct tree_node *
var_decl_qualified_list_until(struct parser *ps, enum token_type rightdelim)
{
        if (check(ps, rightdelim))
                return NULL;
        return var_decl_qualified_list(ps);
}

static int
is_node_lhs(struct tree_node *lhs)
{
        return lhs->type == NODE_ID || lhs->type == NODE_INDEXING;
}

static struct tree_node *
wrap_expr_in_statement(struct tree_node *exprnode)
{
        struct tree_node *node = new_tree_node(NODE_EXPR_STAT);
        node->child = exprnode;
        if (node->child)
                node->value = node->child->value;
        return node;
}

static struct tree_node *
wrap_expr_in_return_statement(struct parser *ps, struct tree_node *exprnode)
{
        struct tree_node *node = new_tree_node(NODE_RETURN_STAT);
        node->child = exprnode;
        if (node->child)
                node->value = node->child->value;
        else
                node->value = ps->current;
        return node;
}

static struct tree_node *
if_stat(struct parser *ps)
{
        static enum token_type stat_list_ends[] = {TOKEN_ELSIF, TOKEN_ELSE, TOKEN_END};
        static int ntypes = sizeof(stat_list_ends) / sizeof(stat_list_ends[0]);
        struct tree_node *res = new_tree_node_at_current(ps, NODE_IF_STAT);
        struct tree_node **child = &res->child;
        do {
                advance(ps);
                *child = new_tree_node_at_previous(ps, NODE_CONDITION_AND_STATEMENT);
                (*child)->left = expr(ps);
                eat_error(ps, TOKEN_THEN);
                (*child)->right = stat_list_until_list(ps, ntypes, stat_list_ends);
                child = &(*child)->next;
        } while (check(ps, TOKEN_ELSIF));
        if (check(ps, TOKEN_ELSE)) {
                advance(ps);
                *child= stat_list_until_list(ps, ntypes, stat_list_ends);
                child = &(*child)->next;
        }
        eat_error(ps, TOKEN_END);
        return res;
}

static struct tree_node *
dispatch_id_stat(struct parser *ps)
{
        struct tree_node *res;
        res = expr(ps);
        switch (ps->current.type) {
        case TOKEN_ASSIGN:
                return assign_stat_trial(ps, res);
        case TOKEN_COMMA:
        case TOKEN_COLON:
                return var_decl_stat_trial(ps, res);
        default:
                return wrap_expr_in_statement(res);
        }
}

static struct tree_node *
assign_stat_trial(struct parser *ps, struct tree_node *lhs)
{
        advance(ps);
        if (!is_node_lhs(lhs)) {
                error_at_current(ps, "invalid assignment target");
                return lhs;
        }
        struct token eq = ps->previous;
        return new_binary_node(lhs, eq, expr(ps));
}

static struct tree_node *
var_decl_stat_trial(struct parser *ps, struct tree_node *res)
{
        if (res->type != NODE_ID) {
                error_at_current(ps, "invalid variable");
                return res;
        }
        struct tree_node *tmp;
        if (check(ps, TOKEN_COMMA))
                advance(ps);
        tmp = id_list_empty(ps);
        tmp->value = res->value;
        res->next = tmp->child;
        tmp->child = res;
        res = tmp;
        eat_error(ps, TOKEN_COLON);
        tmp = new_tree_node_at_previous(ps, NODE_VAR_DECL);
        tmp->left = res;
        tmp->right = type_label(ps);
        res = tmp;
        return res;
}

static struct tree_node *
id_list_empty(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_ID_LIST);
        struct tree_node **chp = &res->child;
        while (check(ps, TOKEN_ID)) {
                *chp = id_expr(ps);
                chp = &(*chp)->next;
                eat(ps, TOKEN_COMMA);
        }
        return res;
}

static struct tree_node *
type_label(struct parser *ps)
{
        advance(ps);
        switch (ps->previous.type) {
        case TOKEN_STRING:
                return new_tree_node_at_previous(ps, NODE_STRING_TYPE);
        case TOKEN_INTEGER:
                return new_tree_node_at_previous(ps, NODE_INTEGER_TYPE);
        case TOKEN_BOOLEAN:
                return new_tree_node_at_previous(ps, NODE_BOOLEAN_TYPE);
        case TOKEN_VECTOR: {
                struct tree_node *toret = new_tree_node_at_previous(ps, NODE_VECTOR_TYPE);
                eat_error(ps, TOKEN_LSQUARE);
                eat_error(ps, TOKEN_INTEGERLIT);
                toret->left = new_tree_node_at_previous(ps, NODE_INTGER_CONST);
                eat_error(ps, TOKEN_RSQUARE);
                eat_error(ps, TOKEN_OF);
                toret->right = type_label(ps);
                return toret;
        }
        default:
                parse_error(ps, ps->previous, "unrecognized type");
                return NULL;
        }
}

static struct tree_node *
var_decl_qualified(struct parser *ps)
{
        struct tree_node *ids = id_list_qualified(ps);
        eat_error(ps, TOKEN_COLON);
        struct tree_node *res = new_tree_node_at_previous(ps, NODE_VAR_DECL);
        res->left = ids;
        res->right = type_label(ps);
        return res;
}

static struct tree_node *
id_list_qualified(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_ID_LIST);
        struct tree_node **pp = &res->child;
        *pp = id_qualified(ps);
        pp = &(*pp)->next;
        while (eat(ps, TOKEN_COMMA)) {
                *pp = id_qualified(ps);
                pp = &(*pp)->next;
        }
        return res;
}

static struct tree_node *
id_qualified(struct parser *ps)
{
        struct tree_node *res = NULL;
        struct tree_node *qualifier = NULL;
        if (eat(ps, TOKEN_INOUT) || eat(ps, TOKEN_OUT)) {
                qualifier = new_tree_node_at_previous(ps, NODE_QUALIFIER);
        }
        res = id_expr(ps);
        res->child = qualifier;
        return res;
}

static struct tree_node *
expr_stat(struct parser *ps)
{
        return wrap_expr_in_statement(expr(ps));
}

/* expressions */

static struct tree_node *
expr(struct parser *ps)
{
        return boolean_expr(ps);
}

struct tree_node *
boolean_expr(struct parser *ps)
{
        struct tree_node *left = comp_expr(ps);
        while (eat(ps, TOKEN_AND) || eat(ps, TOKEN_OR)) {
                struct token op = ps->previous;
                struct tree_node *right = comp_expr(ps);
                left = new_binary_node(left, op, right);
        }
        return left;
}

static struct tree_node *
comp_expr(struct parser *ps)
{
        struct tree_node *left = add_expr(ps);
        if (eat(ps, TOKEN_LESS) ||
                eat(ps, TOKEN_LESSEQ) ||
                eat(ps, TOKEN_GREATER) ||
                eat(ps, TOKEN_GREATEREQ) ||
                eat(ps, TOKEN_EQ) ||
                eat(ps, TOKEN_NEQ)
        ) {
                        struct token op = ps->previous;
                        struct tree_node *right = add_expr(ps);
                        left = new_binary_node(left, op, right);
                }
                return left;
}

static struct tree_node *
add_expr(struct parser *ps)
{
        struct tree_node *left = mul_expr(ps);
        while (eat(ps, TOKEN_PLUS) || eat(ps, TOKEN_MINUS)) {
                struct token op = ps->previous;
                struct tree_node *right = mul_expr(ps);
                left = new_binary_node(left, op, right);
        }
        return left;
}

static struct tree_node *
mul_expr(struct parser *ps)
{
        struct tree_node *left = term(ps);
        while (eat(ps, TOKEN_STAR) || eat(ps, TOKEN_SLASH)) {
                struct token op = ps->previous;
                struct tree_node *right = term(ps);
                left = new_binary_node(left, op, right);
        }
        return left;
}

static struct tree_node *
term(struct parser *ps)
{
        switch (ps->current.type) {
        case TOKEN_MINUS:
        case TOKEN_BANG:
                return unary_expr(ps);
        case TOKEN_LPAREN:
                return grouping_expr(ps);
        case TOKEN_INTEGERLIT:
        case TOKEN_STRINGLIT:
        case TOKEN_LSQUARE:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
                return const_expr(ps);
        case TOKEN_IF:
                return conditional_expr(ps);
        case TOKEN_ID:
                return dispatch_id_expr(ps);
        default:
                error_at_current(ps, "unexpected token");
                return NULL;
        }
}

static struct tree_node *
unary_expr(struct parser *ps)
{
        enum node_type type = ps->current.type == TOKEN_BANG ? NODE_NOT_EXPR : NODE_NEG_EXPR;
        advance(ps);
        struct tree_node *child = term(ps);
        struct tree_node *res = new_tree_node_at_current(ps, type);
        res->right = child;
        return res;
}

static struct tree_node *
const_expr(struct parser *ps)
{
        switch (ps->current.type) {
        case TOKEN_INTEGERLIT:
                return integer_const(ps);
        case TOKEN_STRINGLIT:
                return string_const(ps);
        case TOKEN_LSQUARE:
                return vector_const(ps);
        case TOKEN_TRUE:
        case TOKEN_FALSE:
                return boolean_const(ps);
        default:
                error_at_current(ps, "expected constant expression");
                return NULL;
        }
}

static struct tree_node *
integer_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_INTGER_CONST);
        advance(ps);
        return res;
}

static struct tree_node *
string_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_STRING_CONST);
        advance(ps);
        return res;
}

static struct tree_node *
vector_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_VECTOR_CONST);
        advance(ps);
        res->child = expr_list(ps);
        eat_error(ps, TOKEN_RSQUARE);
        if (eat(ps, TOKEN_LSQUARE)) {
                res = indexing_expr(ps, res);
        }
        return res;
}

static struct tree_node *
boolean_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_BOOLEAN_CONST);
        advance(ps);
        return res;
}

static struct tree_node *
grouping_expr(struct parser *ps)
{
        advance(ps);
        struct tree_node *res = expr(ps);
        eat_error(ps, TOKEN_RPAREN);
        return res;
}

static struct tree_node *
conditional_expr(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_COND_EXPR);
        struct tree_node **child = &res->child;
        do {
                advance(ps);
                *child = new_tree_node_at_previous(ps, NODE_CONDITION_AND_EXPRESSION);
                (*child)->left = expr(ps);
                eat_error(ps, TOKEN_THEN);
                (*child)->right = expr(ps);
                child = &(*child)->next;
        } while (check(ps, TOKEN_ELSIF));
        eat_error(ps, TOKEN_ELSE);
        *child = expr(ps);
        child = &(*child)->next;
        eat_error(ps, TOKEN_END);
        return res;
}

struct tree_node *
id_expr(struct parser *ps)
{
        eat_error(ps, TOKEN_ID);
        return new_tree_node_at_previous(ps, NODE_ID);
}

static struct tree_node *
dispatch_id_expr(struct parser *ps)
{
        struct tree_node *res = id_expr(ps);
        if (!check(ps, TOKEN_LPAREN) && !check(ps, TOKEN_LSQUARE))
                return res;
        while (eat(ps, TOKEN_LPAREN) || eat(ps, TOKEN_LSQUARE)) {
                struct token op = ps->previous;
                switch (op.type) {
                case TOKEN_LSQUARE:
                        res = indexing_expr(ps, res);
                        break;
                case TOKEN_LPAREN:
                        res = call_expr(ps, res);
                        break;
                default:
                        break;
                }
        }
        return res;
}

struct tree_node *
indexing_expr(struct parser *ps, struct tree_node *indexed)
{
        struct tree_node *res = new_binary_node(indexed, ps->previous, NULL);
        struct tree_node **pp = &res->right;
        do {
                *pp = expr(ps);
                pp = &(*pp)->next;
                eat_error(ps, TOKEN_RSQUARE);
        } while (eat(ps, TOKEN_LSQUARE));
        return res;
}

struct tree_node *
call_expr(struct parser *ps, struct tree_node *called)
{
        struct tree_node *res = new_binary_node(called, ps->previous, NULL);
        if (!check(ps, TOKEN_RPAREN))
                res->right = expr_list(ps);
        eat_error(ps, TOKEN_RPAREN);
        return res;
}

static struct tree_node *
expr_list(struct parser *ps)
{
        struct tree_node *res = expr(ps);
        struct tree_node **pp = &res;
        pp = &(*pp)->next;
        while (eat(ps, TOKEN_COMMA)) {
                *pp = expr(ps);
                pp = &(*pp)->next;
        }
        return res;
}

static int
eat(struct parser *ps, enum token_type type)
{
        if (check(ps, type)) {
                advance(ps);
                return 1;
        }
        return 0;
}

static int
check_error(struct parser *ps, enum token_type type)
{
        if (!check(ps, type)) {
                parse_error(
                        ps,
                        ps->current,
                        "expected %s, got %.*s",
                        token_type_string(type),
                        ps->current.length,
                        ps->current.start
                );
                return 0;
        }
        return 1;
}

static int
eat_error(struct parser *ps, enum token_type type)
{
        if (!check_error(ps, type))
                return 0;
        eat(ps, type);
        return 1;
}

static int
check(struct parser *ps, enum token_type type)
{
        return ps->current.type == type;
}

static void
advance(struct parser *ps)
{
        ps->previous = ps->current;
        for (;;) {
                ps->current = next_token(ps->lexer);
                if (ps->current.type != TOKEN_ERROR)
                        break;
                error_at_current(ps, "");
        }
}

static void
synchronize(struct parser *ps)
{
        ps->panic = 0;
        while (ps->current.type != TOKEN_EOF) {
                if (ps->current.type == TOKEN_SEMICOLON)
                        break;
                advance(ps);
        }
}

static struct tree_node *
new_tree_node(enum node_type type)
{
        struct tree_node *node = malloc(sizeof(struct tree_node));
        node->type = type;
        node->left =  NULL;
        node->right =  NULL;
        node->child =  NULL;
        node->next = NULL;
        return node;
}

static struct tree_node *
new_tree_node_at_current(struct parser *ps, enum node_type type)
{
        struct tree_node *node = new_tree_node(type);
        node->value = ps->current;
        return node;
}

static struct tree_node *
new_tree_node_at_previous(struct parser *ps, enum node_type type)
{
        struct tree_node *node = new_tree_node(type);
        node->value = ps->previous;
        return node;
}

static enum
node_type token_to_bin_node_type(struct token op)
{
        switch (op.type) {
        case TOKEN_NEQ: return NODE_NEQ_EXPR;
        case TOKEN_EQ: return NODE_EQ_EXPR;
        case TOKEN_LESS: return NODE_LESS_EXPR;
        case TOKEN_LESSEQ: return NODE_LESSEQ_EXPR;
        case TOKEN_GREATER: return NODE_GREATER_EXPR;
        case TOKEN_GREATEREQ: return NODE_GREATEREQ_EXPR;
        case TOKEN_PLUS: return NODE_PLUS_EXPR;
        case TOKEN_MINUS: return NODE_MINUS_EXPR;
        case TOKEN_BANG: return NODE_NOT_EXPR;
        case TOKEN_OR: return NODE_OR_EXPR;
        case TOKEN_AND: return NODE_AND_EXPR;
        case TOKEN_STAR: return NODE_TIMES_EXPR;
        case TOKEN_SLASH: return NODE_DIVIDE_EXPR;
        case TOKEN_LPAREN: return NODE_MODULE_CALL;
        case TOKEN_LSQUARE: return NODE_INDEXING;
        case TOKEN_ASSIGN: return NODE_ASSIGN_STAT;
        default: return -1;
        }
}

static struct tree_node *
new_binary_node(struct tree_node *left, struct token op, struct tree_node *right)
{

        enum node_type type = token_to_bin_node_type(op);
        struct tree_node *node = new_tree_node(type);
        node->left = left;
        node->right = right;
        node->value = op;
        return node;
}

static void
parse_error(struct parser *ps, struct token tok, char *fmt, ...)
{
        if (ps->panic)
                return;
        va_list args;
        va_start(args, fmt);
        ps->error_detected = 1;
        ps->panic = 1;
        fprintf(stderr, "parse error ");
        if (tok.type == TOKEN_EOF) {
                fprintf(stderr, "[at end]: ");
        } else {
                fprintf(stderr, "[at %d:%d]: ", tok.line, tok.linepos);
                if (tok.type != TOKEN_ERROR) {
                        fprintf(stderr, "at '%.*s', ", tok.length, tok.start);
                } else {
                        fprintf(stderr, "lexer error: %.*s", tok.length, tok.start);
                }
        }
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}


static void
error_at_current(struct parser *ps, char *msg)
{
        parse_error(ps, ps->current, msg);
}

char *
node_type_string(enum node_type type)
{
        switch (type) {
        case NODE_AND_EXPR: return "NODE_AND_EXPR";
        case NODE_ASSIGN_STAT: return "NODE_ASSIGN_STAT";
        case NODE_BOOLEAN_CONST: return "NODE_BOOLEAN_CONST";
        case NODE_BOOLEAN_TYPE: return "NODE_BOOLEAN_TYPE";
        case NODE_BREAK_STAT: return "NODE_BREAK_STAT";
        case NODE_COND_EXPR: return "NODE_COND_EXPR";
        case NODE_CONDITION_AND_EXPRESSION: return "NODE_CONDITION_AND_STATEMENT";
        case NODE_CONDITION_AND_STATEMENT: return "NODE_CONDITION_AND_STATEMENT";
        case NODE_DECLARATION_BLOCKS: return "NODE_DECLARATION_BLOCKS";
        case NODE_DIVIDE_EXPR: return "NODE_DIVIDE_EXPR";
        case NODE_EQ_EXPR: return "NODE_EQ_EXPR";
        case NODE_EXIT_STAT: return "NODE_EXIT_STAT";
        case NODE_EXPR_BODY: return "NODE_EXPR_BODY";
        case NODE_EXPR_LIST: return "NODE_EXPR_LIST";
        case NODE_EXPR_STAT: return "NODE_EXPR_STAT";
        case NODE_FORMAL_DECL: return "NODE_FORMAL_DECL";
        case NODE_FOR_STAT: return "NODE_FOR_STAT";
        case NODE_FUNCTION_DECL: return "NODE_FUNCTION_DECL";
        case NODE_FUNCTION_TYPES: return "NODE_FUNCTION_TYPES";
        case NODE_GREATEREQ_EXPR: return "NODE_GREATEREQ_EXPR";
        case NODE_GREATER_EXPR: return "NODE_GREATER_EXPR";
        case NODE_ID_LIST: return "NODE_ID_LIST";
        case NODE_ID: return "NODE_ID";
        case NODE_IF_STAT: return "NODE_IF_STAT";
        case NODE_INDEXING: return "NODE_INDEXING";
        case NODE_INTEGER_TYPE: return "NODE_INTEGER_TYPE";
        case NODE_INTGER_CONST: return "NODE_INTGER_CONST";
        case NODE_LESSEQ_EXPR: return "NODE_LESSEQ_EXPR";
        case NODE_LESS_EXPR: return "NODE_LESS_EXPR";
        case NODE_MINUS_EXPR: return "NODE_MINUS_EXPR";
        case NODE_MODE_INOUT: return "NODE_MODE_INOUT";
        case NODE_MODE_IN: return "NODE_MODE_IN";
        case NODE_MODE_OUT: return "NODE_MODE_OUT";
        case NODE_MODULE_CALL: return "NODE_MODULE_CALL";
        case NODE_MODULE_DECL_LIST: return "NODE_MODULE_DECL_LIST";
        case NODE_NEG_EXPR: return "NODE_NEG_EXPR";
        case NODE_NEQ_EXPR: return "NODE_NEQ_EXPR";
        case NODE_NOT_EXPR: return "NODE_NOT_EXPR";
        case NODE_OR_EXPR: return "NODE_OR_EXPR";
        case NODE_PLUS_EXPR: return "NODE_PLUS_EXPR";
        case NODE_PROCEDURE_DECL: return "NODE_PROCEDURE_DECL";
        case NODE_PROGRAM: return "NODE_PROGRAM";
        case NODE_QUALIFIER: return "NODE_QUALIFIER";
        case NODE_READ_STAT: return "NODE_READ_STAT";
        case NODE_REPEAT_STAT: return "NODE_REPEAT_STAT";
        case NODE_RETURN_STAT: return "NODE_RETURN_STAT";
        case NODE_STAT_BODY: return "NODE_STAT_BODY";
        case NODE_STAT_LIST: return "NODE_STAT_LIST";
        case NODE_STRING_CONST: return "NODE_STRING_CONST";
        case NODE_STRING_TYPE: return "NODE_STRING_TYPE";
        case NODE_TIMES_EXPR: return "NODE_TIMES_EXPR";
        case NODE_VAR_DECL_LIST: return "NODE_VAR_DECL_LIST";
        case NODE_VAR_DECL: return "NODE_VAR_DECL";
        case NODE_VECTOR_CONST: return "NODE_VECTOR_CONST";
        case NODE_VECTOR_TYPE: return "NODE_VECTOR_TYPE";
        case NODE_WHILE_STAT: return "NODE_WHILE_STAT";
        case NODE_WRITELN_STAT: return "NODE_WRITELN_STAT";
        case NODE_WRITE_STAT: return "NODE_WRITE_STAT";
        }
        exit(100);
        return "";
}

struct tree_node *
lhs_variable(struct tree_node *left)
{
        return left->type == NODE_INDEXING ? left->left : left;
}

static void
tree_node_print_helper(struct tree_node *root, int level)
{
        static char *tee = "├";
        static char *dash = "─";
        static char *pipe =  "│";

        if (level > 0) {
                int i;
                for (i = 0; i < level - 1; i++) {
                        printf("%s   ", pipe);
                }
                printf("%s%s%s ", tee, dash, dash);
        }

        if (root == NULL) {
                printf("NULL\n");
                return;
        }

        printf("%s ", node_type_string(root->type));
        printf("[%.*s %d:%d]\n", root->value.length, root->value.start, root->value.line, root->value.linepos);
        /* right first for clarity in output for nested binary expressions */
        struct tree_node *child;
        child = root->right;
        while (child != NULL) {
                tree_node_print_helper(child, level + 1);
                child = child->next;
        }
        child = root->left;
        while (child != NULL) {
                tree_node_print_helper(child, level + 1);
                child = child->next;
        }
        child = root->child;
        while (child != NULL) {
                tree_node_print_helper(child, level + 1);
                child = child->next;
        }
}

void
tree_node_print(struct tree_node *root)
{
        while (root != NULL) {
                tree_node_print_helper(root, 0);
                root = root->next;
        }
}

void
tree_node_free(struct tree_node *root)
{
        if (root == NULL)
                return;
        tree_node_free(root->left);
        tree_node_free(root->right);
        struct tree_node *child = root->child;
        while (child != NULL) {
                struct tree_node *next = child->next;
                tree_node_free(child);
                child = next;
        }
        free(root);
}
