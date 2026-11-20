/* Callvote hardening (sh1x security audit 2026-10-26): vote arguments that
 * reach Cbuf_AddText carry no command separator, and one client counts once.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "vote_guard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char long_name[80];

    /* Plain map and gametype names pass. */
    assert(VoteNameIsSafe("mp_killhouse_sh"));
    assert(VoteNameIsSafe("war"));
    assert(VoteNameIsSafe("mp_brl-derail"));
    /* Anything Cbuf, the tokenizer or a comment would interpret does not. */
    assert(!VoteNameIsSafe(""));
    assert(!VoteNameIsSafe(NULL));
    assert(!VoteNameIsSafe("war\nquit"));
    assert(!VoteNameIsSafe("war\rquit"));
    assert(!VoteNameIsSafe("war;quit"));
    assert(!VoteNameIsSafe("war quit"));
    assert(!VoteNameIsSafe("war\"x"));
    assert(!VoteNameIsSafe("war//x"));
    assert(!VoteNameIsSafe("war\\x"));
    assert(!VoteNameIsSafe("w\x01r"));
    assert(!VoteNameIsSafe("w\xffr"));
    memset(long_name, 'a', sizeof(long_name) - 1); long_name[sizeof(long_name) - 1] = 0;
    assert(!VoteNameIsSafe(long_name));
    long_name[64] = 0; assert(!VoteNameIsSafe(long_name)); /* 64 bytes */
    long_name[63] = 0; assert(VoteNameIsSafe(long_name));  /* 63 bytes */

    /* Every argument, including a kick vote's player name. */
    assert(VoteArgIsSafe("Some Player^1"));
    assert(VoteArgIsSafe(""));
    assert(!VoteArgIsSafe("map\nquit"));
    assert(!VoteArgIsSafe("map\r"));
    assert(!VoteArgIsSafe("a;b"));
    assert(!VoteArgIsSafe("a\"b"));
    assert(!VoteArgIsSafe("a\x7f"));
    assert(!VoteArgIsSafe(NULL));

    /* One cast per client per vote. */
    assert(!VoteHasCast(3, 5000));
    assert(VoteMarkCast(3, 5000));
    assert(VoteHasCast(3, 5000));
    assert(!VoteMarkCast(3, 5000));
    assert(!VoteMarkCast(3, 5000));
    assert(VoteMarkCast(4, 5000));
    /* A new vote (new voteTime) starts a new record. */
    assert(VoteMarkCast(3, 9000));
    assert(!VoteHasCast(3, 5000));
    /* No vote running, or a client outside the table: refused. */
    assert(!VoteMarkCast(3, 0));
    assert(!VoteMarkCast(-1, 9000));
    assert(!VoteMarkCast(64, 9000));
    puts("CoDOL callvote arguments carry no command separator and one client votes once");
    return 0;
}
