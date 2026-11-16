/*
 * name_display.c - player names in UTF-8, ported from codol-classic's d3d9.dll (NameDisplay.cpp). Included by
 * kernel33.c after theatre.c.
 *
 * C1 converts every UI string to a Scaleform string through 0x5C7340 (output ESI, one caller-popped stack argument,
 * returns ESI in EAX), which reads it as code page 936 (GBK). A player name that arrives as UTF-8 (a dw_username
 * typed in UTF-8, or a lobby name) therefore shows as mojibake. Kernel33's GBK input (kernel33.c,
 * PatchGbkInput) is unchanged: names typed in the game are GBK and stay on the original path.
 *
 * UTF-8 names (on by default; "+set k33_name_utf8 0" on the command line installs none of them):
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
 * The optional display-name service of the online client (codol_name_display_url: an alias fetched over HTTPS and
 * shown in place of the local player's name) was removed: this client is offline only.
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

static char g_ndHudCopy[1024];

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

/* ---- the local player ------------------------------------------------------------------------------------- */

/* The local player's UID ([0xCEA9F8], else [0x6C44F58] once [0x6C44F27] is set); p2p.c's owner (P2PLocalUid). */
static unsigned long long NdLocalUid(void)
{
    unsigned long long uid = *(volatile const unsigned long long *)(DWORD_PTR)ND_UID_VA;
    if (uid) return uid;
    if (!*(volatile const BYTE *)(DWORD_PTR)ND_UID2_READY_VA) return 0;
    return *(volatile const unsigned long long *)(DWORD_PTR)ND_UID2_VA;
}

/* ---- thunks --------------------------------------------------------------------------------------------------- */

void ND_ConvertName(void);
void ND_HudSource(void);
void ND_ObituarySource(void);
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
    NdSite utf8[] = {
        { 0x005AA7CFu, 5, 0xE8, (void *)ND_ConvertName, { 0xE8, 0x6C, 0xCB, 0x01, 0x00 } },
        { 0x005B4E41u, 5, 0xE8, (void *)ND_ConvertName, { 0xE8, 0xFA, 0x24, 0x01, 0x00 } },
        { 0x00469453u, 5, 0xE8, (void *)ND_HudSource, { 0xE8, 0xB8, 0xF9, 0xFD, 0xFF } },
        { 0x00469492u, 5, 0xE8, (void *)ND_HudSource, { 0xE8, 0x79, 0xF9, 0xFD, 0xFF } },
        { 0x0044477Du, 5, 0xE8, (void *)ND_ObituarySource, { 0xE8, 0x6E, 0x43, 0x04, 0x00 } },
        { 0x004447DDu, 5, 0xE8, (void *)ND_ObituarySource, { 0xE8, 0x0E, 0x43, 0x04, 0x00 } },
    };
    const char *cl = GetCommandLineA();
    BOOL utf8On = !(cl && (strstr(cl, "k33_name_utf8 0") || strstr(cl, "k33_name_utf8 \"0\"")));

    BOOL helpers = NdBytes(ND_CONVERT_VA, kConvert, sizeof(kConvert)) && NdBytes(ND_GFX_UTF8_VA, kGfxUtf8, sizeof(kGfxUtf8)) &&
                   NdBytes(ND_HUD_FORMAT_VA, kHud, sizeof(kHud)) && NdBytes(ND_OBIT_NAME_VA, kObit, sizeof(kObit));
    BOOL sitesOk = helpers;
    for (unsigned i = 0; i < sizeof(utf8) / sizeof(utf8[0]); i++) sitesOk = sitesOk && NdCheck(&utf8[i]);
    if (!utf8On) Log("name display: UTF-8 names off (k33_name_utf8 0)");
    else if (!sitesOk) Log("name display: name conversion sites differ; UTF-8 names not installed");
    else {
        for (unsigned i = 0; i < sizeof(utf8) / sizeof(utf8[0]); i++) NdPatch(&utf8[i]);
        Log("name display: UTF-8 player names (local name, scoreboard, Connected, obituary)");
    }
}
