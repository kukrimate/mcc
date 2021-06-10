// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEX_H
#define LEX_H

Token *lex_next(Io *io, _Bool want_header_name, int *lineno);

#endif
