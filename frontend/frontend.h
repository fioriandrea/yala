#ifndef fetypes_h
#define fetypes_h

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

#endif