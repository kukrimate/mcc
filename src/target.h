// SPDX-License-Identifier: GPL-2.0-only

/*
 * Types for representing values used in the execution environment
 * For now the target machine is assumed to be AMD64 with the following types:
 *  - char     : 8 bits, signed
 *  - short    : 16 bits
 *  - int      : 32 bits
 *  - long     : 64 bits
 *  - long long: 64 bits
 */

#ifndef TARGET_H
#define TARGET_H

#include <stdint.h>
#include <limits.h>

// Target integer types

// int
typedef int32_t t_int;
#define T_INT_MIN INT32_MIN
#define T_INT_MAX INT32_MAX

// unsigned int
typedef uint32_t t_uint;
#define T_UINT_MAX UINT32_MAX

// long
typedef int64_t t_long;
#define T_LONG_MIN INT64_MIN
#define T_LONG_MAX INT64_MAX

// unsigned long
typedef uint64_t t_ulong;
#define T_ULONG_MAX UINT64_MAX

// long long
typedef int64_t t_llong;
#define T_LLONG_MIN INT64_MIN
#define T_LLONG_MAX INT64_MAX

// unsigned long long
typedef uint64_t t_ullong;
#define T_ULLONG_MAX UINT64_MAX

// intmax_t
typedef int64_t t_imax;
#define T_IMAX_MAX INT64_MAX
#define T_IMAX_MIN INT64_MIN

// uintmax_t
typedef uint64_t t_umax;
#define T_UMAX_MAX UINT64_MAX

#endif
