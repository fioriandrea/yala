#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "frontend.h"

void
lexer_init(struct lexer *lexer, char *program, int programlength)
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
        token->start = "EOF";
        token->length = 3;
        token->linepos = lexer->linepos;
}

static void
set_stringlit_token(struct token *token, struct lexer *lexer, char delim)
{
        struct lexer oldlexer;

        lexer->current++;
        oldlexer = *lexer;
        while (*lexer->current != '\0' && *lexer->current != delim) {
                if (*lexer->current == '\n') {
                        lexer->line++;
                        lexer->linepos = 0;
                }
                advance(lexer, 1);
        }
        if (*lexer->current != delim) {
                set_error_token(token, &oldlexer, "unterminated string");
        } else {
                set_token(token, &oldlexer, TOKEN_STRINGLIT, lexer->current - oldlexer.current);
                advance(lexer, 1);
        }
}

static void
set_integerlit_token(struct token *token, struct lexer *lexer)
{
        struct lexer oldlexer = *lexer;

        while (isdigit(*lexer->current))
                advance(lexer, 1);
        set_token(token, &oldlexer, TOKEN_INTEGERLIT, lexer->current - oldlexer.current);
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
                {".", 1, TOKEN_DOT},
                {NULL, 0, 0},
        };

        struct _keyoperator *ko;
        for (ko = operators; ko->name != NULL; ko++) {
                int remaining = lexer->programlength - (lexer->current - lexer->program);
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
                {"boolean", 7, TOKEN_BOOLEAN},
                {"break", 5, TOKEN_BREAK},
                {"do", 2, TOKEN_DO},
                {"to", 2, TOKEN_TO},
                {"else", 4, TOKEN_ELSE},
                {"elsif", 5, TOKEN_ELSIF},
                {"end", 3, TOKEN_END},
                {"exit", 4, TOKEN_EXIT},
                {"false", 5, TOKEN_FALSE},
                {"for", 3, TOKEN_FOR},
                {"function", 8, TOKEN_FUNCTION},
                {"if", 2, TOKEN_IF},
                {"inout", 5, TOKEN_INOUT},
                {"integer", 7, TOKEN_INTEGER},
                {"of", 2, TOKEN_OF},
                {"or", 2, TOKEN_OR},
                {"out", 3, TOKEN_OUT},
                {"procedure", 9, TOKEN_PROCEDURE},
                {"program", 7, TOKEN_PROGRAM},
                {"read", 4, TOKEN_READ},
                {"repeat", 6, TOKEN_REPEAT},
                {"string", 6, TOKEN_STRING},
                {"then", 4, TOKEN_THEN},
                {"true", 4, TOKEN_TRUE},
                {"until", 5, TOKEN_UNTIL},
                {"vector", 6, TOKEN_VECTOR},
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
        /* or else, set token as a generic id */
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
                } else if (*lexer->current == '"' || *lexer->current == '\'') {
                        set_stringlit_token(&token, lexer, *lexer->current);
                        break;
                } else if (isdigit(*lexer->current)) {
                        set_integerlit_token(&token, lexer);
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

char *
token_type_string(enum token_type type)
{
        switch (type) {
        case TOKEN_AND: return "TOKEN_AND";
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
        case TOKEN_BANG: return "TOKEN_BANG";
        case TOKEN_BEGIN: return "TOKEN_BEGIN";
        case TOKEN_BOOLEAN: return "TOKEN_BOOLEAN";
        case TOKEN_BREAK: return "TOKEN_BREAK";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_DO: return "TOKEN_DO";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_ELSE: return "TOKEN_ELSE";
        case TOKEN_ELSIF: return "TOKEN_ELSIF";
        case TOKEN_END: return "TOKEN_END";
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_EQ: return "TOKEN_EQ";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        case TOKEN_EXIT: return "TOKEN_EXIT";
        case TOKEN_FALSE: return "TOKEN_FALSE";
        case TOKEN_FOR: return "TOKEN_FOR";
        case TOKEN_FUNCTION: return "TOKEN_FUNCTION";
        case TOKEN_GREATEREQ: return "TOKEN_GREATEREQ";
        case TOKEN_GREATER: return "TOKEN_GREATER";
        case TOKEN_ID: return "TOKEN_ID";
        case TOKEN_IF: return "TOKEN_IF";
        case TOKEN_INOUT: return "TOKEN_INOUT";
        case TOKEN_INTEGERLIT: return "TOKEN_INTEGERLIT";
        case TOKEN_INTEGER: return "TOKEN_INTEGER";
        case TOKEN_LESSEQ: return "TOKEN_LESSEQ";
        case TOKEN_LESS: return "TOKEN_LESS";
        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_LSQUARE: return "TOKEN_LSQUARE";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_NEQ: return "TOKEN_NEQ";
        case TOKEN_OF: return "TOKEN_OF";
        case TOKEN_OR: return "TOKEN_OR";
        case TOKEN_OUT: return "TOKEN_OUT";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_PROCEDURE: return "TOKEN_PROCEDURE";
        case TOKEN_PROGRAM: return "TOKEN_PROGRAM";
        case TOKEN_READ: return "TOKEN_READ";
        case TOKEN_REPEAT: return "TOKEN_REPEAT";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_RSQUARE: return "TOKEN_RSQUARE";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_STAR: return "TOKEN_STAR";
        case TOKEN_STRINGLIT: return "TOKEN_STRINGLIT";
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_THEN: return "TOKEN_THEN";
        case TOKEN_TO: return "TOKEN_TO";
        case TOKEN_TRUE: return "TOKEN_TRUE";
        case TOKEN_UNTIL: return "TOKEN_UNTIL";
        case TOKEN_VECTOR: return "TOKEN_VECTOR";
        case TOKEN_WHILE: return "TOKEN_WHILE";
        case TOKEN_WRITELN: return "TOKEN_WRITELN";
        case TOKEN_WRITE: return "TOKEN_WRITE";
        }
        exit(100);
        return "";
}

int
token_equal(struct token t0, struct token t1)
{
        if (t0.length != t1.length)
                return 0;
        for (int i = 0; i < t0.length; i++) {
                if (t0.start[i] != t1.start[i])
                        return 0;
        }
        return 1;
}
