/*
 * Lexical analyzer tests
 *
 * The way these work is we feed text blobs into the lexer and than have a series
 * of assert statements making sure we get back the desired tokens
 */

#include <assert.h>
#include <string.h>
#include <pp/io.h>
#include <pp/token.h>
#include <pp/lex.h>

static void assert_next_type(Io *io, TokenType type)
{
    int lineno;
    Token *tmp;

    lineno = 0;
    tmp = lex_next(io, 0, &lineno);
    assert(tmp && tmp->type == type);
    free_token(tmp);
}

static void assert_next_data(Io *io, TokenType type, const char *data)
{
    int lineno;
    Token *tmp;

    lineno = 0;
    tmp = lex_next(io, 0, &lineno);
    assert(tmp && tmp->type == type && !strcmp(tmp->data, data));
    free_token(tmp);
}

static void assert_next_space(Io *io, _Bool lnew, _Bool lwhite)
{
    int lineno;
    Token *tmp;

    lineno = 0;
    tmp = lex_next(io, 0, &lineno);
    assert(tmp && tmp->lnew == lnew && tmp->lwhite == lwhite);
}

static void assert_next_null(Io *io)
{
    int lineno;

    lineno = 0;
    assert(!lex_next(io, 0, &lineno));
}

// Test pre-processing numbers
static void test_ppnum(void)
{
    // These include valid numberic constants as well as other things,
    // e.g. 0xE+12 becoming one pre-processing token is intended behaviour
    Io *io = io_open_string(
        "0 123 05698 0x5555 0xE+12 .5555 0.5552 555ULL 55gggahHHH");

    size_t i;

    for (i = 0; i < 9; ++i)
        assert_next_type(io, TK_PP_NUMBER);

    assert_next_null(io);
    io_close(io);
}

// Escape sequences
static void test_esc(void)
{
    Io *io = io_open_string(
        "\"\\' \\\" \\? \\\\ \\a \\b \\f \\n \\r \\t \\v \\x7a \\122\" "
        "'0 1 2 3 4 5 6 7 8 9 \x41 \x42 \x43 \x44 E F'");

    assert_next_data(io,
            TK_STRING_LIT, "\' \" \? \\ \a \b \f \n \r \t \v z R");
    assert_next_data(io,
            TK_CHAR_CONST, "0 1 2 3 4 5 6 7 8 9 A B C D E F");
    assert_next_null(io);
    io_close(io);
}

// Punctuator parsing test
static void test_punct(void)
{
    Io *io = io_open_string(
        // Normal
        "[ ] ( ) { } . ->\n"
        "++ -- & * + - ~ !\n"
        "/ % << >> < > <= >= == != ^ | && ||\n"
        "? : ; ...\n"
        "= *= /= %= += -= <<= >>= &= ^= |=\n"
        ", # ##\n"
        // Digraph
        "<: :> <% %> %: %:%:\n"
        // Backtracking
        ".. .. %:%\n");

    static TokenType types[] = {
        // Normal
        TK_LEFT_SQUARE, TK_RIGHT_SQUARE, TK_LEFT_PAREN, TK_RIGHT_PAREN,
        TK_LEFT_CURLY, TK_RIGHT_CURLY, TK_MEMBER, TK_DEREF_MEMBER, TK_PLUS_PLUS,
        TK_MINUS_MINUS, TK_AMPERSAND, TK_STAR, TK_PLUS, TK_MINUS, TK_TILDE,
        TK_EXCL_MARK, TK_FWD_SLASH, TK_PERCENT, TK_LEFT_SHIFT, TK_RIGHT_SHIFT,
        TK_LEFT_ANGLE, TK_RIGHT_ANGLE, TK_LESS_EQUAL, TK_MORE_EQUAL,
        TK_EQUAL_EQUAL, TK_NOT_EQUAL, TK_CARET, TK_VERTICAL_BAR, TK_LOGIC_AND,
        TK_LOGIC_OR, TK_QUEST_MARK, TK_COLON, TK_SEMICOLON, TK_VARARGS,
        TK_EQUAL, TK_MUL_EQUAL, TK_DIV_EQUAL, TK_REM_EQUAL, TK_ADD_EQUAL,
        TK_SUB_EQUAL, TK_LSHIFT_EQUAL, TK_RSHIFT_EQUAL, TK_AND_EQUAL,
        TK_XOR_EQUAL, TK_OR_EQUAL, TK_COMMA, TK_HASH, TK_HASH_HASH,
        // Digraph
        TK_LEFT_SQUARE, TK_RIGHT_SQUARE, TK_LEFT_CURLY, TK_RIGHT_CURLY,
        TK_HASH, TK_HASH_HASH,
        // Make sure backtracking works correctly
        TK_MEMBER, TK_MEMBER, TK_MEMBER, TK_MEMBER,
        TK_HASH, TK_PERCENT
    };

    size_t i;

    for (i = 0; i < sizeof types / sizeof *types; ++i)
        assert_next_type(io, types[i]);

    assert_next_null(io);
    io_close(io);
}

// Token spacing test
static void test_spacing(void)
{
    Io *io = io_open_string(
        "1 2 3;\n4 \n5");

    assert_next_space(io, 0, 0);
    assert_next_space(io, 0, 1);
    assert_next_space(io, 0, 1);
    assert_next_space(io, 0, 0);
    assert_next_space(io, 1, 0);
    assert_next_space(io, 1, 1);

    assert_next_null(io);
    io_close(io);
}

int main(void)
{
    test_ppnum();
    test_esc();
    test_punct();
    test_spacing();
}
