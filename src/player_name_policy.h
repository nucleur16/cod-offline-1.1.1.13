/* player_name_policy.h - the pure parts of player_name.c (no C1 memory, no Windows calls): which values the console's
 * `name` accepts, and the per-profile names file. tests/player_name_policy_test.c checks them on the build machine. */
#pragma once
#include <stddef.h>
#include <string.h>

/* At most 31 bytes. The tightest consumers of the name: the lobby's admission gamertag (src/offline/dw_session.cpp
 * TagOk, 32 bytes) and the match server, which cleans a connecting player's name into 33 bytes with the NUL
 * (server/sh1x/src/sv_client.c, ClientCleanName(..., nick, 33, ...)). C1's own copies are 63 (party member +0x5A,
 * AS_GetPlayerName's buffer). */
#define PN_NAME_MAX 31

/* 1 when s[0..n) is well-formed UTF-8 (no overlong forms, no surrogates, at most U+10FFFF). */
static int PnUtf8Ok(const unsigned char *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned c = s[i];
        size_t len;
        unsigned cp;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07; }
        else return 0;
        if (i + len > n) return 0;
        for (size_t k = 1; k < len; k++) {
            if ((s[i + k] & 0xC0) != 0x80) return 0;
            cp = (cp << 6) | (s[i + k] & 0x3F);
        }
        if ((len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        i += len;
    }
    return 1;
}

/* 1 when s[0..n) is well-formed GBK / code page 936 (lead 0x81-0xFE, trail 0x40-0xFE but not 0x7F): what C1's own
 * text path (0x5C7340) and Kernel33's GBK input (PatchGbkInput) produce for a name typed in Chinese. */
static int PnGbkOk(const unsigned char *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned c = s[i];
        if (c < 0x80) { i++; continue; }
        if (c < 0x81 || c > 0xFE || i + 1 >= n) return 0;
        unsigned t = s[i + 1];
        if (t < 0x40 || t > 0xFE || t == 0x7F) return 0;
        i += 2;
    }
    return 1;
}

/* NULL when `s` may be the player's name, else why not (one line, for the console). The rules: 1..31 bytes; no
 * control bytes; none of " \ ; % (the userinfo / info-string separator, the command separator, quoting, C1's
 * printf-style text); no space at either end; something visible besides ^N colour codes; and the bytes are ASCII,
 * UTF-8 or GBK (name_display.c draws UTF-8 names, C1 draws GBK ones). */
static const char *PnNameProblem(const char *s)
{
    if (!s || !*s) return "a name cannot be empty";
    size_t n = strlen(s);
    if (n > PN_NAME_MAX) return "at most 31 bytes (a Chinese character takes 2 or 3)";
    int visible = 0, ascii = 1;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == 0x7F) return "no control characters";
        if (c == '"' || c == '\\' || c == ';' || c == '%') return "none of \" \\ ; %";
        if (c >= 0x80) ascii = 0;
        if (c == '^' && i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '9') { i++; continue; } /* a colour code */
        if (c != ' ') visible = 1;
    }
    if (s[0] == ' ' || s[n - 1] == ' ') return "no space at the start or the end";
    if (!visible) return "the name needs a visible character besides colour codes";
    if (!ascii && !PnUtf8Ok((const unsigned char *)s, n) && !PnGbkOk((const unsigned char *)s, n))
        return "the text is neither UTF-8 nor GBK";
    return NULL;
}

/* The names file (<players>\k33_names.txt): one line per profile, "<profile>\t<name>". A profile is the dw_username
 * that started the client; it is kept only when it has no tab, CR or LF and is not empty. */
static int PnProfileStorable(const char *profile)
{
    return profile && *profile && !strpbrk(profile, "\t\r\n") && strlen(profile) < 64;
}

/* The saved name of `profile` in the file text -> 1 and the name in out (cap bytes), else 0. */
static int PnNamesLookup(const char *text, const char *profile, char *out, size_t cap)
{
    size_t pl = strlen(profile);
    if (!text || !PnProfileStorable(profile) || !cap) return 0;
    for (const char *line = text; *line;) {
        const char *end = line + strcspn(line, "\r\n");
        if ((size_t)(end - line) > pl && !memcmp(line, profile, pl) && line[pl] == '\t') {
            size_t vl = (size_t)(end - line) - pl - 1;
            if (vl >= cap) return 0;
            memcpy(out, line + pl + 1, vl);
            out[vl] = 0;
            return 1;
        }
        line = end;
        while (*line == '\r' || *line == '\n') line++;
    }
    return 0;
}

/* The file text with `profile`'s line set to `name` (NULL: removed); the other lines are kept as they are, in order.
 * -> the length written to out, or (size_t)-1 when it does not fit in cap bytes (with the NUL). */
static size_t PnNamesEdit(const char *text, const char *profile, const char *name, char *out, size_t cap)
{
    size_t pl = strlen(profile), n = 0;
    int placed = 0;
#define PN_PUT(p, len) do { if (n + (len) >= cap) return (size_t)-1; memcpy(out + n, (p), (len)); n += (len); } while (0)
    for (const char *line = text ? text : ""; *line;) {
        const char *end = line + strcspn(line, "\r\n");
        size_t ll = (size_t)(end - line);
        int mine = ll > pl && !memcmp(line, profile, pl) && line[pl] == '\t';
        if (mine && name && !placed) {
            PN_PUT(profile, pl); PN_PUT("\t", 1); PN_PUT(name, strlen(name)); PN_PUT("\n", 1);
            placed = 1;
        } else if (!mine && ll) {
            PN_PUT(line, ll); PN_PUT("\n", 1);
        }
        line = end;
        while (*line == '\r' || *line == '\n') line++;
    }
    if (name && !placed) { PN_PUT(profile, pl); PN_PUT("\t", 1); PN_PUT(name, strlen(name)); PN_PUT("\n", 1); }
#undef PN_PUT
    if (n >= cap) return (size_t)-1;
    out[n] = 0;
    return n;
}
