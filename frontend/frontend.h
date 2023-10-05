#ifndef frontend_h
#define frontend_h

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
        TOKEN_TO,
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

void lexer_init(struct lexer *lexer, char *program, int programlength);
struct token next_token(struct lexer *lexer);
char *token_type_string(enum token_type type);
int token_equal(struct token t0, struct token t1);

enum node_type {
        NODE_AND_EXPR,
        NODE_ASSIGN_STAT,
        NODE_BOOLEAN_CONST,
        NODE_BOOLEAN_TYPE,
        NODE_BREAK_STAT,
        NODE_COND_EXPR,
        NODE_CONDITION_AND_EXPRESSION,
        NODE_CONDITION_AND_STATEMENT,
        NODE_DECLARATION_BLOCKS,
        NODE_DIVIDE_EXPR,
        NODE_EQ_EXPR,
        NODE_EXIT_STAT,
        NODE_EXPR_BODY,
        NODE_EXPR_LIST,
        NODE_EXPR_STAT,
        NODE_FORMAL_DECL,
        NODE_FOR_STAT,
        NODE_FUNCTION_DECL,
        NODE_FUNCTION_TYPES,
        NODE_GREATEREQ_EXPR,
        NODE_GREATER_EXPR,
        NODE_ID,
        NODE_ID_LIST,
        NODE_IF_STAT,
        NODE_INDEXING,
        NODE_INTEGER_TYPE,
        NODE_INTGER_CONST,
        NODE_LESSEQ_EXPR,
        NODE_LESS_EXPR,
        NODE_MINUS_EXPR,
        NODE_MODE_IN,
        NODE_MODE_INOUT,
        NODE_MODE_OUT,
        NODE_MODULE_CALL,
        NODE_MODULE_DECL_LIST,
        NODE_NEG_EXPR,
        NODE_NEQ_EXPR,
        NODE_NOT_EXPR,
        NODE_OR_EXPR,
        NODE_PLUS_EXPR,
        NODE_PROCEDURE_DECL,
        NODE_PROGRAM,
        NODE_QUALIFIER,
        NODE_READ_STAT,
        NODE_REPEAT_STAT,
        NODE_RETURN_STAT,
        NODE_STAT_BODY,
        NODE_STAT_LIST,
        NODE_STRING_CONST,
        NODE_STRING_TYPE,
        NODE_TIMES_EXPR,
        NODE_VAR_DECL,
        NODE_VAR_DECL_LIST,
        NODE_VECTOR_CONST,
        NODE_VECTOR_TYPE,
        NODE_WHILE_STAT,
        NODE_WRITELN_STAT,
        NODE_WRITE_STAT,
};

struct tree_node {
        enum node_type type;
        struct token value;
        struct tree_node *next;
        struct tree_node *left;
        struct tree_node *right;
        struct tree_node *child;
};

void tree_node_print(struct tree_node *root);
struct tree_node *parse(char *program, int programlen);
struct tree_node *lhs_variable(struct tree_node *left);
void tree_node_free(struct tree_node *root);
char *node_type_string(enum node_type type);

#endif
