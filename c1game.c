/*
 * c1game.c - the few C1 engine calls that Kernel33 features share (included by kernel33.c before
 * console.c, voice.c and controller.c, after parity.c). Nothing here needs the developer console: a
 * build without console.c (-DK33_NO_CONSOLE) uses exactly this file.
 *
 *   print      Com_Printf 0x5941B0 (channel, fmt, ...): C1's own console and console_mp.log.
 *   execute    Cbuf_AddText 0x58BA60 (ecx = localClient, eax = text), as CL_KeyEvent's binds do.
 *   commands   the cmd_function_t list 0x24F6424 {next, name, function}; Cmd_FindCommand 0x58C790.
 *              C1's Cmd_AddCommand is inlined at every call site (e.g. 0x498BA9: find, then prepend a
 *              static node), so C1AddCommand does the same.
 *   dvars      Dvar_RegisterInt 0x624420 (name, default, min, max, flags, description); the dvar count
 *              0x2AB98C4 says when registration is safe (C1 has run its own registrations).
 *   keys       keyCatchers 0x16FC2E0 (menus, chat line), the word CL_CmdButtons 0x4905C5 tests.
 *
 * Every entry is byte-checked once in C1GameInit; a mismatch is logged and that helper stays off.
 */

#define C1_COM_PRINTF_VA   0x005941B0u
#define C1_CBUF_VA         0x0058BA60u
#define C1_CMDFIND_VA      0x0058C790u
#define C1_CMD_LIST_VA     0x024F6424u
#define C1_DVAR_COUNT_VA   0x02AB98C4u
#define C1_REGISTER_INT_VA 0x00624420u
#define C1_KEYCATCHERS_VA  0x016FC2E0u
#define C1_PRINT_CHANNEL   0x10        /* C1's own client prints (CL_ConnectionlessPacket) */

typedef struct C1Cmd { struct C1Cmd *next; const char *name; void *fn; } C1Cmd;

static BOOL g_c1PrintOk, g_c1CbufOk, g_c1CmdsOk, g_c1DvarOk;

static void C1GameInit(void)
{
    static BOOL done;
    static const BYTE kComPrintf[] = { 0x55, 0x8B, 0xEC, 0xB8, 0x28, 0x20, 0x00, 0x00 };
    static const BYTE kCbuf[] = { 0x53, 0x56, 0x57, 0x68, 0xE0, 0x2D, 0xB6, 0x02, 0x8B, 0xF0, 0x8B, 0xF9 };
    static const BYTE kCmdFind[] = { 0xA1, 0x24, 0x64, 0x4F, 0x02, 0x53, 0x85, 0xC0 };
    if (done) return;
    done = TRUE;
    g_c1PrintOk = Readable((void *)(DWORD_PTR)C1_COM_PRINTF_VA, sizeof(kComPrintf)) &&
                  !memcmp((void *)(DWORD_PTR)C1_COM_PRINTF_VA, kComPrintf, sizeof(kComPrintf));
    g_c1CbufOk = Readable((void *)(DWORD_PTR)C1_CBUF_VA, sizeof(kCbuf)) &&
                 !memcmp((void *)(DWORD_PTR)C1_CBUF_VA, kCbuf, sizeof(kCbuf));
    g_c1CmdsOk = Readable((void *)(DWORD_PTR)C1_CMDFIND_VA, sizeof(kCmdFind)) &&
                 !memcmp((void *)(DWORD_PTR)C1_CMDFIND_VA, kCmdFind, sizeof(kCmdFind));
    g_c1DvarOk = Readable((void *)(DWORD_PTR)C1_REGISTER_INT_VA, 1) && Readable((void *)(DWORD_PTR)C1_DVAR_COUNT_VA, 4);
    Log("c1game: Com_Printf %s, Cbuf_AddText %s, command list %s, Dvar_RegisterInt %s", g_c1PrintOk ? "ok" : "bytes differ",
        g_c1CbufOk ? "ok" : "bytes differ", g_c1CmdsOk ? "ok" : "bytes differ", g_c1DvarOk ? "ok" : "missing");
}

/* Main thread. Goes to C1's console and console_mp.log (and the Kernel33 console when it is built). */
static void C1Print(int channel, const char *text)
{
    if (g_c1PrintOk)
        ((void(__cdecl *)(int, const char *, ...))(DWORD_PTR)C1_COM_PRINTF_VA)(channel, "%s", text);
    else
        Log("%s", text);
}

static void C1Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void C1Printf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    C1Print(C1_PRINT_CHANNEL, buf);
}

/* Main thread. */
static BOOL C1CbufAddText(const char *text)
{
    if (!g_c1CbufOk) return FALSE;
    int lc = 0;
    DWORD fn = C1_CBUF_VA;
    __asm__ volatile("call *%[fn]" : "+a"(text), "+c"(lc) : [fn] "r"(fn) : "edx", "memory", "cc");
    return TRUE;
}

static C1Cmd *C1CommandList(void) { return g_c1CmdsOk ? *(C1Cmd *volatile *)(DWORD_PTR)C1_CMD_LIST_VA : NULL; }

