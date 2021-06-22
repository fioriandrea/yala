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

static struct tree_node *new_tree_node(enum node_type type, int line, int linepos);
static struct tree_node *new_tree_node_at_current(struct parser *ps, enum node_type type);
static struct tree_node *new_binary_node(enum node_type type, struct tree_node *left, struct tree_node *right);
static void parse_error(struct parser *ps, struct token tok, char *fmt, ...);
static void error_at_current(struct parser *ps, char *msg);
static void error_at_previous(struct parser *ps, char *msg);
static enum node_type token_to_bin_node_type(struct token tok);
static void advance(struct parser *ps);
static int check(struct parser *ps, enum token_type type);
static int eat(struct parser *ps, enum token_type type);
static int eat_error(struct parser *ps, enum token_type type);
static void synchronize(struct parser *ps);
static char *copystrtoken(struct token tok);

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
static struct tree_node *const_list(struct parser *ps);
static struct tree_node *boolean_const(struct parser *ps);
static struct tree_node *grouping_expr(struct parser *ps);
static struct tree_node *conditional_expr(struct parser *ps);
static struct tree_node *elsif_expr_list(struct parser *ps);
static struct tree_node *dispatch_id_expr(struct parser *ps);
static struct tree_node *lhs(struct parser *ps);
static struct tree_node *expr_list(struct parser *ps);

struct tree_node *
parse(char *program, int programlen)
{
        struct parser parser;
        struct lexer lexer;
        init_lexer(&lexer, program, programlen);
        parser.lexer = &lexer;
        parser.current = next_token(&lexer);
        parser.panic = 0;
        parser.error_detected = 0;
        struct tree_node *res = expr(&parser);
        if (parser.error_detected) {
                tree_node_free(res);
                return NULL;
        }
        return res;
}

struct tree_node *
expr(struct parser *ps)
{
        return boolean_expr(ps);
}

struct tree_node *
boolean_expr(struct parser *ps)
{
        struct tree_node *left = comp_expr(ps);
        while (eat(ps, TOKEN_AND) || eat(ps, TOKEN_OR)) {
                enum node_type type = token_to_bin_node_type(ps->previous);
                struct tree_node *right = comp_expr(ps);
                left = new_binary_node(type, left, right);
        }
        return left;
}

static struct tree_node *
comp_expr(struct parser *ps)
{
        struct tree_node *left = add_expr(ps);
        if (eat(ps, TOKEN_LESS) ||
                eat(ps, TOKEN_LESS) ||
                eat(ps, TOKEN_LESSEQ) || 
                eat(ps, TOKEN_GREATER) || 
                eat(ps, TOKEN_GREATEREQ) ||
                eat(ps, TOKEN_EQ) ||
                eat(ps, TOKEN_NEQ)
        ) {
                        enum node_type type = token_to_bin_node_type(ps->previous);
                        struct tree_node *right = add_expr(ps);
                        left = new_binary_node(type, left, right);
                }
                return left;
}

static struct tree_node *
add_expr(struct parser *ps)
{
        struct tree_node *left = mul_expr(ps);
        while (eat(ps, TOKEN_PLUS) || eat(ps, TOKEN_MINUS)) {
                enum node_type type = token_to_bin_node_type(ps->previous);
                struct tree_node *right = mul_expr(ps);
                left = new_binary_node(type, left, right);
        }
        return left;
}

static struct tree_node *
mul_expr(struct parser *ps)
{
        struct tree_node *left = term(ps);
        while (eat(ps, TOKEN_STAR) || eat(ps, TOKEN_SLASH)) {
                enum node_type type = token_to_bin_node_type(ps->previous);
                struct tree_node *right = term(ps);
                left = new_binary_node(type, left, right);
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
        res->child = child;
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

        /* convert string to integer */
        int num = 0;
        char *str = ps->current.start;
        while (str - ps->current.start < ps->current.length) {
                num = num * 10 + (*str - '0');
                str++;
        }

        res->value.ival = num;
        advance(ps);
        return res;
}

static struct tree_node *
string_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_STRING_CONST);
        res->value.sval = copystrtoken(ps->current);
        advance(ps);
        return res;
}

static struct tree_node *
vector_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_VECTOR_CONST);
        advance(ps);
        res->child = const_list(ps);
        eat_error(ps, TOKEN_RSQUARE);
        return res;
}

static struct tree_node *
const_list(struct parser *ps)
{
        struct tree_node *res = const_expr(ps);
        struct tree_node **ptr = &res;
        ptr = &(*ptr)->next;
        while (eat(ps, TOKEN_COMMA)) {
                *ptr = const_expr(ps);
                ptr = &(*ptr)->next;
        }
        return res;
}

static struct tree_node *
boolean_const(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_BOOLEAN_CONST);
        res->value.bval = ps->current.type == TOKEN_TRUE;
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
        advance(ps);
        struct tree_node **child = &res->child;
        *child = expr(ps);
        child = &(*child)->next;
        eat_error(ps, TOKEN_THEN);
        *child = expr(ps);
        child = &(*child)->next;
        if (check(ps, TOKEN_ELSIF)) {
                *child = elsif_expr_list(ps);
                child = &(*child)->next;
        }
        eat_error(ps, TOKEN_ELSE);
        *child = expr(ps);
        child = &(*child)->next;
        eat_error(ps, TOKEN_END);
        return res;
}

