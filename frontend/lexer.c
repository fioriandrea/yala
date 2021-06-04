#include <string.h>
#include <ctype.h>

#include "frontend.h"

void
init_lexer(struct lexer *lexer, char *program, int programlength)
{
        lexer->program = program;
        lexer->programlength = programlength;
        lexer->current = program;
        lexer->line = 1;
}

static void
set_token(struct token *token, struct lexer *lexer, enum token_type type, int length)
{
        token->type = type;
        token->start = lexer->current;
        token->length = length;
        token->line = lexer->line;
        lexer->current += length;
}

static void
set_error_token(struct token *token, struct lexer *lexer, char *message)
{
        token->type = TOKEN_ERROR;
        token->start = message;
        token->length = strlen(message);
        token->line = lexer->line;
}

static void
set_eof_token(struct token *token, struct lexer *lexer)
{
        token->type = TOKEN_EOF;
        token->line = lexer->line;
        token->start = lexer->current;
        token->length = 0;
}

static void
set_string_token(struct token *token, struct lexer *lexer)
{
        char *ptr;

        lexer->current++;
        ptr = lexer->current;
        while (*ptr != '\0' && *ptr != '"') {
                ptr++;
        }
        if (*ptr != '"') {
                set_error_token(token, lexer, "unterminated string");
        } else {
                set_token(token, lexer, TOKEN_STRING, ptr - lexer->current);
                lexer->current++;
        }
}

static void
set_int_token(struct token *token, struct lexer *lexer)
{
        char *ptr;

        ptr = lexer->current;
        while (isdigit(*ptr)) {
                ptr++;
        }
        set_token(token, lexer, TOKEN_INT, ptr - lexer->current);
}

static void
set_identifier_token(struct token *token, struct lexer *lexer)
{
        struct _keyword {
                char *name;
                int length;
                enum token_type type;
        };
        static struct _keyword keywords[] = {
                {"+", 1, TOKEN_AND},
                {"!", 1, TOKEN_BANG},
                {"begin", 5, TOKEN_BEGIN},
                {"break", 5, TOKEN_BREAK},
                {":", 1, TOKEN_COLON},
                {",", 1, TOKEN_COMMA},
                {"do", 1, TOKEN_DO},
                {"else", 4, TOKEN_ELSE},
                {"elsif", 5, TOKEN_ELSIF},
                {"end", 3, TOKEN_END},
                {"==", 2, TOKEN_EQ},
                {"exit", 4, TOKEN_EXIT},
                {"false", 5, TOKEN_FALSE},
                {"for", 3, TOKEN_FOR},
                {"function", 8, TOKEN_FUNCTION},
                {">", 1, TOKEN_GREATER},
                {">=", 2, TOKEN_GREATEREQ},
                {"if", 2, TOKEN_IF},
                {"inout", 5, TOKEN_INOUT},
                {"<", 1, TOKEN_LESS},
                {"<=", 2, TOKEN_LESSEQ},
                {"(", 1, TOKEN_LPAREN},
                {"[", 1, TOKEN_LSQUARE},
                {"-", 1, TOKEN_MINUS},
                {"!=", 2, TOKEN_NEQ},
                {"or", 2, TOKEN_OR},
                {"out", TOKEN_OUT},
                {"+", 1, TOKEN_PLUS},
                {"procedure", 9, TOKEN_PROCEDURE},
                {"program", 7, TOKEN_PROGRAM},
                {"read", 4, TOKEN_READ},
                {"repeat", 6, TOKEN_REPEAT},
                {")", 1, TOKEN_RPAREN},
                {"]", 1, TOKEN_RSQUARE},
                {";", 1, TOKEN_SEMICOLON},
                {"/", 1, TOKEN_SLASH},
                {"*", 1, TOKEN_STAR},
                {"true", 4, TOKEN_TRUE},
                {"then", 4, TOKEN_THEN},
                {"until", 5, TOKEN_UNTIL},
                {"while", 5, TOKEN_WHILE},
                {"write", 5, TOKEN_WRITE},
                {"writeln", 7, TOKEN_WRITELN},
                {NULL, 0, 0},
        };

        /* try to set token as a keyword */
        struct _keyword *kw;
        for (kw = keywords; kw->name != NULL; kw++) {
                int remaining = lexer->programlength - (lexer->current - lexer->program + 1);
                if (remaining >= kw->length && memcmp(kw->name, lexer->current, kw->length) == 0) {
                        set_token(token, lexer, kw->type, kw->length);
                        return;
                }
        }

        /* otherwise, set generic identifier (e.g. variable) */
        char *ptr = lexer->current;
        while (*ptr != '\0' && (isalpha(*ptr))) {
                ptr++;
        }
        set_token(token, lexer, TOKEN_ID, ptr - lexer->current);
}

struct token
next_token(struct lexer *lexer)
{
        struct token token;

        for (;;) {
                if (*lexer->current == '\0') {
                        set_eof_token(&token, lexer);
                        break;
                } else if (isspace(*lexer->current)) {
                        if (*lexer->current == '\n')
                                lexer->line++;
                        lexer->current++;
                        continue;
                } else if (*lexer->current == '"') {
                        set_string_token(&token, lexer);
                        break;
                } else if (isdigit(*lexer->current)) {
                        set_int_token(&token, lexer);
                        break;
                } else {
                        set_identifier_token(&token, lexer);
                        break;
                }
        }
        return token;
}