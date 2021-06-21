#ifndef frontend_h
#define frontend_h

#include <stdint.h>

#include "../datastructs/datastructs.h"

enum token_type {
        TOKEN_AND,
        TOKEN_ASSIGN,
        TOKEN_BANG,
        TOKEN_BEGIN,
        TOKEN_BOOLEAN,
        TOKEN_BREAK,
        TOKEN_COLON,
        TOKEN_COMMA,
        TOKEN_DO,
        TOKEN_DOT,
        TOKEN_ELSE,
        TOKEN_ELSIF,
        TOKEN_END,
        TOKEN_EOF,
        TOKEN_EQ,
        TOKEN_ERROR,
        TOKEN_EXIT,
        TOKEN_FALSE,
        TOKEN_FOR,
        TOKEN_FUNCTION,
        TOKEN_GREATER,
        TOKEN_GREATEREQ,
        TOKEN_ID,
        TOKEN_IF,
        TOKEN_INOUT,
        TOKEN_INTEGER,
        TOKEN_INTEGERLIT,
        TOKEN_LESS,
        TOKEN_LESSEQ,
        TOKEN_LPAREN,
        TOKEN_LSQUARE,
        TOKEN_MINUS,
        TOKEN_NEQ,
        TOKEN_OF,
        TOKEN_OR,
        TOKEN_OUT,
        TOKEN_PLUS,
        TOKEN_PROCEDURE,
        TOKEN_PROGRAM,
        TOKEN_READ,
        TOKEN_REPEAT,
        TOKEN_RPAREN,
        TOKEN_RSQUARE,
        TOKEN_SEMICOLON,
        TOKEN_SLASH,
        TOKEN_STAR,
        TOKEN_STRING,
        TOKEN_STRINGLIT,
        TOKEN_THEN,
        TOKEN_TRUE,
        TOKEN_UNTIL,
        TOKEN_VECTOR,
        TOKEN_WHILE,
        TOKEN_WRITE,
        TOKEN_WRITELN,
};

struct token {
        enum token_type type;
        int line;
        int linepos;
        char *start;
        int length;
};

struct lexer {
        char *program;
        int programlength;
        char *current;
        int line;
        int linepos;
};

void init_lexer(struct lexer *lexer, char *program, int programlength);
struct token next_token(struct lexer *lexer);
char *tokentypestring(enum token_type type);

union atomic {
        int ival;
        char *sval;
        int bval;
};

enum node_type {
        NODE_AND_EXPR,
        NODE_ASSIGN_STAT,
        NODE_BOOLEAN_CONST,
        NODE_BOOLEAN_TYPE,
        NODE_BREAK_STAT,
        NODE_COND_EXPR,
        NODE_DIVIDE_EXPR,
        NODE_NEG_EXPR,
        NODE_EQ_EXPR,
        NODE_EXIT_STAT,
        NODE_EXPR_BODY,
        NODE_EXPR_LIST,
        NODE_FORMAL_DECL,
        NODE_FOR_STAT,
        NODE_FUNCTION_DECL,
        NODE_FUNCTION_CALL,
        NODE_GREATEREQ_EXPR,
        NODE_GREATER_EXPR,
        NODE_ID_LIST,
        NODE_ID,
        NODE_IF_STAT,
        NODE_INDEXING,
        NODE_INTEGER_TYPE,
        NODE_INTGER_CONST,
        NODE_LESSEQ_EXPR,
        NODE_LHS,
        NODE_LESS_EXPR,
        NODE_MINUS_EXPR,
        NODE_MODE_INOUT,
        NODE_MODE_IN,
        NODE_MODE_OUT,
        NODE_MODULE_DECL_LIST,
        NODE_NEQ_EXPR,
        NODE_NOT_EXPR,
        NODE_ELSIF_EXPR_LIST,
        NODE_OR_EXPR,
        NODE_PLUS_EXPR,
        NODE_PROCEDURE_DECL,
        NODE_PROC_CALL_NODE,
        NODE_PROGRAM,
        NODE_READ_STAT,
        NODE_REPEAT_STAT,
        NODE_STAT_BODY,
        NODE_STAT_LIST,
        NODE_STRING_CONST,
        NODE_STRING_TYPE,
        NODE_TIMES_EXPR,
        NODE_UNARY_MINUS_EXPR,
        NODE_VAR_DECL_LIST,
        NODE_VAR_DECL,
        NODE_VECTOR_CONST,
        NODE_VECTOR_TYPE,
        NODE_WHILE_STAT,
        NODE_WRITELN_STAT,
        NODE_WRITE_STAT,
};

struct tree_node {
        enum node_type type;
        union atomic value;
        struct tree_node *next;
        struct tree_node *left;
        struct tree_node *right;
        struct tree_node *child;
};

void treeprint(struct tree_node *root);
struct tree_node *parse(char *program, int programlen);
void tree_node_free(struct tree_node *root);
char *nodetypestring(enum node_type type);
char *treenodestring(struct tree_node *node);

enum opcode {
        OP_LOCI, /* constants */
        OP_LOCS,

        OP_ADDI, /* integer arithmetic */
        OP_SUBI,
        OP_MULI,
        OP_DIVI,

        OP_IGRT, /* comparison */
        OP_ILET,
        OP_NEQUA,
        OP_EQUA,

        OP_AND, /* boolean logic */
        OP_OR,
        OP_NOT,
};

#endif