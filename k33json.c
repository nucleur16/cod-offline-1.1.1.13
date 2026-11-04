/*
 * k33json.c - a minimal JSON reader for the services the ported codol-classic features talk to (theatre.c's
 * recording service, name_display.c's display-name service). Included by kernel33.c after k33http.c.
 *
 *   K33JsonGet(obj, key)  the text of key's value in the object at obj, NULL when absent (one level, no copy)
 *   K33JsonSkip(p)        past one value, NULL on malformed input
 *   K33JsonStr(p, ...)    a string value, escapes decoded (\uXXXX and surrogate pairs to UTF-8)
 *   K33JsonNum / K33JsonInt / K33JsonIsNull
 * Input must be NUL-terminated (K33HttpGet's bodies are).
 */

static const char *K33JsonWs(const char *p)
{
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return p;
}

static const char *K33JsonSkip(const char *p)
{
    p = K33JsonWs(p);
    if (!p || !*p) return NULL;
    if (*p == '"') {
        for (p++; *p && *p != '"'; p++)
            if (*p == '\\' && !*++p) return NULL;
        return *p ? p + 1 : NULL;
    }
    if (*p == '{' || *p == '[') {
        char close = *p == '{' ? '}' : ']';
        p = K33JsonWs(p + 1);
        if (p && *p == close) return p + 1;
        for (;;) {
            if (close == '}') {
                p = K33JsonWs(K33JsonSkip(p));   /* key */
                if (!p || *p != ':') return NULL;
                p++;
            }
            p = K33JsonWs(K33JsonSkip(p));
            if (!p) return NULL;
            if (*p == ',') { p++; continue; }
            return *p == close ? p + 1 : NULL;
        }
    }
    while (*p && !strchr(",}] \t\r\n", *p)) p++;
    return p;
}

__attribute__((unused)) static const char *K33JsonGet(const char *p, const char *key)
{
    p = K33JsonWs(p);
    if (!p || *p != '{') return NULL;
    p = K33JsonWs(p + 1);
    size_t klen = strlen(key);
    while (p && *p == '"') {
        const char *k = p + 1, *ke = K33JsonSkip(p);
        if (!ke) return NULL;
        BOOL match = (size_t)(ke - 1 - k) == klen && !memcmp(k, key, klen);
        p = K33JsonWs(ke);
        if (!p || *p != ':') return NULL;
        p = K33JsonWs(p + 1);
        if (match) return p;
        p = K33JsonWs(K33JsonSkip(p));
        if (!p || *p != ',') return NULL;
        p = K33JsonWs(p + 1);
    }
    return NULL;
}

static int K33JsonHex4(const char *p)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char h = p[i];
        int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v;
}

__attribute__((unused)) static BOOL K33JsonStr(const char *p, char *out, int n)
{
    out[0] = 0;
    p = K33JsonWs(p);
    if (!p || *p != '"') return FALSE;
    int o = 0;
    for (p++; *p && *p != '"'; p++) {
        unsigned cp = (unsigned char)*p;
        if (*p == '\\') {
            char c = *++p;
            switch (c) {
            case 'n': cp = '\n'; break;
            case 't': cp = '\t'; break;
            case 'r': cp = '\r'; break;
            case 'b': cp = '\b'; break;
            case 'f': cp = '\f'; break;
            case '"': case '\\': case '/': cp = (unsigned char)c; break;
            case 'u': {
                int hi = K33JsonHex4(p + 1);
                if (hi < 0) return FALSE;
                p += 4;
                cp = (unsigned)hi;
                if (cp >= 0xD800 && cp < 0xDC00) {
                    int lo = p[1] == '\\' && p[2] == 'u' ? K33JsonHex4(p + 3) : -1;
                    if (lo < 0xDC00 || lo > 0xDFFF) return FALSE;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + ((unsigned)lo - 0xDC00);
                    p += 6;
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return FALSE;
                }
                char enc[4];
                int m;
                if (cp < 0x80) { enc[0] = (char)cp; m = 1; }
                else if (cp < 0x800) { enc[0] = (char)(0xC0 | (cp >> 6)); enc[1] = (char)(0x80 | (cp & 63)); m = 2; }
                else if (cp < 0x10000) {
                    enc[0] = (char)(0xE0 | (cp >> 12)); enc[1] = (char)(0x80 | ((cp >> 6) & 63));
                    enc[2] = (char)(0x80 | (cp & 63)); m = 3;
                } else {
                    enc[0] = (char)(0xF0 | (cp >> 18)); enc[1] = (char)(0x80 | ((cp >> 12) & 63));
                    enc[2] = (char)(0x80 | ((cp >> 6) & 63)); enc[3] = (char)(0x80 | (cp & 63)); m = 4;
                }
                if (o + m >= n) return FALSE;
                memcpy(out + o, enc, m);
                o += m;
                continue;
            }
            default: return FALSE;
            }
        }
        if (o >= n - 1) return FALSE;
        out[o++] = (char)cp;
    }
    out[o] = 0;
    return *p == '"';
}

__attribute__((unused)) static BOOL K33JsonNum(const char *p, double *v)
{
    p = K33JsonWs(p);
    if (!p || !(*p == '-' || (*p >= '0' && *p <= '9'))) return FALSE;
    *v = strtod(p, NULL);
    return TRUE;
}

__attribute__((unused)) static int K33JsonInt(const char *obj, const char *key, int def)
{
    double v;
    return K33JsonNum(K33JsonGet(obj, key), &v) ? (int)v : def;
}

__attribute__((unused)) static BOOL K33JsonIsNull(const char *p)
{
    p = K33JsonWs(p);
    return p && !strncmp(p, "null", 4);
}
