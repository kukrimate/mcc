/*
 * AST debug functions
 */

#include <stdio.h>
#include <pp/token.h>
#include <pp/pp.h>
#include "target.h"
#include "parse.h"

#define AST_INDENT 2

static void _dump_ast(Node *root, int depth)
{
    // Print indentation
    for (int i = 0; i < depth * AST_INDENT; ++i)
        putchar(' ');

    switch (root->type) {
    // Constant
    case ND_CONST:
        printf("Const(%ld)\n", root->value);
        break;
    // Unary opeartors
    case ND_REF:
        printf("&\n");
        break;
    case ND_DEREF:
        printf("*\n");
        break;
    case ND_MINUS:
        printf("(unary) -\n");
        break;
    case ND_BIT_INV:
        printf("~\n");
        break;
    case ND_NOT:
        printf("!\n");
        break;
    // Binary operators
    case ND_MUL:
        printf("*\n");
        break;
    case ND_DIV:
        printf("/\n");
        break;
    case ND_MOD:
        printf("%%\n");
        break;
    case ND_ADD:
        printf("+\n");
        break;
    case ND_SUB:
        printf("-\n");
        break;
    case ND_LSHIFT:
        printf("<<\n");
        break;
    case ND_RSHIFT:
        printf(">>\n");
        break;
    case ND_LESS:
        printf("<\n");
        break;
    case ND_MORE:
        printf(">\n");
        break;
    case ND_LESS_EQ:
        printf("<=\n");
        break;
    case ND_MORE_EQ:
        printf(">=\n");
        break;
    case ND_EQ:
        printf("==\n");
        break;
    case ND_NEQ:
        printf("!=\n");
        break;
    case ND_AND:
        printf("&\n");
        break;
    case ND_XOR:
        printf("^\n");
        break;
    case ND_OR:
        printf("|\n");
        break;
    case ND_LAND:
        printf("&&\n");
        break;
    case ND_LOR:
        printf("||\n");
        break;
    // Assingment operators
    case ND_ASSIGN:
        printf("=\n");
        break;
    case ND_AS_MUL:
        printf("*=\n");
        break;
    case ND_AS_DIV:
        printf("/=\n");
        break;
    case ND_AS_MOD:
        printf("%%=\n");
        break;
    case ND_AS_ADD:
        printf("+=\n");
        break;
    case ND_AS_SUB:
        printf("-=\n");
        break;
    case ND_AS_LSH:
        printf("<<=\n");
        break;
    case ND_AS_RSH:
        printf(">>=\n");
        break;
    case ND_AS_AND:
        printf("&=\n");
        break;
    case ND_AS_XOR:
        printf("^=\n");
        break;
    case ND_AS_OR:
        printf("|=\n");
        break;
    // Others
    case ND_MEMBER:
        printf(".\n");
        break;
    case ND_CAST:
        printf("(cast)\n");
        break;
    case ND_COND:
        printf("?:\n");
        break;
    case ND_COMMA:
        printf(",\n");
        break;
    case ND_PRE_INC:
        printf("++ (pre)\n");
        break;
    case ND_PRE_DEC:
        printf("-- (pre)\n");
        break;
    case ND_POST_INC:
        printf("++ (post)\n");
        break;
    case ND_POST_DEC:
        printf("-- (post)\n");
        break;
    }

    // Children
    if (root->child1)
        _dump_ast(root->child1, depth + 1);
    if (root->child2)
        _dump_ast(root->child2, depth + 1);
    if (root->child3)
        _dump_ast(root->child3, depth + 1);
}

void dump_ast(Node *root)
{
    _dump_ast(root, 0);
}
