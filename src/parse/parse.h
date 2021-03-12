#ifndef PARSE_H
#define PARSE_H

// Constant type
// enum {
//     CT_INT,
//     CT_UINT,
//     CT_LONG,
//     CT_ULONG,
//     CT_LLONG,
//     CT_ULLONG,
// };

typedef enum {
    // Constant
    ND_CONST,
    // Unary opeartors
    ND_REF,     // &foo
    ND_DEREF,   // *foo
    ND_MINUS,   // -
    ND_BIT_INV, // ~
    ND_NOT,     // !
    // Binary operators
    ND_MUL,     // *
    ND_DIV,     // /
    ND_MOD,     // %
    ND_ADD,     // +
    ND_SUB,     // -
    ND_LSHIFT,  // <<
    ND_RSHIFT,  // >>
    ND_LESS,    // <
    ND_MORE,    // >
    ND_LESS_EQ, // <=
    ND_MORE_EQ, // >=
    ND_EQ,      // ==
    ND_NEQ,     // !=
    ND_AND,     // &
    ND_XOR,     // ^
    ND_OR,      // |
    ND_LAND,    // &&
    ND_LOR,     // ||
    // Assingment operators
    ND_ASSIGN,  // =
    ND_AS_MUL,  // *=
    ND_AS_DIV,  // /=
    ND_AS_MOD,  // %=
    ND_AS_ADD,  // +=
    ND_AS_SUB,  // -=
    ND_AS_LSH,  // <<=
    ND_AS_RSH,  // >>=
    ND_AS_AND,  // &=
    ND_AS_XOR,  // ^=
    ND_AS_OR,   // |=
    // Others
    ND_MEMBER,  // foo.bar (foo->bar desugars to (*foo).bar)
    ND_CAST,    // (int) foo
    ND_COND,    // ?: (ternary conditional)
    ND_COMMA,   // , opeartor
    ND_PRE_INC, // ++i
    ND_PRE_DEC, // --i
    ND_POST_INC,// i++ (postfix increment)
    ND_POST_DEC,// i-- (postfix decrement)
} NodeType;

typedef struct Node Node;
struct Node {
    // Type of this node
    NodeType type;
    // Children of this node
    Node *child1, *child2, *child3;

    // ND_CONSTANT
    struct {
        t_umax value;
    };
};

// Parser context
typedef struct ParseCtx ParseCtx;

// Create a parser context
ParseCtx *parse_create(PpContext *pp);

// Close a parser context
void parse_free(ParseCtx *parse_ctx);

// Generate an abstract syntax tree from the parser context
void parse_run(ParseCtx *parse_ctx);

#endif
