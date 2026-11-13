/*
 * name_display.c - player names in UTF-8 and an optional display-name service, ported from codol-classic's
 * d3d9.dll (NameDisplay.cpp). Included by kernel33.c after k33json.c.
 *
 * C1 converts every UI string to a Scaleform string through 0x5C7340 (output ESI, one caller-popped stack argument,
 * returns ESI in EAX), which reads it as code page 936 (GBK). A player name that arrives as UTF-8 (a dw_username
 * typed in UTF-8, or a lobby/service name) therefore shows as mojibake. Kernel33's GBK input (kernel33.c,
 * PatchGbkInput) is unchanged: names typed in the game are GBK and stay on the original path.
 *
 * UTF-8 names (on by default; "+set codol_name_utf8 0" on the command line installs none of them):
 *   0x5AA7CF  local player name (AS_GetPlayerName, from dw_username)  call 0x5C7340 -> NdConvertName
 *   0x5B4E41  scoreboard player names                                 call 0x5C7340 -> NdConvertName
 *     A string that is valid UTF-8 and has a non-ASCII byte is built with GFx's own UTF-8 constructor 0x744AA0 on
 *     the current movie (in a match the HUD movie [0x810C50C], else the front end [0x810C2A4]; byte 0x6C44D6C says
 *     which); ASCII, GBK, invalid UTF-8 and a missing movie take the original 0x5C7340 path.
 *   0x469453 / 0x469492  the e / f HUD notifications: only an exact MP_CONNECTED message with one raw name segment
 *     (after 0x15) has that segment re-encoded UTF-8 -> CP936 before the original formatter 0x448E10.
 *   0x44477D / 0x4447DD  obituary victim / attacker: after the original name getter 0x488AF0 (EAX index, ESI
 *     70-byte buffer), a valid UTF-8 name is re-encoded to CP936 in that private buffer.
 *   The CP936 re-encodings run only while the game language (dvar [0x2800A80] +0xC) is 11, Chinese (C1's HUD font
 *   path is CP936 in both Kernel33 languages); a character CP936 lacks keeps the whole original text.
 *   A GBK name whose bytes also happen to be valid UTF-8 (rare: every lead byte 0xC2-0xDF followed by a trail
 *   0x80-0xBF) is read as UTF-8; codol-classic accepts the same ambiguity.
 *
 * Display-name service (off unless "+set codol_name_display_url <https://host>" is on the command line): every 30 s
 * GET <url>/api/community/display-name?identity=<dw_username>, no credentials, no redirects, at most 4 KB; the
 * reply {identity, uid (decimal string or null), name} must name this exact dw_username and, when uid is a string,
 * this client's UID ([0xCEA9F8], else [0x6C44F58] once [0x6C44F27] is set). The alias (valid UTF-8, at most 63
 * bytes, no controls) is shown for 60 s after each good reply, only for the local player, in three more places:
 *   0x5AA780  the local name source (dw_username's string)              -> NdLocalSource, back at 0x5AA786
 *   0x69598A  room member rows (the row whose UID and name are ours,     -> NdMemberSource, back at 0x695990
 *             a "[clan] " prefix kept)
 *   0x60AD94  the lobby avatar label (CP936 into its 70-byte stack label) -> NdLobbySource, 0x60ADE7 / 0x60AD9A
 * dw_username, the UID, realName fields and everything sent to servers stay canonical. This project's lobby has no
 * such service: codol-classic's (https://codol.kewkew.org) maps ITS accounts' names, so pointing a client of
 * another lobby at it would show other people's aliases for matching names.
 *
 * Every site is byte-checked first (codol-classic gated on its patched exe's SHA-256); all are patched before the game
 * starts (TakeOver), so no thread can be inside them.
 */

#define ND_CONVERT_VA       0x005C7340u
#define ND_GFX_UTF8_VA      0x00744AA0u
#define ND_HUD_FORMAT_VA    0x00448E10u
#define ND_OBIT_NAME_VA     0x00488AF0u
#define ND_LANGUAGE_PTR_VA  0x02800A80u
#define ND_FRONT_MOVIE_VA   0x0810C2A4u
#define ND_HUD_MOVIE_VA     0x0810C50Cu
#define ND_IN_GAME_VA       0x06C44D6Cu
#define ND_USERNAME_PTR_VA  0x06C26318u
#define ND_UID_VA           0x00CEA9F8u
#define ND_UID2_READY_VA    0x06C44F27u
#define ND_UID2_VA          0x06C44F58u

