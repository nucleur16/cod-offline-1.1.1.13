/* Callvote hardening (sh1x security audit 2026-10-26).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef VOTE_GUARD_H
#define VOTE_GUARD_H

/* 1 when every byte of a map or gametype vote argument is [A-Za-z0-9_-]
 * and it is 1..63 bytes long. The vote string is later run through
 * Cbuf_AddText, which splits commands on newlines even inside quotes, so the
 * argument must never carry a separator, quote, comment or control byte. */
int VoteNameIsSafe(const char *s);

/* 1 when the argument carries no byte Cbuf_AddText or the tokenizer treats
 * as structure: control bytes (newline, CR), DEL, '"', ';'. Used for every
 * callvote argument, including a kick vote's player name (matched only). */
int VoteArgIsSafe(const char *s);

/* Per-client vote record for the vote identified by voteTime. IW3 kept the
 * "already voted" bit in eFlags (0x100000), which is C1's EF_VEHICLE_ACTIVE,
 * so CoDOL never set it and one client could vote yes repeatedly.
 * VoteMarkCast returns 1 the first time a client votes in this vote and
 * 0 on every repeat; an out-of-range client is refused. */
int VoteMarkCast(int clientnum, int voteTime);
int VoteHasCast(int clientnum, int voteTime);

#endif
