// SPDX-License-Identifier: ISC

#ifndef VEC_H
#define VEC_H

#include <stdlib.h>
#include <string.h>

// Minimum size
#ifndef VEC_MINSIZE
#define VEC_MINSIZE 8
#endif

// Grow factor when full
#ifndef VEC_GROW_FACTOR
#define VEC_GROW_FACTOR 2
#endif

//
// Generate type specific definitions
//
#define VEC_GEN(type, alias)                                                   \
                                                                               \
typedef struct {                                                               \
    size_t n;    /* Number of elements */                                      \
    size_t size; /* Size of the arr */                                         \
    type   *arr; /* The backing arr */                                         \
} Vec_##alias;                                                                 \
                                                                               \
static inline void vec_##alias##_init(Vec_##alias *self)                       \
{                                                                              \
    self->n = 0;                                                               \
    self->size = VEC_MINSIZE;                                                  \
    self->arr = reallocarray(NULL, self->size, sizeof(type));                  \
}                                                                              \
                                                                               \
static inline void vec_##alias##_free(Vec_##alias *self)                       \
{                                                                              \
    free(self->arr);                                                           \
}                                                                              \
                                                                               \
static inline void vec_##alias##_reserve(Vec_##alias *self, size_t size)       \
{                                                                              \
    self->size = size;                                                         \
    self->arr = reallocarray(self->arr, self->size, sizeof(type));             \
}                                                                              \
                                                                               \
static inline void vec_##alias##_add(Vec_##alias *self, type m)                \
{                                                                              \
    if (++self->n > self->size)                                                \
        vec_##alias##_reserve(self, self->n * VEC_GROW_FACTOR);                \
    self->arr[self->n - 1] = m;                                                \
}                                                                              \
                                                                               \
static inline void vec_##alias##_addall(Vec_##alias *self, type *m, size_t n)  \
{                                                                              \
    self->n += n;                                                              \
    if (self->n > self->size)                                                  \
        vec_##alias##_reserve(self, self->n * VEC_GROW_FACTOR);                \
    memcpy(self->arr + self->n - n, m, n * sizeof(type));                      \
}                                                                              \
                                                                               \
static inline type vec_##alias##_pop(Vec_##alias *self)                        \
{                                                                              \
    return self->arr[--self->n];                                               \
}                                                                              \
                                                                               \
static inline type vec_##alias##_top(Vec_##alias *self)                        \
{                                                                              \
    return self->arr[self->n - 1];                                             \
}

//
// String builder (character vector) type
//

VEC_GEN(char, char)

static inline char *vec_char_str(Vec_char *self)
{
    vec_char_add(self, 0);
    return self->arr;
}

#endif