__attribute__((used)) DWORD g_ndHudOrig = ND_HUD_FORMAT_VA;
__attribute__((used)) DWORD g_ndObitOrig = ND_OBIT_NAME_VA;
__attribute__((used)) DWORD g_ndSourceCont = 0x005AA786u;
__attribute__((used)) DWORD g_ndMemberCont = 0x00695990u;
__attribute__((used)) DWORD g_ndLobbyCont = 0x0060AD9Au;
__attribute__((used)) DWORD g_ndLobbyAlias = 0x0060ADE7u;

static BOOL g_ndAliasOn;
static char g_ndUrl[256];
static CRITICAL_SECTION g_ndLock;
static char g_ndCanonical[64];           /* dw_username the alias belongs to */
static unsigned long long g_ndUid;
static unsigned g_ndGeneration;          /* bumps when the identity changes; stale replies are dropped */
static char g_ndAlias[64];
static DWORD g_ndAliasAt;
static volatile LONG g_ndBusy;
static char g_ndAliasCopy[80], g_ndHudCopy[1024];

/* ---- text ------------------------------------------------------------------------------------------------- */

static BOOL NdCopy(const char *s, char *out, int n)
{
    if (!s || !Readable(s, 1)) return FALSE;
    if (Readable(s, n)) {                         /* the usual case: one region check */
        for (int i = 0; i < n; i++)
            if (!(out[i] = s[i])) return TRUE;
        return FALSE;
    }
    for (int i = 0; i < n; i++) {
        if (!Readable(s + i, 1)) return FALSE;
        out[i] = s[i];
        if (!s[i]) return TRUE;
    }
    return FALSE;
}

/* Valid UTF-8, 1..63 bytes, no controls; *extended when it has a non-ASCII byte. */
static BOOL NdValidName(const char *s, BOOL *extended)
{
    size_t n = strlen(s);
    BOOL ext = FALSE;
    if (!n || n > 63) return FALSE;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == 127) return FALSE;
        ext |= c >= 128;
    }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)n, NULL, 0)) return FALSE;
    if (extended) *extended = ext;
    return TRUE;
}

/* UTF-8 (with a non-ASCII byte) -> CP936, refusing any character CP936 lacks. */
static BOOL NdToCp936(const char *src, int size, char *out, int outSize)
{
    WCHAR wide[1024];
    BOOL ext = FALSE, replaced = FALSE;
    if (size <= 0 || size >= 1024) return FALSE;
    for (int i = 0; i < size; i++) ext |= (unsigned char)src[i] >= 128;
    if (!ext) return FALSE;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, size, wide, 1024);
    if (!count) return FALSE;
    int len = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wide, count, out, outSize - 1, NULL, &replaced);
    if (!len || replaced) return FALSE;
    out[len] = 0;
    return TRUE;
}

static BOOL NdChineseFont(void)
{
    const BYTE *d = *(const BYTE *const *)(DWORD_PTR)ND_LANGUAGE_PTR_VA;
    return d && Readable(d, 0x10) && *(const int *)(d + 0xC) == 11;
}

/* ---- UTF-8 names -------------------------------------------------------------------------------------------- */

static void *NdOriginalConvert(void *out, const char *text)
{
    void *r;
    DWORD fn = ND_CONVERT_VA;
    __asm__ volatile("pushl %[t]\n\tcall *%[fn]\n\taddl $4, %%esp"
                     : "=&a"(r), "+S"(out) : [t] "r"(text), [fn] "r"(fn) : "ecx", "edx", "memory", "cc");
    return r;
}

__attribute__((used)) void *__cdecl NdConvert(void *out, const char *text)
{
    char copy[128];
    BOOL ext = FALSE;
    if (!NdCopy(text, copy, sizeof(copy)) || !NdValidName(copy, &ext) || !ext) return NdOriginalConvert(out, text);
    BYTE inGame = *(volatile const BYTE *)(DWORD_PTR)ND_IN_GAME_VA;
    void *movie = *(void *const *)(DWORD_PTR)(inGame ? ND_HUD_MOVIE_VA : ND_FRONT_MOVIE_VA);
    if (!movie || !Readable(movie, 0x10)) return NdOriginalConvert(out, text);
    ((DWORD *)out)[0] = 0;
    ((DWORD *)out)[1] = 0;
    ((void(__attribute__((thiscall)) *)(void *, void *, const char *))(DWORD_PTR)ND_GFX_UTF8_VA)(movie, out, text);
    return out;
}