/* Cmd_AddCommand: a static node prepended to the list unless the name exists. */
static BOOL C1AddCommand(C1Cmd *node)
{
    if (!g_c1CmdsOk || !node || !node->name) return FALSE;
    for (C1Cmd *c = C1CommandList(); c && Readable(c, sizeof(*c)); c = c->next)
        if (c->name && Readable(c->name, 1) && !_stricmp(c->name, node->name)) return FALSE;
    C1Cmd **head = (C1Cmd **)(DWORD_PTR)C1_CMD_LIST_VA;
    node->next = *head;
    *head = node;
    return TRUE;
}

static BOOL C1DvarsReady(void) { return g_c1DvarOk && *(volatile int *)(DWORD_PTR)C1_DVAR_COUNT_VA > 100; }

/* Dvar_RegisterInt; NULL when unavailable. The value is the int at dvar+0xC. */
static const BYTE *C1RegisterInt(const char *name, int value, int min, int max, unsigned flags, const char *desc)
{
    typedef const BYTE *(__cdecl *RegisterIntFn)(const char *, int, int, int, unsigned, const char *);
    if (!g_c1DvarOk) return NULL;
    const BYTE *d = ((RegisterIntFn)(DWORD_PTR)C1_REGISTER_INT_VA)(name, value, min, max, flags, desc);
    return d && Readable(d, 0x10) ? d : NULL;
}

__attribute__((unused)) static int C1DvarInt(const BYTE *d, int def) { return d ? *(volatile const int *)(d + 0xC) : def; }

__attribute__((unused)) static int C1KeyCatchers(void) { return *(volatile int *)(DWORD_PTR)C1_KEYCATCHERS_VA; }

/* ---- dvar strings and queued "set" (theatre.c, quick_room.c, name_display.c) ------------------------------------
 *   find       Dvar_FindVar 0x6232C0 (name) -> dvar_t*: name +0, type byte +8 (0 bool, 1 float, 5 int, 7 string),
 *              current value +0xC (a string dvar's is a char*).
 *   cbuf room  the local client's command buffer {data 0x25094DC, maxsize +4, cursize +8}, the one
 *              Cbuf_AddText fills. A line that does not fit is dropped by C1 with a print, so C1QueueSet checks first.
 * A "set" goes through the command buffer, so the dvar changes in C1's own frame (Cbuf_Execute), never inside a hook,
 * and a dvar that does not exist yet is created as a string dvar, which is what the Scaleform movies read. */
#define C1_DVAR_FIND_VA    0x006232C0u
#define C1_CBUF_DATA_VA    0x025094DCu

static const BYTE *C1DvarFind(const char *name)
{
    static const BYTE kFind[] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x56, 0x57, 0x68, 0xD0, 0x98, 0xAB, 0x02 };
    static int ok = -1;
    if (ok < 0) ok = Readable((void *)(DWORD_PTR)C1_DVAR_FIND_VA, sizeof(kFind)) &&
                     !memcmp((void *)(DWORD_PTR)C1_DVAR_FIND_VA, kFind, sizeof(kFind));
    if (!ok || !C1DvarsReady()) return NULL;
    const BYTE *d = ((const BYTE *(__cdecl *)(const char *))(DWORD_PTR)C1_DVAR_FIND_VA)(name);
    return d && Readable(d, 0x10) ? d : NULL;
}

/* A string dvar's value copied into out ("" when it is missing or not a string). Main thread. */
__attribute__((unused)) static BOOL C1DvarString(const char *name, char *out, int size)
{
    out[0] = 0;
    const BYTE *d = C1DvarFind(name);
    if (!d || d[8] != 7) return FALSE;
    const char *s = *(const char *const *)(d + 0xC);
    if (!s || !Readable(s, 1)) return FALSE;
    int i = 0;
    for (; i < size - 1 && Readable(s + i, 1) && s[i]; i++) out[i] = s[i];
    out[i] = 0;
    return TRUE;
}

/* Free bytes in the local client's command buffer (0 when it is not set up). */
static int C1CbufFree(void)
{
    const DWORD *cb = (const DWORD *)(DWORD_PTR)C1_CBUF_DATA_VA;
    if (!g_c1CbufOk || !Readable(cb, 12) || !cb[0] || cb[1] < 1024 || cb[1] > 0x100000 || cb[2] > cb[1]) return 0;
    return (int)(cb[1] - cb[2]);
}

/* Queues set <name> "<value>". The value must not hold a quote or a line break (they are dropped). Main thread. */
__attribute__((unused)) static BOOL C1QueueSet(const char *name, const char *value)
{
    char line[1100];
    int n = _snprintf(line, sizeof(line) - 2, "set %s \"", name);
    if (n < 0) return FALSE;
    for (const char *p = value; p && *p && n < (int)sizeof(line) - 3; p++)
        if (*p != '"' && *p != '\r' && *p != '\n' && *p != ';') line[n++] = *p;
    line[n++] = '"';
    line[n++] = '\n';
    line[n] = 0;
    if (C1CbufFree() <= n + 1) return FALSE;
    return C1CbufAddText(line);
}
