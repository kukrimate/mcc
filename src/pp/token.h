// SPDX-License-Identifier: GPL-2.0-only

#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
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

    TK_PLACEMARKER,     // Placemarker (used when applying the ## opeartor)
} TokenType;

typedef struct {
    _Bool lwhite    : 1; // Is there whitespace to the left
    _Bool lnew      : 1; // Is there newline to the left
    _Bool directive : 1; // Was this token at the beginning of a line
    _Bool no_expand : 1; // Token can't expand anymore
} TokenFlags;

#define TOKEN_NOFLAGS (TokenFlags) { 0 }

// Token type
typedef struct Token Token;
struct Token {
    TokenType type;   // Type of token
    TokenFlags flags; // Various token flags (used by the pre-processor)
    char *data;       // String data from the lexer
    Token *next;      // Next token (if used as a list)
};

// Create a new token
Token *create_token(TokenType type, TokenFlags flags, char *data);

// Duplicate a token
Token *dup_token(Token *other);

// Duplicate a token list
Token *dup_tokens(Token *head);

// Free a token
void free_token(Token *token);

// Free a token list
void free_tokens(Token *head);

// Output a token to the screen
void output_token(Token *token);

// Convert a list of tokens to a string token
Token *stringize(Token *tokens);

// Glue to tokens together to form one
Token *glue(Token *left, Token *right);

// Concatenate the spelling of a list of tokens
char *concat_spellings(Token *head);

#endif