/* e / f notification text: only MP_CONNECTED with one raw name segment gets that segment as CP936. */
__attribute__((used)) const char *__cdecl NdHudMessage(const char *original)
{
    char copy[1024];
    if (!NdChineseFont() || !NdCopy(original, copy, sizeof(copy))) return original;
    BOOL localized = TRUE, connected = FALSE;
    size_t nameAt = 0, nameSize = 0, length = strlen(copy);
    for (size_t at = 0; at < length;) {
        unsigned char m = (unsigned char)copy[at];
        if (m == 0x14 || m == 0x15 || m == 0x16) {
            if (m == 0x16) return original;
            localized = m == 0x14;
            at++;
            continue;
        }
        size_t end = at;
        while (end < length && copy[end] != 0x14 && copy[end] != 0x15 && copy[end] != 0x16) end++;
        if (localized) {
            if (connected || end - at != 12 || memcmp(copy + at, "MP_CONNECTED", 12)) return original;
            connected = TRUE;
        } else {
            if (nameSize || end - at > 69) return original;
            nameAt = at;
            nameSize = end - at;
            for (size_t i = at; i < end; i++)
                if ((unsigned char)copy[i] < 32 || (unsigned char)copy[i] == 127) return original;
        }
        at = end;
    }
    char enc[140];
    if (!connected || !nameSize || !NdToCp936(copy + nameAt, (int)nameSize, enc, sizeof(enc))) return original;
    size_t el = strlen(enc);
    if (length - nameSize + el >= sizeof(g_ndHudCopy)) return original;
    memcpy(g_ndHudCopy, copy, nameAt);
    memcpy(g_ndHudCopy + nameAt, enc, el);
    memcpy(g_ndHudCopy + nameAt + el, copy + nameAt + nameSize, length - nameAt - nameSize + 1);
    return g_ndHudCopy;
}

__attribute__((used)) void __cdecl NdObituaryName(char *output)
{
    char copy[70], enc[70];
    if (!NdChineseFont() || !NdCopy(output, copy, sizeof(copy))) return;
    if (!NdToCp936(copy, (int)strlen(copy), enc, sizeof(enc))) return;
    memcpy(output, enc, strlen(enc) + 1);
}

/* ---- the display-name service ------------------------------------------------------------------------------- */

static unsigned long long NdLocalUid(void)
{
    unsigned long long uid = *(volatile const unsigned long long *)(DWORD_PTR)ND_UID_VA;
    if (uid) return uid;
    if (!*(volatile const BYTE *)(DWORD_PTR)ND_UID2_READY_VA) return 0;
    return *(volatile const unsigned long long *)(DWORD_PTR)ND_UID2_VA;
}

static BOOL NdCanonical(char *out, int n)
{
    const BYTE *d = *(const BYTE *const *)(DWORD_PTR)ND_USERNAME_PTR_VA;
    if (!d || !Readable(d, 0x10)) return FALSE;
    if (!NdCopy(*(const char *const *)(d + 0xC), out, n) || !out[0]) return FALSE;
    for (const char *p = out; *p; p++)
        if ((unsigned char)*p < 33 || (unsigned char)*p > 126) return FALSE;
    return TRUE;
}

/* The alias for (canonical, uid) when fresh, copied into out; FALSE otherwise. Any thread. */
static BOOL NdAliasFor(const char *canonical, unsigned long long uid, char *out, int n)
{
    BOOL ok = FALSE;
    if (!g_ndAliasOn || !uid) return FALSE;
    EnterCriticalSection(&g_ndLock);
    if (g_ndAlias[0] && g_ndUid == uid && !strcmp(g_ndCanonical, canonical) && GetTickCount() - g_ndAliasAt < 60000) {
        lstrcpynA(out, g_ndAlias, n);
        ok = TRUE;
    }
    LeaveCriticalSection(&g_ndLock);
    return ok;
}

