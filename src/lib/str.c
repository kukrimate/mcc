/*
 * String builder for parsing
 */

#include <stdlib.h>
#include <string.h>
#include "str.h"

// Initialize a string builder object
void Str_init(Str *str)
{
    str->n = 0;
    str->size = 10;
    str->array = reallocarray(NULL, str->size, sizeof *str->array);
}

// Reserve space for 2n elements
static void Str_expand(Str *str)
{
    str->size = str->n * 2;
    str->array = reallocarray(str->array, str->size, sizeof *str->array);
}

// Append a character
void Str_add(Str *str, char ch)
{
    size_t new_idx;

    new_idx = str->n++;
    if (str->size < str->n)
        Str_expand(str);
    str->array[new_idx] = ch;
}

// Append multiple characters
void Str_addall(Str *str, char *chars, size_t size)
{
    size_t new_idx;

    new_idx = str->n;
    str->n += size;
    if (str->size < str->n)
        Str_expand(str);
    memcpy(str->array + new_idx, chars, size);
}

// Convert string builder to a NUL-terminated string
char *Str_str(Str *str)
{
    Str_add(str, 0);
    return str->array;
}

// Free the string builder
void Str_free(Str *str)
{
    free(str->array);
}