static struct tree_node *
elsif_expr_list(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_ELSIF_EXPR_LIST);
        struct tree_node **child = &res->child;
        while (eat(ps, TOKEN_ELSIF)) {
                *child = expr(ps);
                child = &(*child)->next;
                eat_error(ps, TOKEN_THEN);
                *child = expr(ps);
                child = &(*child)->next;
        }
        return res;
}

static struct tree_node *
dispatch_id_expr(struct parser *ps)
{
        struct tree_node *res = lhs(ps);
        if (res->type == NODE_ID && eat(ps, TOKEN_LPAREN)) {
                struct tree_node *args = expr_list(ps);
                eat_error(ps, TOKEN_RPAREN);
                struct tree_node *tmp = new_tree_node_at_current(ps, NODE_FUNCTION_CALL);
                tmp->left = res;
                tmp->right = args;
                res = tmp;
        }
        return res;
}

static struct tree_node *
lhs(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_ID);
        res->value.sval = copystrtoken(ps->current);
        advance(ps);
        while (eat(ps, TOKEN_LSQUARE)) {
                struct tree_node *index = expr(ps);
                eat_error(ps, TOKEN_RSQUARE);
                res = new_binary_node(NODE_INDEXING, res, index);
        }
        return res;
}

static struct tree_node *
expr_list(struct parser *ps)
{
        struct tree_node *res = new_tree_node_at_current(ps, NODE_EXPR_LIST);
        res->child = expr(ps);
        struct tree_node **child = &res->child;
        child = &(*child)->next;
        while (eat(ps, TOKEN_COMMA)) {
                *child = expr(ps);
                child = &(*child)->next;
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
eat_error(struct parser *ps, enum token_type type)
{
        if (!eat(ps, type)) {
                parse_error(
                        ps,
                        ps->current,
                        "expected %s, got %.*s",
                        tokentypestring(type),
                        ps->current.length,
                        ps->current.start
                );
                return 0;
        }
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
        /* TODO: Synchronize on statement boundaries */
        ps->panic = 0;
        while (ps->current.type != TOKEN_EOF) {
                if (ps->previous.type == TOKEN_SEMICOLON)
                        break;
                advance(ps);
        }
}

static struct tree_node *
new_tree_node(enum node_type type, int line, int linepos)
{
        struct tree_node *node = malloc(sizeof(struct tree_node));
        node->type = type;
        node->left =  NULL;
        node->right =  NULL;
        node->child =  NULL;
        node->next = NULL;
        node->linfo.line = line;
        node->linfo.linepos = linepos;
        return node;
}

static struct tree_node *
new_tree_node_at_current(struct parser *ps, enum node_type type)
{
        return new_tree_node(type, ps->current.line, ps->current.linepos);
}

static struct tree_node *
new_binary_node(enum node_type type, struct tree_node *left, struct tree_node *right)
{
        struct tree_node *node = new_tree_node(type, left->linfo.line, left->linfo.linepos);
        node->left = left;
        node->right = right;
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
        fprintf(stderr, "error ");
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

static void
error_at_previous(struct parser *ps, char *msg)
{
        parse_error(ps, ps->previous, msg);
}

static enum
node_type token_to_bin_node_type(struct token tok)
{
        switch (tok.type) {
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
        default: return -1;
        }
}

static char *
copystrtoken(struct token tok)
{
        char *str = malloc(sizeof(char) * (tok.length + 1));
        strncpy(str, tok.start, tok.length);
        str[tok.length] = '\0';
        return str;
}

char *
nodetypestring(enum node_type type)
{
        switch (type) {
        case NODE_AND_EXPR: return "NODE_AND_EXPR";
        case NODE_ASSIGN_STAT: return "NODE_ASSIGN_STAT";
        case NODE_BOOLEAN_CONST: return "NODE_BOOLEAN_CONST";
        case NODE_BOOLEAN_TYPE: return "NODE_BOOLEAN_TYPE";
        case NODE_BREAK_STAT: return "NODE_BREAK_STAT";
        case NODE_COND_EXPR: return "NODE_COND_EXPR";
        case NODE_DIVIDE_EXPR: return "NODE_DIVIDE_EXPR";
        case NODE_NEG_EXPR: return "NODE_NEG_EXPR";
        case NODE_EQ_EXPR: return "NODE_EQ_EXPR";
        case NODE_EXIT_STAT: return "NODE_EXIT_STAT";
        case NODE_EXPR_BODY: return "NODE_EXPR_BODY";
        case NODE_EXPR_LIST: return "NODE_EXPR_LIST";
        case NODE_FORMAL_DECL: return "NODE_FORMAL_DECL";
        case NODE_FOR_STAT: return "NODE_FOR_STAT";
        case NODE_FUNCTION_DECL: return "NODE_FUNCTION_DECL";
        case NODE_FUNCTION_CALL: return "NODE_FUNCTION_CALL";
        case NODE_GREATEREQ_EXPR: return "NODE_GREATEREQ_EXPR";
        case NODE_GREATER_EXPR: return "NODE_GREATER_EXPR";
        case NODE_ID_LIST: return "NODE_ID_LIST";
        case NODE_ID: return "NODE_ID";
        case NODE_IF_STAT: return "NODE_IF_STAT";
        case NODE_INDEXING: return "NODE_INDEXING";
        case NODE_INTEGER_TYPE: return "NODE_INTEGER_TYPE";
        case NODE_INTGER_CONST: return "NODE_INTGER_CONST";
        case NODE_LESSEQ_EXPR: return "NODE_LESSEQ_EXPR";
        case NODE_LHS: return "NODE_LHS";
        case NODE_LESS_EXPR: return "NODE_LESS_EXPR";
        case NODE_MINUS_EXPR: return "NODE_MINUS_EXPR";
        case NODE_MODE_INOUT: return "NODE_MODE_INOUT";
        case NODE_MODE_IN: return "NODE_MODE_IN";
        case NODE_MODE_OUT: return "NODE_MODE_OUT";
        case NODE_MODULE_DECL_LIST: return "NODE_MODULE_DECL_LIST";
        case NODE_NEQ_EXPR: return "NODE_NEQ_EXPR";
        case NODE_NOT_EXPR: return "NODE_NOT_EXPR";
        case NODE_ELSIF_EXPR_LIST: return "NODE_ELSIF_EXPR_LIST";
        case NODE_OR_EXPR: return "NODE_OR_EXPR";
        case NODE_PLUS_EXPR: return "NODE_PLUS_EXPR";
        case NODE_PROCEDURE_DECL: return "NODE_PROCEDURE_DECL";
        case NODE_PROC_CALL_NODE: return "NODE_PROC_CALL_NODE";
        case NODE_PROGRAM: return "NODE_PROGRAM";
        case NODE_READ_STAT: return "NODE_READ_STAT";
        case NODE_REPEAT_STAT: return "NODE_REPEAT_STAT";
        case NODE_STAT_BODY: return "NODE_STAT_BODY";
        case NODE_STAT_LIST: return "NODE_STAT_LIST";
        case NODE_STRING_CONST: return "NODE_STRING_CONST";
        case NODE_STRING_TYPE: return "NODE_STRING_TYPE";
        case NODE_TIMES_EXPR: return "NODE_TIMES_EXPR";
        case NODE_UNARY_MINUS_EXPR: return "NODE_UNARY_MINUS_EXPR";
        case NODE_VAR_DECL_LIST: return "NODE_VAR_DECL_LIST";
        case NODE_VAR_DECL: return "NODE_VAR_DECL";
        case NODE_VECTOR_CONST: return "NODE_VECTOR_CONST";
        case NODE_VECTOR_TYPE: return "NODE_VECTOR_TYPE";
        case NODE_WHILE_STAT: return "NODE_WHILE_STAT";
        case NODE_WRITELN_STAT: return "NODE_WRITELN_STAT";
        case NODE_WRITE_STAT: return "NODE_WRITE_STAT";
        default: return "unrecognized node type";
        }
}

static void
treeprinthelper(struct tree_node *root, int level)
{
        static char *tee = "├";
        static char *dash = "─";
        static char *pipe =  "│";

        if (root == NULL)
                return;

        if (level > 0) {
                int i;
                for (i = 0; i < level - 1; i++) {
                        printf("%s   ", pipe);
                }
                printf("%s%s%s ", tee, dash, dash);
        }

        printf("%s (%d:%d)", nodetypestring(root->type), root->linfo.line, root->linfo.linepos);
        switch (root->type) {
        case NODE_ID:
        case NODE_STRING_CONST:
        printf(" (%s)", root->value.sval);
        break;
        case NODE_INTGER_CONST:
        printf(" (%d)", root->value.ival);
        break;
        case NODE_BOOLEAN_CONST:
        printf(" (%s)", root->value.bval ? "true" : "false");
        break;
        default:
        break;
        }
        printf("\n");
        /* right first for clarity in output for nested binary expressions */
        treeprinthelper(root->right, level + 1);
        treeprinthelper(root->left, level + 1);
        struct tree_node *child = root->child;
        while (child != NULL) {
                treeprinthelper(child, level + 1);
                child = child->next;
        }
}

void
treeprint(struct tree_node *root)
{
        treeprinthelper(root, 0);
}

void
tree_node_free(struct tree_node *root)
{
        if (root == NULL)
                return;
        if (root->type == NODE_STRING_CONST || root->type == NODE_ID)
                free(root->value.sval);
        tree_node_free(root->left);
        tree_node_free(root->right);
        struct tree_node *child = root->child;
        while (child != NULL) {
                struct tree_node *next = child->next;
                tree_node_free(child);
                child = next;
        }
}