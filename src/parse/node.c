/*
 * Parse node creation
 */

#include <stdio.h>
#include <stdlib.h>
#include "err.h"
#include "pp/token.h"
#include "node.h"

// Size of an array
#define ARRAY_SIZE(x) (sizeof x / sizeof *x)

// Create an AST node
static Node *create_node(NodeType type)
{
    Node *node;

    node = calloc(1, sizeof *node);
    node->type = type;
    return node;
}

// Type descriptors
typedef struct {
    DataType type;    // DataType
    _Bool usig;       // Is unsigned?
    t_umax max_value; // Max value
} DataTypeDesc;

static DataTypeDesc data_types[] = {
    { DT_INT,    0, T_INT_MAX    },
    { DT_UINT,   1, T_UINT_MAX   },
    { DT_LONG,   0, T_LONG_MAX   },
    { DT_ULONG,  1, T_ULONG_MAX  },
    { DT_LLONG,  0, T_LLONG_MAX  },
    { DT_ULLONG, 1, T_ULLONG_MAX },
};

// Minimum type indexes
#define MIN_INT 0
#define MIN_LONG 2
#define MIN_LLONG 4

// Get the first DataType big enough to fit value
static DataType get_type(size_t min_type, _Bool allow_usig,
                         _Bool allow_sig, t_umax value)
{
    size_t i;

    for (i = min_type; i < ARRAY_SIZE(data_types); ++i) {
        // Skip banned type categories
        if (!allow_usig && data_types[i].usig)
            continue;
        if (!allow_sig && !data_types[i].usig)
            continue;

        // Check if the value fits
        if (value <= data_types[i].max_value)
            return data_types[i].type;
    }

    // Value didn't fit in any type
    mcc_err("Integer constant overflows type");
}

// Convert a pre-processing number to an integer constant
static Node *convert_int_const(Token *pp_num)
{
    char   *cur;
    t_umax value;
    int    base;
    _Bool  suf_l, suf_ll, suf_u;
    Node   *node;

    cur = pp_num->data;
    value = 0;

    switch (*cur) {
    case '0':
        if (*++cur == 'x') {
            // Hexadecimal
            base = 16;
            while (*++cur)
                switch (*cur) {
                case '0' ... '9':
                    value = value << 4 | (*cur - '0');
                    break;
                case 'a' ... 'f':
                    value = value << 4 | (*cur - 'a' + 0xa);
                    break;
                case 'A' ... 'F':
                    value = value << 4 | (*cur - 'A' + 0xa);
                    break;
                default:
                    goto end_value;
                }
        } else {
            // Octal
            base = 8;
            for (; *cur; ++cur)
                switch (*cur) {
                case '0' ... '7':
                    value = value << 3 | (*cur - '0');
                    break;
                default:
                    goto end_value;
                }
        }
        break;
    default:
        // Decimal
        base = 10;
        for (; *cur; ++cur)
            switch (*cur) {
            case '0' ... '9':
                value = value * 10 | (*cur - '0');
                break;
            default:
                goto end_value;
            }
        break;
    }
end_value:

    // Look for suffix
    suf_l = suf_ll = suf_u = 0;
retry_suf:
    if (*cur == 'l' || *cur == 'L') {
        ++cur;

        if (suf_l) {
            // Double l is ll
            suf_l = 0;
            suf_ll = 1;
        } else if (suf_ll) {
            // Can't have more ls
            goto err;
        } else {
            // Single l
            suf_l = 1;
        }

        goto retry_suf;
    } else if (*cur == 'u' || *cur == 'U') {
        ++cur;

        // Can only have a single u
        if (suf_u)
            goto err;

        suf_u = 1;
        goto retry_suf;
    }

    // Check for unexpected chars
    if (*cur)
        goto err;

    // Create node
    node = create_node(ND_CONSTANT);
    node->constant.type = get_type(
            suf_ll ? MIN_LLONG : suf_l ? MIN_LONG : MIN_INT,
             base != 10 || suf_u, !suf_u, value);
    node->constant.value = value;
    return node;

err:
    mcc_err("Invalid character in integer constant");
}

// Convert character constant to node
static Node *char_const(Token *char_const)
{
    Node *node;
    char *cur;

    if (!*char_const->data)
        mcc_err("Empty character constant");

    node = create_node(ND_CONSTANT);
    node->constant.type = DT_INT;
    // Add all characters
    for (cur = char_const->data; *cur; ++cur)
        node->constant.value = node->constant.value << 8 | *cur;
    return node;
}

// Convert identifier (or keyword)
static Node *convert_identifier(Token *identifier)
{

}

// Convert a string literal
static Node *convert_string_lit(Token *string_lit)
{

}

// Convert a pp-token to a parse node
Node *convert_token(Token *token)
{
    Node *node;

    switch (token->type) {
    case TK_END_LINE:
    case TK_HEADER_NAME:
    case TK_PLACEMARKER:
        // It's a compiler bug if these tokens *ever* reach this function
        abort();
    case TK_IDENTIFIER:
        return identifier(token);
    case TK_PP_NUMBER:
        return int_const(token);
    case TK_CHAR_CONST:
        return char_const(token);
    case TK_STRING_LIT:
        return string_lit()
    default:
        node = create_node(ND_PUNCTUATOR);
        node->punctuator.type = token->type;
        return node;
    }
}