__attribute__((used)) const char *__cdecl NdSelectAlias(const char *original)
{
    char canonical[64];
    if (!NdCopy(original, canonical, sizeof(canonical)) || !NdAliasFor(canonical, NdLocalUid(), g_ndAliasCopy, 64))
        return original;
    return g_ndAliasCopy;
}

/* A room member row: entry+0 UID, entry+8 name (EDI). Only our own row with our exact name (or "[clan] name"). */
__attribute__((used)) const char *__cdecl NdSelectMemberAlias(const char *original, const void *member)
{
    char raw[80], canonical[64], alias[64];
    unsigned long long uid = NdLocalUid();
    if (!uid || !Readable(member, 8) || *(const unsigned long long *)member != uid) return original;
    if (!NdCanonical(canonical, sizeof(canonical)) || !NdCopy(original, raw, sizeof(raw))) return original;
    size_t prefix = 0;
    if (strcmp(raw, canonical)) {
        const char *close = raw[0] == '[' ? strstr(raw, "] ") : NULL;
        if (!close || strcmp(close + 2, canonical)) return original;
        prefix = (size_t)(close + 2 - raw);
    }
    if (!NdAliasFor(canonical, uid, alias, sizeof(alias))) return original;
    if (prefix + strlen(alias) >= sizeof(g_ndAliasCopy)) return original;
    memcpy(g_ndAliasCopy, raw, prefix);
    lstrcpyA(g_ndAliasCopy + prefix, alias);
    return NdValidName(g_ndAliasCopy, NULL) ? g_ndAliasCopy : original;
}

/* The lobby avatar label: EBX = the row's name (row UID at name-8); output = its 70-byte stack label. */
__attribute__((used)) BOOL __cdecl NdLobbyLabel(const char *original, char *output)
{
    const char *selected = NdSelectMemberAlias(original, original - 8);
    if (selected == original) return FALSE;
    char enc[70];
    if (!NdToCp936(selected, (int)strlen(selected), enc, sizeof(enc))) {
        /* an ASCII alias needs no conversion */
        BOOL ext = TRUE;
        if (!NdValidName(selected, &ext) || ext || strlen(selected) >= sizeof(enc)) return FALSE;
        lstrcpyA(enc, selected);
    }
    memcpy(output, enc, strlen(enc) + 1);
    return TRUE;
}

typedef struct NdJob { char url[512], canonical[64]; unsigned long long uid; unsigned generation; } NdJob;

static DWORD WINAPI NdWorker(LPVOID arg)
{
    NdJob *job = (NdJob *)arg;
    char *body = NULL, identity[64], uidText[32], name[128], want[32];
    DWORD len = 0;
    int status = 0;
    BOOL ok = K33HttpGet(job->url, NULL, 2000, 4096, &body, &len, &status) && status == 200;
    _snprintf(want, sizeof(want), "%I64u", job->uid);
    if (ok) {
        const char *uidv = K33JsonGet(body, "uid");
        ok = K33JsonStr(K33JsonGet(body, "identity"), identity, sizeof(identity)) && !strcmp(identity, job->canonical) &&
             uidv && (K33JsonIsNull(uidv) || (K33JsonStr(uidv, uidText, sizeof(uidText)) && !strcmp(uidText, want))) &&
             K33JsonStr(K33JsonGet(body, "name"), name, sizeof(name)) && NdValidName(name, NULL) && strcmp(name, want);
    }
    free(body);
    EnterCriticalSection(&g_ndLock);
    if (job->generation == g_ndGeneration) {
        if (ok) lstrcpynA(g_ndAlias, name, sizeof(g_ndAlias));
        else g_ndAlias[0] = 0;
        g_ndAliasAt = GetTickCount();
    }
    LeaveCriticalSection(&g_ndLock);
    InterlockedExchange(&g_ndBusy, 0);
    free(job);
    return 0;
}

