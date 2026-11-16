/* player_name_policy.h: which names the console's `name` accepts and the per-profile names file, without the game.
 * Run by tests/run_tests.sh. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/player_name_policy.h"

int main(void)
{
    char out[256];
    int checks = 0;
#define CHECK(x) do { assert(x); checks++; } while (0)

    /* accepted */
    CHECK(!PnNameProblem("edtest"));
    CHECK(!PnNameProblem("New Name"));
    CHECK(!PnNameProblem("^1Red^7Name"));
    CHECK(!PnNameProblem("a"));
    CHECK(!PnNameProblem("1234567890123456789012345678901"));           /* 31 bytes */
    CHECK(!PnNameProblem("\xE4\xB8\xAD\xE6\x96\x87"));                     /* UTF-8 "zhongwen" */
    CHECK(!PnNameProblem("\xD6\xD0\xCE\xC4"));                             /* the same in GBK */
    CHECK(!PnNameProblem("[TAG]x/y-z.q_1"));
    /* refused */
    CHECK(PnNameProblem(NULL) && PnNameProblem(""));
    CHECK(PnNameProblem("12345678901234567890123456789012"));             /* 32 bytes */
    CHECK(PnNameProblem("a\"b") && PnNameProblem("a\\b") && PnNameProblem("a;b") && PnNameProblem("a%sb"));
    CHECK(PnNameProblem("a\tb") && PnNameProblem("a\nb") && PnNameProblem("a\x7f"));
    CHECK(PnNameProblem(" lead") && PnNameProblem("trail "));
    CHECK(PnNameProblem("^1^2") && PnNameProblem("^1 ^2"));
    CHECK(PnNameProblem("\xFF\xFF") && PnNameProblem("\x80"));
    CHECK(!PnNameProblem("\xC0\xAF"));                                     /* overlong as UTF-8, but valid GBK */
    CHECK(PnNameProblem("\xED\xA0\x80"));                                  /* a UTF-8 surrogate; not GBK either (trail 0x80 ok, 0x80 lead no) */
    CHECK(PnUtf8Ok((const unsigned char *)"\xF0\x9F\x98\x80", 4) && !PnUtf8Ok((const unsigned char *)"\xF0\x8F\x98\x80", 4));
    CHECK(PnGbkOk((const unsigned char *)"\x81\x40", 2) && !PnGbkOk((const unsigned char *)"\x81\x7F", 2) && !PnGbkOk((const unsigned char *)"\x81", 1));

    /* profiles that can be keys */
    CHECK(PnProfileStorable("edtest") && !PnProfileStorable("") && !PnProfileStorable("a\tb") && !PnProfileStorable(NULL));

    /* the names file */
    const char *text = "alpha\tAlpha One\r\nedtest\tOld\nbeta\tB\n";
    CHECK(PnNamesLookup(text, "edtest", out, sizeof(out)) && !strcmp(out, "Old"));
    CHECK(PnNamesLookup(text, "alpha", out, sizeof(out)) && !strcmp(out, "Alpha One"));
    CHECK(!PnNamesLookup(text, "alp", out, sizeof(out)));
    CHECK(!PnNamesLookup(text, "gamma", out, sizeof(out)));
    CHECK(!PnNamesLookup(NULL, "edtest", out, sizeof(out)));
    CHECK(!PnNamesLookup(text, "edtest", out, 3));                         /* does not fit: not found */
    CHECK(PnNamesEdit(text, "edtest", "New", out, sizeof(out)) != (size_t)-1 && !strcmp(out, "alpha\tAlpha One\nedtest\tNew\nbeta\tB\n"));
    CHECK(PnNamesEdit(text, "edtest", NULL, out, sizeof(out)) != (size_t)-1 && !strcmp(out, "alpha\tAlpha One\nbeta\tB\n"));
    CHECK(PnNamesEdit(text, "gamma", "G", out, sizeof(out)) != (size_t)-1 && !strcmp(out, "alpha\tAlpha One\nedtest\tOld\nbeta\tB\ngamma\tG\n"));
    CHECK(PnNamesEdit("", "edtest", "N", out, sizeof(out)) == strlen("edtest\tN\n") && !strcmp(out, "edtest\tN\n"));
    CHECK(PnNamesEdit(NULL, "edtest", NULL, out, sizeof(out)) == 0 && !out[0]);
    CHECK(PnNamesEdit("edtest\tA\nedtest\tB\n", "edtest", "C", out, sizeof(out)) != (size_t)-1 && !strcmp(out, "edtest\tC\n"));
    CHECK(PnNamesEdit(text, "edtest", "New", out, 10) == (size_t)-1);
    CHECK(PnNamesEdit("edtest\tNew\n", "edtest", "New", out, sizeof(out)) != (size_t)-1 &&
          PnNamesLookup(out, "edtest", out + 100, 100) && !strcmp(out + 100, "New"));

    printf("Player name policy: %d checks passed\n", checks);
    return 0;
}
