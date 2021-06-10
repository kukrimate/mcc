// SPDX-License-Identifier: GPL-2.0-only

#ifndef __MCC_STDARG_H
#define __MCC_STDARG_H

// glibc uses and requires this
#define __GNUC_VA_LIST 1
typedef __builtin_va_list __gnuc_va_list;

// va_list will be implemented via this
typedef __builtin_va_list va_list;

#endif
