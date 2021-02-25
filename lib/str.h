#ifndef STR_H
#define STR_H

typedef struct {
    // Number of elements
    size_t n;
    // Size of array
    size_t size;
    // Underlying array
    char   *array;
} Str;

// Initialize a string builder object
void Str_init(Str *str);

// Append a character
void Str_add(Str *str, char ch);

// Append multiple characters
void Str_addall(Str *str, char *chars, size_t size);

// Convert string builder to a NUL-terminated string
char *Str_str(Str *str);

// Free the string builder
void Str_free(Str *str);

#endif
