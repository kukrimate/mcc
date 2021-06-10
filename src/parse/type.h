// SPDX-License-Identifier: GPL-2.0-only

/*
 * A reasonably abstract description of the type system
 */

#ifndef TYPE_H
#define TYPE_H

typedef struct TypeDesc TypeDesc;

// Aggregate (struct or union) member
typedef struct AggrMember AggrMember;
struct AggrMember {
    // Member type
    TypeDesc *am_type;
    // Member name
    const char *am_name;
    // Next member
    AggrMember *am_next;
};

// Type category
typedef enum {
    TK_BUILTIN, // Built-in type
    TK_STRUCT,  // Structure
    TK_UNION,   // Union
} TypeKind;

// Type declaration
struct TypeDesc {
    // Type kind
    TypeKind td_kind;

    // Values returned by the corresponding operators
    size_t td_sizeof;
    size_t td_alignof;

    // Struct or union members
    AggrMember *td_members;
};

// Built-in types
// NOTE: (_Complex is not supported and yes that is a violation of the standard
//        and yes I also do not care about that *at all*)

extern TypeDesc builtin_void;
extern TypeDesc builtin_char;
extern TypeDesc builtin_schar;
extern TypeDesc builtin_uchar;
extern TypeDesc builtin_short;
extern TypeDesc builtin_ushort;
extern TypeDesc builtin_int;
extern TypeDesc builtin_uint;
extern TypeDesc builtin_long;
extern TypeDesc builtin_ulong;
extern TypeDesc builtin_llong;
extern TypeDesc builtin_ullong;
extern TypeDesc builtin_float;
extern TypeDesc builtin_double;
extern TypeDesc builtin_ldouble;
extern TypeDesc builtin_bool;

#endif
