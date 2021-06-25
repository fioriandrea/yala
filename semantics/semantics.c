#include <stdarg.h>
#include <stdio.h>

#include "semantics.h"

static void semantics_error(int *result, struct tree_node *root, char *fmt, ...);

struct expr_type
analyze_semantics(int *result, struct tree_node *root)
{
        *result = 1;
        struct tree_node *child;
        struct expr_type lefttype, righttype, childtype0, childtype1;
        struct expr_type inttype, booltype;
        inttype.type = TYPE_INTEGER;
        booltype.type = TYPE_BOOLEAN;
        switch (root->type) {
        case NODE_AND_EXPR:
        case NODE_OR_EXPR:
                lefttype = analyze_semantics(result, root->left);
                righttype = analyze_semantics(result, root->right);
                if (lefttype.type != TYPE_BOOLEAN || righttype.type != TYPE_BOOLEAN) {
                        semantics_error(result, root, "operands must be booleans");
                }
                return booltype;
        case NODE_NOT_EXPR:
                lefttype = analyze_semantics(result, root->left);
                if (lefttype.type != TYPE_BOOLEAN) {
                        semantics_error(result, root, "operand must be a boolean");
                }
                return booltype;
        case NODE_PLUS_EXPR:
        case NODE_MINUS_EXPR:
        case NODE_TIMES_EXPR:
        case NODE_DIVIDE_EXPR:
                lefttype = analyze_semantics(result, root->left);
                righttype = analyze_semantics(result, root->right);
                if (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER) {
                        semantics_error(result, root, "operands must be integers");
                }
                return inttype;
        case NODE_NEG_EXPR:
                lefttype = analyze_semantics(result, root->left);
                if (lefttype.type != TYPE_INTEGER) {
                        semantics_error(result, root, "operand must be an integer");
                }
                return inttype;
        case NODE_EQ_EXPR:
        case NODE_NEQ_EXPR:
                lefttype = analyze_semantics(result, root->left);
                righttype = analyze_semantics(result, root->right);
                if (lefttype.type != righttype.type) {
                        semantics_error(result, root, "operands must be of the same type");
                }
                return booltype;
        case NODE_GREATEREQ_EXPR:
        case NODE_GREATER_EXPR:
        case NODE_LESSEQ_EXPR:
        case NODE_LESS_EXPR:
                lefttype = analyze_semantics(result, root->left);
                righttype = analyze_semantics(result, root->right);
                if (lefttype.type != TYPE_INTEGER || righttype.type != TYPE_INTEGER) {
                        semantics_error(result, root, "operands must be integers");
                }
                return inttype;
        case NODE_COND_EXPR:
                child = root->child;
                childtype0 = analyze_semantics(result, child);
                if (childtype0.type != TYPE_BOOLEAN) {
                        semantics_error(result, root, "if condition must be boolean");
                }
                child = child->next;
                childtype0 = analyze_semantics(result, child);
                while (child != NULL) {
                        childtype1 = analyze_semantics(result, child);
                        if (childtype0.type != childtype1.type) {
                                semantics_error(result, child, "conditional expression types must be the same");
                        }
                }
                return childtype0;
        case NODE_BOOLEAN_CONST:
                return booltype;
        case NODE_INTGER_CONST:
                return inttype;
        default:
                semantics_error(result, root, "semantic analysis for node not implemented (%s)", node_type_string(root->type));
        }
        return inttype;
}

static void
semantics_error(int *result, struct tree_node *root, char *fmt, ...)
{
        *result = 0;
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "semantic error ");
        fprintf(stderr, "[at %d:%d]: ", root->value.line, root->value.linepos);
        fprintf(stderr, "at '%.*s', ", root->value.length, root->value.start);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}
