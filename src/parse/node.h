#ifndef NODE_H
#define NODE_H

// Target types are needed
#include "target.h"

// Built-in types
typedef enum {
    DT_BOOL,    // _Bool
    DT_CHAR,    // char
    // Signed
    DT_SCHAR,   // signed char
    DT_SHORT,   // short
    DT_INT,     // int
    DT_LONG,    // long
    DT_LLONG,   // long long
    // Unsigned
    DT_UCHAR,   // unsigned char
    DT_USHORT,  // unsigned short
    DT_UINT,    // unsigned int
    DT_ULONG,   // unsigned long
    DT_ULLONG,  // unsigned long long
    // Floating types
    DT_FLOAT,   // float
    DT_DOUBLE,  // double
    DT_LDOUBLE, // long double
} DataType;

// AST node type
typedef enum {
    // Keywords
    ND_AUTO,
    ND_BREAK,
    ND_CASE,
    ND_CHAR,
    ND_CONST,
    ND_CONTINUE,
    ND_DEFAULT,
    ND_DO,
    ND_DOUBLE,
    ND_ELSE,
    ND_ENUM,
    ND_EXTERN,
    ND_FLOAT,
    ND_FOR,
    ND_GOTO,
    ND_IF,
    ND_INLINE,
    ND_INT,
    ND_LONG,
    ND_REGISTER,
    ND_RESTRICT,
    ND_RETURN,
    ND_SHORT,
    ND_SIGNED,
    ND_SIZEOF,
    ND_STATIC,
    ND_STRUCT,
    ND_SWITCH,
    ND_TYPEDEF,
    ND_UNION,
    ND_UNSIGNED,
    ND_VOID,
    ND_VOLATILE,
    ND_WHILE,
    ND_BOOL,
    ND_COMPLEX,
    ND_IMAGINARY,
    // Identifier
    ND_IDENTIFIER,
    // Constant
    ND_CONSTANT,
    // String literal
    ND_STRING_LIT,
    // Punctuator
    ND_PUNCTUATOR
} NodeType;

// Node in the abstract syntax tree
typedef struct Node Node;
struct Node {
    // Type of this node
    NodeType type;
    // Next child of this node's parent node
    Node *next;
    // First child of this node
    Node *child;

    union {
        // ND_KEYWORD
        struct {
            // TODO: add keyword type
        } keyword;
        // ND_IDENTIFIER
        struct {
            char *data;
        } identifier;
        // ND_CONSTANT
        struct {
            DataType type;
            t_umax value;
        } constant;
        // ND_STRING_LIT
        struct {
            char *data;
        } string_lit;
        // ND_PUNCTUATOR
        struct {
            TokenType type;
        } punctuator;
    };
};

// Convert a pp-token to a parse node
Node *convert_token(Token *token);

#endif
