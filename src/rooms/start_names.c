/* start_names.c - the owner's match start under another map and mode than the room's own.
 * Included by kernel33.c ahead of offline_lobby.c.
 *
 * START_GAME carries no map: C1's handler (OnChatChannelMessage, MESSAGE_START_GAME) hands the room's own dwRoomInfo
 * names (room+0x15a80 mode, +0x15a88 map) to the start at 0x7120C2. A Survival room names coop_survival_<level> / coop
 * and its match is <level> / survival, so the lobby used to write those names into the room first, with the one
 * message a room's host accepts them in: a JoinResponse (typeId 5, 0x71217E). That message replaces the member roster
 * too, and the lobby's has none (C1 refuses members in it, Com_Error 702), so after the match the room view had no
 * player and no room: "DEV PANEL", "Example text 1/16".
 *
 * Here the names travel beside the room. The lobby stages them (SnStage, lobby thread) and the hook gives them to the
 * one START_GAME that follows; the room keeps its roster and its own names, which is what its view shows afterwards.
 * Not installed, SnStage answers FALSE and the lobby sends the JoinResponse as before. */
#define SN_CALL_VA   0x007120C2u
#define SN_TARGET_VA 0x0857DD6Bu   /* ecx = room, edx = the session record; on the stack: map, mode */

typedef struct SnNames { char map[64], mode[32]; } SnNames;
static SnNames *volatile g_snStaged;
static BOOL g_snInstalled;
static void *g_snTarget __attribute__((used)) = (void *)(DWORD_PTR)SN_TARGET_VA;

/* Any thread. Empty names only drop what is staged. */
static BOOL SnStage(const char *map, const char *mode)
{
    SnNames *n = NULL;
    if (g_snInstalled && map && mode && *map && *mode && strlen(map) < sizeof(n->map) && strlen(mode) < sizeof(n->mode) &&
        (n = (SnNames *)calloc(1, sizeof(*n))) != NULL) {
        strcpy(n->map, map);
        strcpy(n->mode, mode);
    }
    BOOL staged = n != NULL;
    free(InterlockedExchangePointer((PVOID volatile *)&g_snStaged, n));
    return staged;
}

/* Game thread: frame[0] the return address, frame[1] the map, frame[2] the mode. */
static void __attribute__((cdecl, noinline, used)) SnStart(const char **frame)
{
    static SnNames names;   /* what C1 was given stays readable */
    SnNames *n = (SnNames *)InterlockedExchangePointer((PVOID volatile *)&g_snStaged, NULL);
    if (!n) return;
    names = *n;
    free(n);
    Log("room start: the room stays %s / %s; this match loads %s / %s", frame[1] ? frame[1] : "", frame[2] ? frame[2] : "",
        names.map, names.mode);
    frame[1] = names.map;
    frame[2] = names.mode;
}

static void __attribute__((naked, used)) SnStartHook(void)
{
    __asm__ __volatile__("pushfl\n\tpushal\n\tleal 36(%esp),%eax\n\tpushl %eax\n\tcall _SnStart\n\taddl $4,%esp\n\t"
                         "popal\n\tpopfl\n\tjmp *_g_snTarget\n");
}

static void PatchRoomStartNames(void)
{
    /* push eax ; lea ecx, [ebx+15A88h] ; call 0xA2C870 ; push eax ; lea edx, [ebp-388h] ; mov ecx, ebx ; call 0x857DD6B */
    static const BYTE kSite[] = { 0x50, 0x8D, 0x8B, 0x88, 0x5A, 0x01, 0x00, 0xE8, 0xB7, 0xA7, 0x31, 0x00, 0x50,
                                  0x8D, 0x95, 0x78, 0xFC, 0xFF, 0xFF, 0x8B, 0xCB, 0xE8, 0xA4, 0xBC, 0xE6, 0x07 };
    const BYTE *site = (const BYTE *)(DWORD_PTR)(SN_CALL_VA - (sizeof(kSite) - 5));
    if (!Readable(site, sizeof(kSite)) || memcmp(site, kSite, sizeof(kSite))) {
        Log("room start names: the bytes at 0x%08X differ; unchanged", SN_CALL_VA);
        return;
    }
    LONG rel = (LONG)((DWORD_PTR)SnStartHook - (SN_CALL_VA + 5));
    WriteCode((void *)(DWORD_PTR)(SN_CALL_VA + 1), &rel, sizeof(rel));
    g_snInstalled = TRUE;
    Log("room start names: the owner's room keeps its roster and names through a start (0x%08X)", SN_CALL_VA);
}
