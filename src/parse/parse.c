/*
 * Parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "pp/token.h"
#include "parse.h"

#define ARRAY_SIZE(x) (sizeof x / sizeof *x)

typedef struct {
    const char *str;
    ConstType  type;
    t_umax     max;
} Suffix;

static Suffix SUFFIXES[] = {
    { ""   , CT_INT   , T_INT_MAX    }, // int
    { "L"  , CT_LONG  , T_LONG_MAX   }, // long
    { "LL" , CT_LLONG , T_LLONG_MAX  }, // long long
    { "U"  , CT_UINT  , T_UINT_MAX   }, // unsigned int
    { "UL" , CT_ULONG , T_ULONG_MAX  }, // unsigned long
    { "LU" , CT_ULONG , T_ULONG_MAX  }, // unsigned long
    { "ULL", CT_ULLONG, T_ULLONG_MAX }, // unsigned long long
    { "LLU", CT_ULLONG, T_ULLONG_MAX }, // unsigned long long
};

// Convert a pre-processing number to a constant
void pp_num_to_const(Token *pp_num, Const *out)
{
    char   *cur;
    t_umax result;
    size_t i;

    cur = pp_num->data;
    result = 0;

    // Read value
    switch (*cur) {
    case '0':
        if (*++cur == 'x') {
            // Hexadecimal
            while (*++cur)
                switch (*cur) {
                case '0' ... '9':
                    result = result << 4 | (*cur - '0');
                    break;
                case 'a' ... 'f':
                    result = result << 4 | (*cur - 'a' + 0xa);
                    break;
                case 'A' ... 'F':
                    result = result << 4 | (*cur - 'A' + 0xa);
                    break;
                default:
                    goto end;
                }
        } else {
            // Octal
            for (; *cur; ++cur)
                switch (*cur) {
                case '0' ... '7':
                    result = result << 3 | (*cur - '0');
                    break;
                default:
                    goto end;
                }
        }
        break;
    default:
        // Decimal
        for (; *cur; ++cur)
            switch (*cur) {
            case '0' ... '9':
                result = result * 10 | (*cur - '0');
                break;
            default:
                goto end;
            }
        break;
    }
    end:

    // Check suffix
    for (i = 0; i < ARRAY_SIZE(SUFFIXES); ++i) {
        if (!strcasecmp(SUFFIXES[i].str, cur)) {
            if (result > SUFFIXES[i].max) {
                pp_err("Integer constant overflows its type");
            }

            out->type = SUFFIXES[i].type;
            out->value = result;
            return;
        }
    }

    pp_err("Invalid character in integer constant");
}

static size_t parse_char_const(Token *char_const)
{
    char   *cur;
    size_t result;

    cur = char_const->data;
    result = 0;

    while (*cur)
        result = result << 8 | *cur++;

    return result;
}
