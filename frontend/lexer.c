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
        lexer->linepos = 1;
}

static void
advance(struct lexer *lexer, int offset)
{
        lexer->current += offset;
        lexer->linepos += offset;
}

static void
set_token(struct token *token, struct lexer *lexer, enum token_type type, int length)
{
        token->type = type;
        token->start = lexer->current;
        token->length = length;
        token->linepos = lexer->linepos;
        token->line = lexer->line;
        //lexer->current += length;
        //lexer->linepos += length;
}

static void
set_error_token(struct token *token, struct lexer *lexer, char *message)
{
        token->type = TOKEN_ERROR;
        token->start = message;
        token->length = strlen(message);
        token->line = lexer->line;
        token->linepos = lexer->linepos;
}

static void
set_eof_token(struct token *token, struct lexer *lexer)
{
        token->type = TOKEN_EOF;
        token->line = lexer->line;
        token->start = lexer->current;
        token->length = 0;
        token->linepos = lexer->linepos;
}

static void
set_string_token(struct token *token, struct lexer *lexer)
{
        struct lexer oldlexer;

        lexer->current++;
        oldlexer = *lexer;
        while (*lexer->current != '\0' && *lexer->current != '"') {
                if (*lexer->current == '\n') {
                        lexer->line++;
                        lexer->linepos = 0;
                }
                advance(lexer, 1);
        }
        if (*lexer->current != '"') {
                set_error_token(token, &oldlexer, "unterminated string");
        } else {
                set_token(token, &oldlexer, TOKEN_STRING, lexer->current - oldlexer.current);
                advance(lexer, 1);
        }
}

static void
set_int_token(struct token *token, struct lexer *lexer)
{
        struct lexer oldlexer = *lexer;

        while (isdigit(*lexer->current))
                advance(lexer, 1);
        set_token(token, &oldlexer, TOKEN_INT, lexer->current - oldlexer.current);
}

static void
set_operator_token(struct token *token, struct lexer *lexer)
{
        struct _keyoperator {
                char *name;
                int length;
                enum token_type type;
        };
        static struct _keyoperator operators[] = {
                {"==", 2, TOKEN_EQ}, /* == before = to give == precedence */
                {"=", 1, TOKEN_ASSIGN},
                {"!=", 2, TOKEN_NEQ},
                {"!", 1, TOKEN_BANG},
                {":", 1, TOKEN_COLON},
                {",", 1, TOKEN_COMMA},
                {">=", 2, TOKEN_GREATEREQ},
                {">", 1, TOKEN_GREATER},
                {"<=", 2, TOKEN_LESSEQ},
                {"<", 1, TOKEN_LESS},
                {"(", 1, TOKEN_LPAREN},
                {"[", 1, TOKEN_LSQUARE},
                {"-", 1, TOKEN_MINUS},
                {"+", 1, TOKEN_PLUS},
                {")", 1, TOKEN_RPAREN},
                {"]", 1, TOKEN_RSQUARE},
                {";", 1, TOKEN_SEMICOLON},
                {"/", 1, TOKEN_SLASH},
                {"*", 1, TOKEN_STAR},
                {NULL, 0, 0},
        };

        struct _keyoperator *ko;
        for (ko = operators; ko->name != NULL; ko++) {
                int remaining = lexer->programlength - (lexer->current - lexer->program + 1);
                if (remaining >= ko->length && memcmp(ko->name, lexer->current, ko->length) == 0) {
                        set_token(token, lexer, ko->type, ko->length);
                        advance(lexer, ko->length);
                        return;
                }
        }
        set_error_token(token, lexer, "unexpected character");
        advance(lexer, 1);
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
                {"and", 3, TOKEN_AND},
                {"begin", 5, TOKEN_BEGIN},
                {"break", 5, TOKEN_BREAK},
                {"do", 1, TOKEN_DO},
                {"else", 4, TOKEN_ELSE},
                {"elsif", 5, TOKEN_ELSIF},
                {"end", 3, TOKEN_END},
                {"exit", 4, TOKEN_EXIT},
                {"false", 5, TOKEN_FALSE},
                {"for", 3, TOKEN_FOR},
                {"function", 8, TOKEN_FUNCTION},
                {"if", 2, TOKEN_IF},
                {"inout", 5, TOKEN_INOUT},
                {"of", 2, TOKEN_OF},
                {"or", 2, TOKEN_OR},
                {"out", TOKEN_OUT},
                {"procedure", 9, TOKEN_PROCEDURE},
                {"program", 7, TOKEN_PROGRAM},
                {"read", 4, TOKEN_READ},
                {"repeat", 6, TOKEN_REPEAT},
                {"true", 4, TOKEN_TRUE},
                {"then", 4, TOKEN_THEN},
                {"until", 5, TOKEN_UNTIL},
                {"while", 5, TOKEN_WHILE},
                {"write", 5, TOKEN_WRITE},
                {"writeln", 7, TOKEN_WRITELN},
                {NULL, 0, 0},
        };


        if (!isalpha(*lexer->current)) {
                set_error_token(token, lexer, "unexpected character");
                advance(lexer, 1);
                return;
        }
        struct lexer oldlexer = *lexer;
        while (*lexer->current != '\0' && (isalpha(*lexer->current) || isdigit(*lexer->current) || *lexer->current == '_'))
                advance(lexer, 1);
        
        /* try to set token as a keyword */
        struct _keyword *kw;
        for (kw = keywords; kw->name != NULL; kw++) {
                if (lexer->current - oldlexer.current == kw->length && memcmp(kw->name, oldlexer.current, kw->length) == 0) {
                        set_token(token, &oldlexer, kw->type, kw->length);
                        return;
                }
        }
        set_token(token, &oldlexer, TOKEN_ID, lexer->current - oldlexer.current);
}

static void
skip_comment(struct lexer *lexer)
{
        while (*lexer->current != '\0' && *lexer->current != '\n')
                advance(lexer, 1);
}

struct token
next_token(struct lexer *lexer)
{
        struct token token;

        for (;;) {
                if (*lexer->current == '\0') {
                        set_eof_token(&token, lexer);
                        break;
                } else if (*lexer->current == '#') {
                        skip_comment(lexer);
                } else if (isspace(*lexer->current)) {
                        if (*lexer->current == '\n') {
                                lexer->line++;
                                lexer->linepos = 0;
                        }
                        advance(lexer, 1);
                } else if (*lexer->current == '"') {
                        set_string_token(&token, lexer);
                        break;
                } else if (isdigit(*lexer->current)) {
                        set_int_token(&token, lexer);
                        break;
                } else if (isalpha(*lexer->current)) {
                        set_identifier_token(&token, lexer);
                        break;
                } else {
                        set_operator_token(&token, lexer);
                        break;
                }
        }
        return token;
}