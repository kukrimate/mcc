// SPDX-License-Identifier: GPL-2.0-only

#ifndef TOKEN_H
#define TOKEN_H

// Pre-processor token types
typedef enum {
    TK_NEW_LINE,        // New line

    TK_IDENTIFIER,      // Identifiers
    TK_PP_NUMBER,       // Pre-processing numbers

    TK_CHAR_CONST,      // Character constant
    TK_STRING_LIT,      // String literal

    TK_LEFT_SQUARE,     // [
    TK_RIGHT_SQUARE,    // ]
    TK_LEFT_PAREN,      // (
    TK_RIGHT_PAREN,     // )
    TK_LEFT_CURLY,      // {
    TK_RIGHT_CURLY,     // }
    TK_MEMBER,          // .
    TK_DEREF_MEMBER,    // ->
    TK_PLUS_PLUS,       // ++
    TK_MINUS_MINUS,     // --
    TK_AMPERSAND,       // &
    TK_STAR,            // *
    TK_PLUS,            // +
    TK_MINUS,           // -
    TK_TILDE,           // ~
    TK_EXCL_MARK,       // !
    TK_FWD_SLASH,       // /
    TK_PERCENT,         // %
    TK_LEFT_SHIFT,      // <<
    TK_RIGHT_SHIFT,     // >>
    TK_LEFT_ANGLE,      // <
    TK_RIGHT_ANGLE,     // >
    TK_LESS_EQUAL,      // <=
    TK_MORE_EQUAL,      // >=
    TK_EQUAL_EQUAL,     // ==
    TK_NOT_EQUAL,       // !=
    TK_CARET,           // ^
    TK_VERTICAL_BAR,    // |
    TK_LOGIC_AND,       // &&
    TK_LOGIC_OR,        // ||
    TK_QUEST_MARK,      // ?
    TK_COLON,           // :
    TK_SEMICOLON,       // ;
    TK_VARARGS,         // ...
    TK_EQUAL,           // =
    TK_MUL_EQUAL,       // *=
    TK_DIV_EQUAL,       // /=
    TK_REM_EQUAL,       // %=
    TK_ADD_EQUAL,       // +=
    TK_SUB_EQUAL,       // -=
    TK_LSHIFT_EQUAL,    // <<=
    TK_RSHIFT_EQUAL,    // >>=
    TK_AND_EQUAL,       // &=
    TK_XOR_EQUAL,       // ^=
    TK_OR_EQUAL,        // |=
    TK_COMMA,           // ,
    TK_HASH,            // #
    TK_HASH_HASH,       // ##

    TK_OTHER,           // Any other character
} TokenType;

// Pre-processor token flags
typedef struct {
    _Bool lwhite    : 1; // Is there whitespace to the left
    _Bool directive : 1; // Was this token at the beginning of a line
    _Bool no_expand : 1; // Token can't expand anymore
} TokenFlags;

#define TOKEN_NOFLAGS (TokenFlags) { 0 }

// Pre-processor token
typedef struct {
    int refcnt;    // Reference count
    TokenType type;   // Type of token
    TokenFlags flags; // Various token flags (used by the pre-processor)
    char *data;       // String data from the lexer
} Token;

// Create a new token
Token *create_token(TokenType type, TokenFlags flags, char *data);
// Create a new reference to a token
Token *ref_token(Token *token);
// Free a token
void free_token(Token *token);

// Output a token to the screen
void output_token(Token *token);

// List of pre-processor tokens
VEC_GEN(Token *, TokenList, token_list)
// Free a list of tokens (both the tokens in it and the list itself)
void token_list_freeall(TokenList *list);
// Extend the contents of a token list from another one, referencing all tokens
void token_list_refxtend(TokenList *list, TokenList *other);

// Concat the spellings of a list of tokens into a string
// Used for re-constructing the header name from a list of tokens making up
// a system header name (e.g. <stdio.h>)
char *concat_spellings(TokenList *tokens);

// Concat the spellings of a list of tokens into to a new string literal token
Token *stringize_operator(TokenList *tokens);
// Glue to tokens together to form one
Token *glue_operator(Token *left, Token *right);


#endif