static void NameDisplayTick(void)
{
    static DWORD next, nextRequest;
    DWORD now = GetTickCount();
    if ((LONG)(now - next) < 0) return;
    next = now + 250;
    char canonical[64];
    unsigned long long uid = NdLocalUid();
    BOOL valid = uid && NdCanonical(canonical, sizeof(canonical)) && strlen(canonical) <= 63;
    if (!valid) canonical[0] = 0, uid = 0;
    EnterCriticalSection(&g_ndLock);
    if (uid != g_ndUid || strcmp(canonical, g_ndCanonical)) {
        g_ndUid = uid;
        lstrcpynA(g_ndCanonical, canonical, sizeof(g_ndCanonical));
        g_ndGeneration++;
        g_ndAlias[0] = 0;
        nextRequest = now;
    }
    unsigned generation = g_ndGeneration;
    LeaveCriticalSection(&g_ndLock);
    if (!valid || (LONG)(now - nextRequest) < 0 || InterlockedCompareExchange(&g_ndBusy, 1, 0)) return;
    nextRequest = now + 30000;
    NdJob *job = (NdJob *)calloc(1, sizeof(NdJob));
    if (!job) { InterlockedExchange(&g_ndBusy, 0); return; }
    int n = _snprintf(job->url, sizeof(job->url) - 1, "%s/api/community/display-name?identity=", g_ndUrl);
    for (const char *p = canonical; *p && n < (int)sizeof(job->url) - 4; p++) {
        if (isalnum((unsigned char)*p) || *p == '_' || *p == '-') job->url[n++] = *p;
        else n += _snprintf(job->url + n, 4, "%%%02X", (unsigned char)*p);
    }
    job->url[n] = 0;
    lstrcpynA(job->canonical, canonical, sizeof(job->canonical));
    job->uid = uid;
    job->generation = generation;
    HANDLE t = CreateThread(NULL, 0, NdWorker, job, 0, NULL);
    if (t) CloseHandle(t);
    else { free(job); InterlockedExchange(&g_ndBusy, 0); }
}

/* ---- thunks --------------------------------------------------------------------------------------------------- */

void ND_ConvertName(void);
void ND_HudSource(void);
void ND_ObituarySource(void);
void ND_LocalSource(void);
void ND_MemberSource(void);
void ND_LobbySource(void);
__asm__(
    ".intel_syntax noprefix\n"
    ".text\n"
    /* replaces call 0x5C7340: [esp+4] = text, ESI = output; returns output in EAX, caller pops */
    ".globl _ND_ConvertName\n"
    "_ND_ConvertName:\n"
    "    push ebp\n"
    "    mov ebp, esp\n"
    "    push dword ptr [ebp+8]\n"
    "    push esi\n"
    "    call _NdConvert\n"
    "    add esp, 8\n"
    "    pop ebp\n"
    "    ret\n"
    /* replaces call 0x448E10 at the e/f notifications: ECX = text; everything else passes through */
    ".globl _ND_HudSource\n"
    "_ND_HudSource:\n"
    "    pushfd\n"
    "    pushad\n"
    "    push ecx\n"
    "    call _NdHudMessage\n"
    "    add esp, 4\n"
    "    mov [esp+24], eax\n"           /* the saved ECX */
    "    popad\n"
    "    popfd\n"
    "    jmp dword ptr [_g_ndHudOrig]\n"
    /* replaces call 0x488AF0 (EAX index, ESI 70-byte buffer, no stack arguments) at the obituary */
    ".globl _ND_ObituarySource\n"
    "_ND_ObituarySource:\n"
    "    call dword ptr [_g_ndObitOrig]\n"
    "    pushfd\n"
    "    pushad\n"
    "    test eax, eax\n"
    "    jz 1f\n"
    "    push esi\n"
    "    call _NdObituaryName\n"
    "    add esp, 4\n"
    "1:\n"
    "    popad\n"
    "    popfd\n"
    "    ret\n"
    /* 0x5AA780: mov ecx,[eax+0Ch]; push 3Fh; push ecx (EAX = dw_username's dvar) */
    ".globl _ND_LocalSource\n"
    "_ND_LocalSource:\n"
    "    pushfd\n"
    "    pushad\n"
    "    mov ecx, [eax+0x0C]\n"
    "    push ecx\n"
    "    call _NdSelectAlias\n"
    "    add esp, 4\n"
    "    mov [esp+24], eax\n"
    "    popad\n"
    "    popfd\n"
    "    push 0x3F\n"
    "    push ecx\n"
    "    jmp dword ptr [_g_ndSourceCont]\n"
    /* 0x69598A: mov edx,[ebp-20h]; mov ecx,[ebp-24h] (EDI = entry+8, [ebp-64h] = the name GFx reads) */
    ".globl _ND_MemberSource\n"
    "_ND_MemberSource:\n"
    "    pushfd\n"
    "    pushad\n"
    "    mov eax, [ebp-0x64]\n"
    "    lea edx, [edi-8]\n"
    "    push edx\n"
    "    push eax\n"
    "    call _NdSelectMemberAlias\n"
    "    add esp, 8\n"
    "    mov [ebp-0x64], eax\n"
    "    popad\n"
    "    popfd\n"
    "    mov edx, [ebp-0x20]\n"
    "    mov ecx, [ebp-0x24]\n"
    "    jmp dword ptr [_g_ndMemberCont]\n"
    /* 0x60AD94: mov edx,[2800A80h] (EBX = row name, [ebp-50h] = 70-byte label) */
    ".globl _ND_LobbySource\n"
    "_ND_LobbySource:\n"
    "    pushfd\n"
    "    pushad\n"
    "    lea eax, [ebp-0x50]\n"
    "    push eax\n"
    "    push ebx\n"
    "    call _NdLobbyLabel\n"
    "    add esp, 8\n"
    "    test eax, eax\n"
    "    jz 2f\n"
    "    popad\n"
    "    popfd\n"
    "    mov edx, dword ptr ds:0x02800A80\n"
    "    jmp dword ptr [_g_ndLobbyAlias]\n"
    "2:\n"
    "    popad\n"
    "    popfd\n"
    "    mov edx, dword ptr ds:0x02800A80\n"
    "    jmp dword ptr [_g_ndLobbyCont]\n"
    ".att_syntax prefix\n");

