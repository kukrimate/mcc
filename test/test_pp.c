// SPDX-License-Identifier: GPL-2.0-only

/*
 * Pre-processor tests
 *
 * These work by pre-processing a string with directives and comparing it to
 * the expected pre-processing result
 */

#include <assert.h>
#include <string.h>
#include <pp/token.h>
#include <pp/pp.h>

// Assert that tokens will result in identical output
static void assert_identical(Token *t1, Token *t2)
{
    if (t1 == t2)
        return;

    assert(t1);
    assert(t2);
    assert(t1->type == t2->type);
    assert(t1->lnew == t2->lnew);
    assert(t1->lwhite == t2->lwhite);

    if (t1->data == t2->data)
        return;

    assert(t1->data);
    assert(t2->data);
    assert(!strcmp(t1->data, t2->data));
}

// Assert that two token streams are identical after pre-processing
static void assert_identical_result(const char *str1, const char *str2)
{
    PpContext *ctx1, *ctx2;
    Token *t1, *t2;

    ctx1 = pp_create();
    pp_push_string(ctx1, "test_pp1.c", str1);
    ctx2 = pp_create();
    pp_push_string(ctx2, "test_pp2.c", str2);

    for (;;) {
        t1 = pp_expand(ctx1);
        t2 = pp_expand(ctx2);
        assert_identical(t1, t2);
        if (!t1)
            break;
        free_token(t1);
        free_token(t2);
    }

    pp_free(ctx1);
    pp_free(ctx2);
}

int main(void)
{
    assert_identical_result(
        // C standard macro expansion example 3
        "#define x 3\n"
        "#define f(a) f(x * (a))\n"
        "#undef x\n"
        "#define x 2\n"
        "#define g f\n"
        "#define z z[0]\n"
        "#define h g(~\n"
        "#define m(a) a(w)\n"
        "#define w 0,1\n"
        "#define t(a) a\n"
        "#define p() int\n"
        "#define q(x) x\n"
        "#define r(x,y) x ## y\n"
        "#define str(x) # x\n"
        "\n"
        "f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\n"
        "g(x+(3,4)-w) | h 5) & m\n"
        "    (f)^m(m);\n"
        "p() i[q()] = { q(1), r(2, 3), r(4,), r(,5), r(,) };\n"
        "char c[2][6] = { str(hello), str() };\n",
        // Expected result
        "\n"
        "f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);\n"
        "f(2 * (2+(3,4)-0,1)) | f(2 * (~ 5)) & f(2 * (0,1))^m(0,1);\n"
        "int i[] = { 1, 23, 4, 5, };\n"
        "char c[2][6] = { \"hello\", \"\" };\n"
    );


    assert_identical_result(
        // C standard macro expansion example 4
        "#define hash_hash # ## #\n"
        "#define mkstr(a) # a\n"
        "#define in_between(a) mkstr(a)\n"
        "#define join(c, d) in_between(c hash_hash d)\n"
        "\n"
        "char p[] = join(x, y);\n",
        // Expected result
        "\nchar p[] = \"x ## y\";\n"
    );

    assert_identical_result(
        // C standard macro expansion example 5
        "#define t(x,y,z) x ## y ## z\n"
        "int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),\n"
        "            t(10,,), t(,11,), t(,,12), t(,,) };\n",
        // Expected result
        "\n"
        "int j[] = { 123, 45, 67, 89,\n"
        "            10, 11, 12, };\n"
    );

    assert_identical_result(
        // C standard varargs example
        "#define debug(...) fprintf(stderr, __VA_ARGS__)\n"
        "#define showlist(...) puts(#__VA_ARGS__)\n"
        "#define report(test, ...) ((test)?puts(#test):\\\n"
        "    printf(__VA_ARGS__))\n"
        "debug(\"Flag\");\n"
        "debug(\"X = %d\\n\", x);\n"
        "showlist(The first, second, and third items.);\n"
        "report(x>y, \"x is %d but y is %d\", x, y);\n",
        // Expected result
        "\n"
        "fprintf(stderr, \"Flag\");\n"
        "fprintf(stderr, \"X = %d\\n\", x);\n"
        "puts(\"The first, second, and third items.\");\n"
        "((x>y)?puts(\"x>y\"): printf(\"x is %d but y is %d\", x, y));\n"
    );

    assert_identical_result(
        // Simple macro expansion test
        "#define A(x, y, z) (x + y + z)\n"
        "A(1,5,2)\n"
        "A((1 + 1),2,3)\n"
        "\n"
        "#define X Y\n"
        "#define Y Z\n"
        "#define Z X\n"
        "X\n",
        // Expected result
        "\n"
        "(1 + 5 + 2)\n"
        "((1 + 1) + 2 + 3)\n"
        "\n"
        "X\n"
    );
}

