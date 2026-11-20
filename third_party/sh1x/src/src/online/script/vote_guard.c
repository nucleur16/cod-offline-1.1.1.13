/* Callvote hardening (sh1x security audit 2026-10-26). See vote_guard.h.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "vote_guard.h"
#include <string.h>

#define VOTE_CLIENTS 64

static int vote_cast_time[VOTE_CLIENTS];

int VoteNameIsSafe(const char *s)
{
    size_t n;
    if (!s) return 0;
    for (n = 0; s[n]; ++n) {
        unsigned char c = (unsigned char)s[n];
        if (n >= 63) return 0;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return n > 0;
}

int VoteArgIsSafe(const char *s)
{
    if (!s) return 0;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c == 0x7f || c == '"' || c == ';') return 0;
    }
    return 1;
}

int VoteHasCast(int clientnum, int voteTime)
{
    if (clientnum < 0 || clientnum >= VOTE_CLIENTS || !voteTime) return 0;
    return vote_cast_time[clientnum] == voteTime;
}

int VoteMarkCast(int clientnum, int voteTime)
{
    if (clientnum < 0 || clientnum >= VOTE_CLIENTS || !voteTime) return 0;
    if (vote_cast_time[clientnum] == voteTime) return 0;
    vote_cast_time[clientnum] = voteTime;
    return 1;
}