/* ---- install -------------------------------------------------------------------------------------------------- */

typedef struct NdSite { DWORD va; BYTE size; BYTE opcode; void *target; BYTE orig[6]; } NdSite;

static BOOL NdCheck(const NdSite *s)
{
    return Readable((void *)(DWORD_PTR)s->va, s->size) && !memcmp((void *)(DWORD_PTR)s->va, s->orig, s->size);
}

static void NdPatch(const NdSite *s)
{
    BYTE code[6] = { s->opcode, 0, 0, 0, 0, 0x90 };
    LONG rel = (LONG)((DWORD_PTR)s->target - (s->va + 5));
    memcpy(code + 1, &rel, 4);
    WriteCode((void *)(DWORD_PTR)s->va, code, s->size);
}

static BOOL NdBytes(DWORD va, const BYTE *b, unsigned n)
{
    return Readable((void *)(DWORD_PTR)va, n) && !memcmp((void *)(DWORD_PTR)va, b, n);
}

static void InstallNameDisplay(void)
{
    static const BYTE kConvert[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x04, 0x02, 0, 0, 0xA1, 0x50, 0xDE, 0xCB, 0, 0x33, 0xC5 };
    static const BYTE kGfxUtf8[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x49, 0x0C, 0x8B, 0x01, 0x8B, 0x80, 0xB0, 0, 0, 0, 0x5D, 0xFF, 0xE0 };
    static const BYTE kHud[] = { 0x55, 0x8B, 0xEC, 0x6A, 0, 0x50, 0xE8, 0xE5, 0x15, 0x19, 0 };
    static const BYTE kObit[] = { 0x83, 0x3D, 0xE8, 0xC2, 0x6F, 0x01, 0x09, 0x74, 0x58, 0x83, 0x3D, 0xF8, 0xE9, 0xD7, 0x06, 0 };
    static const BYTE kUid1[] = { 0xA1, 0xF8, 0xA9, 0xCE, 0, 0x8B, 0x0D, 0xFC, 0xA9, 0xCE, 0, 0x8B, 0xD0, 0x0B, 0xD1, 0x74, 0x15 };
    static const BYTE kUid2[] = { 0xA0, 0x27, 0x4F, 0xC4, 0x06, 0x84, 0xC0, 0x74, 0x20, 0xA1, 0x58, 0x4F, 0xC4, 0x06 };
    static const BYTE kLobbyRow[] = { 0x8B, 0x43, 0xF8, 0x0B, 0x43, 0xFC, 0x0F, 0x84, 0x71, 0x02, 0, 0, 0x80, 0x3B, 0 };
    NdSite utf8[] = {
        { 0x005AA7CFu, 5, 0xE8, (void *)ND_ConvertName, { 0xE8, 0x6C, 0xCB, 0x01, 0x00 } },
        { 0x005B4E41u, 5, 0xE8, (void *)ND_ConvertName, { 0xE8, 0xFA, 0x24, 0x01, 0x00 } },
        { 0x00469453u, 5, 0xE8, (void *)ND_HudSource, { 0xE8, 0xB8, 0xF9, 0xFD, 0xFF } },
        { 0x00469492u, 5, 0xE8, (void *)ND_HudSource, { 0xE8, 0x79, 0xF9, 0xFD, 0xFF } },
        { 0x0044477Du, 5, 0xE8, (void *)ND_ObituarySource, { 0xE8, 0x6E, 0x43, 0x04, 0x00 } },
        { 0x004447DDu, 5, 0xE8, (void *)ND_ObituarySource, { 0xE8, 0x0E, 0x43, 0x04, 0x00 } },
    };
    NdSite alias[] = {
        { 0x005AA780u, 6, 0xE9, (void *)ND_LocalSource, { 0x8B, 0x48, 0x0C, 0x6A, 0x3F, 0x51 } },
        { 0x0069598Au, 6, 0xE9, (void *)ND_MemberSource, { 0x8B, 0x55, 0xE0, 0x8B, 0x4D, 0xDC } },
        { 0x0060AD94u, 6, 0xE9, (void *)ND_LobbySource, { 0x8B, 0x15, 0x80, 0x0A, 0x80, 0x02 } },
    };
    const char *cl = GetCommandLineA();
    BOOL utf8On = !(cl && (strstr(cl, "codol_name_utf8 0") || strstr(cl, "codol_name_utf8 \"0\"")));
    const char *u = cl ? strstr(cl, "codol_name_display_url ") : NULL;
    if (u) {
        u += 23;
        while (*u == ' ' || *u == '"') u++;
        int n = 0;
        while (u[n] && u[n] != ' ' && u[n] != '"' && n < (int)sizeof(g_ndUrl) - 1) { g_ndUrl[n] = u[n]; n++; }
        g_ndUrl[n] = 0;
        while (n && g_ndUrl[n - 1] == '/') g_ndUrl[--n] = 0;
        if (strncmp(g_ndUrl, "https://", 8) && strncmp(g_ndUrl, "http://127.0.0.1:", 17) && strncmp(g_ndUrl, "http://localhost:", 17)) {
            Log("name display: codol_name_display_url must be https:// (or a loopback http://); service off");
            g_ndUrl[0] = 0;
        }
    }

    BOOL helpers = NdBytes(ND_CONVERT_VA, kConvert, sizeof(kConvert)) && NdBytes(ND_GFX_UTF8_VA, kGfxUtf8, sizeof(kGfxUtf8)) &&
                   NdBytes(ND_HUD_FORMAT_VA, kHud, sizeof(kHud)) && NdBytes(ND_OBIT_NAME_VA, kObit, sizeof(kObit));
    BOOL sitesOk = helpers;
    for (unsigned i = 0; i < sizeof(utf8) / sizeof(utf8[0]); i++) sitesOk = sitesOk && NdCheck(&utf8[i]);
    if (!utf8On) Log("name display: UTF-8 names off (codol_name_utf8 0)");
    else if (!sitesOk) Log("name display: name conversion sites differ; UTF-8 names not installed");
    else {
        for (unsigned i = 0; i < sizeof(utf8) / sizeof(utf8[0]); i++) NdPatch(&utf8[i]);
        Log("name display: UTF-8 player names (local name, scoreboard, Connected, obituary)");
    }

    if (!g_ndUrl[0]) return;
    BOOL aliasOk = helpers && NdBytes(0x00722773u, kUid1, sizeof(kUid1)) && NdBytes(0x00722799u, kUid2, sizeof(kUid2)) &&
                   NdBytes(0x0060AD01u, kLobbyRow, sizeof(kLobbyRow));
    for (unsigned i = 0; i < sizeof(alias) / sizeof(alias[0]); i++) aliasOk = aliasOk && NdCheck(&alias[i]);
    if (!aliasOk) { Log("name display: alias sites differ; display-name service off"); return; }
    InitializeCriticalSection(&g_ndLock);
    g_ndAliasOn = TRUE;
    for (unsigned i = 0; i < sizeof(alias) / sizeof(alias[0]); i++) NdPatch(&alias[i]);
    K33OnMainTick(NameDisplayTick);
    Log("name display: display names from %s (local player only; dw_username and UID unchanged)", g_ndUrl);
}
