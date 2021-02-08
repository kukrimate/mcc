#ifndef LEX_H
#define LEX_H


typedef enum {
    /* Terminators */
    TK_END_FILE     = 0,
    TK_END_LINE     = 1,

    /* Identifiers */
    TK_HEADER_NAME  = 2,
    TK_IDENTIFIER   = 3,

    /* Constants */
    TK_PP_NUMBER    = 4,
    TK_CHAR_CONST   = 5,
    TK_STRING_LIT   = 6,

    /* Punctuators */
    TK_LEFT_SQUARE  = 0x100, /* [   */
    TK_RIGHT_SQUARE = 0x101, /* ]   */
    TK_LEFT_PAREN   = 0x102, /* (   */
    TK_RIGHT_PAREN  = 0x103, /* )   */
    TK_LEFT_CURLY   = 0x104, /* {   */
    TK_RIGHT_CURLY  = 0x105, /* }   */
    TK_MEMBER       = 0x106, /* .   */
    TK_DEREF_MEMBER = 0x107, /* ->  */
    TK_PLUS_PLUS    = 0x108, /* ++  */
    TK_MINUS_MINUS  = 0x109, /* --  */
    TK_AMPERSAND    = 0x10a, /* &   */
    TK_STAR         = 0x10b, /* *   */
    TK_PLUS         = 0x10c, /* +   */
    TK_MINUS        = 0x10d, /* -   */
    TK_TILDE        = 0x10e, /* ~   */
    TK_EXCL_MARK    = 0x10f, /* !   */
    TK_FWD_SLASH    = 0x110, /* /   */
    TK_PERCENT      = 0x111, /* %   */
    TK_LEFT_SHIFT   = 0x112, /* <<  */
    TK_RIGHT_SHIFT  = 0x113, /* >>  */
    TK_LEFT_ANGLE   = 0x114, /* <   */
    TK_RIGHT_ANGLE  = 0x115, /* >   */
    TK_LESS_EQUAL   = 0x116, /* <=  */
    TK_MORE_EQUAL   = 0x117, /* >=  */
    TK_EQUAL_EQUAL  = 0x118, /* ==  */
    TK_NOT_EQUAL    = 0x119, /* !=  */
    TK_CARET        = 0x11a, /* ^   */
    TK_VERTICAL_BAR = 0x11b, /* |   */
    TK_LOGIC_AND    = 0x11c, /* &&  */
    TK_LOGIC_OR     = 0x11d, /* ||  */
    TK_QUEST_MARK   = 0x11e, /* ?   */
    TK_COLON        = 0x11f, /* :   */
    TK_SEMICOLON    = 0x120, /* ;   */
    TK_VARARGS      = 0x121, /* ... */
    TK_EQUAL        = 0x122, /* =   */
    TK_MUL_EQUAL    = 0x123, /* *=  */
    TK_DIV_EQUAL    = 0x124, /* /=  */
    TK_REM_EQUAL    = 0x125, /* %=  */
    TK_ADD_EQUAL    = 0x126, /* +=  */
    TK_SUB_EQUAL    = 0x127, /* -=  */
    TK_LSHIFT_EQUAL = 0x128, /* <<= */
    TK_RSHIFT_EQUAL = 0x129, /* >>= */
    TK_AND_EQUAL    = 0x12a, /* &=  */
    TK_XOR_EQUAL    = 0x12b, /* ^=  */
    TK_OR_EQUAL     = 0x12c, /* |=  */
    TK_COMMA        = 0x12d, /* ,   */
    TK_HASH         = 0x12e, /* #   */
    TK_HASH_HASH    = 0x12f, /* ##  */
} token_type;

typedef struct {
    // Type of the token
	token_type type;
    // Number of whitespaces to the left
    size_t lwhite;
    // Original string (for identifiers/constants)
    const char *data;
    // Expansion disabled (preprocessor cruft that probably shouldnt be here)
    _Bool no_expand;
} token;

/* Get the next token from a char stream */
void
lex_next_token(FILE *fp, token *token);

// Punctuator to string table
extern const char *punctuator_str[];

#endif
