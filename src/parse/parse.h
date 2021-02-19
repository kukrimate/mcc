#ifndef PARSE_H
#define PARSE_H

// Target types are needed
#include "target.h"

// AST node type
typedef enum {
    // Identifier
    ND_IDENTIFIER,
    // Constant
    ND_CONSTANT,
    // String literal
    ND_STRING_LIT,
} NodeType;

// Type of constant
typedef enum {
    CT_INT,    // int
    CT_LONG,   // long (int)
    CT_LLONG,  // long long (int)
    CT_UINT,   // unsigned (int)
    CT_ULONG,  // unsigned long (int)
    CT_ULLONG, // unsigned long long (int)
} ConstType;

// Constant
typedef struct {
    ConstType type;
    t_umax    value;
} Const;

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
        // ND_CONSTANT
        Const constant;
    };
};

void pp_num_to_const(Token *pp_num, Const *out);

#endif
