// SPDX-License-Identifier: GPL-2.0-only

/*
 * Built-in type descriptors
 */

#include <stdlib.h>
#include "type.h"

TypeDesc builtin_void = {
    .td_kind = TK_BUILTIN,
    // [GNU] Arithmetic on void* is the same as char*
    .td_sizeof = 1,
    .td_alignof = 1,
};

TypeDesc builtin_char = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 1,
    .td_alignof = 1,
};

TypeDesc builtin_schar = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 1,
    .td_alignof = 1,
};

TypeDesc builtin_uchar = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 1,
    .td_alignof = 1,
};

TypeDesc builtin_short = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 2,
    .td_alignof = 2,
};

TypeDesc builtin_ushort = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 2,
    .td_alignof = 2,
};

TypeDesc builtin_int = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 4,
    .td_alignof = 4,
};

TypeDesc builtin_uint = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 4,
    .td_alignof = 4,
};

TypeDesc builtin_long = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 8,
    .td_alignof = 8,
};

TypeDesc builtin_ulong = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 8,
    .td_alignof = 8,
};

TypeDesc builtin_llong = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 8,
    .td_alignof = 8,
};

TypeDesc builtin_ullong = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 8,
    .td_alignof = 8,
};

TypeDesc builtin_float = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 4,
    .td_alignof = 4,
};

TypeDesc builtin_double = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 8,
    .td_alignof = 8,
};

TypeDesc builtin_ldouble = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 16,
    .td_alignof = 16,
};

TypeDesc builtin_bool = {
    .td_kind = TK_BUILTIN,
    .td_sizeof = 1,
    .td_alignof = 1,
};
