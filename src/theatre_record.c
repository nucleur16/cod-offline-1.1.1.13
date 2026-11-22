/*
 * theatre_record.c - every match the player plays is recorded as a C1 film, into players\<profile>\demos\, the
 * folder the THEATRE screen (theatre.c) lists and plays. Included by theatre.c.
 *
 * Why the client records. C1 has no recorder, only BO1's film player (cl_demo_play, virtualized at 0x8578541; no
 * code in C1 writes a film header, and CL_Init never adds "record"): the online films came from the servers
 * (codol-classic's from its own film service). Offline the match host is the sh1x server inside this DLL, which has no C1 film writer,
 * so without this file nothing was ever recorded and THEATRE stayed empty. The client already receives everything a
 * film holds, so it writes the film itself, in the format C1's reader takes:
 *
 *   header   0x8020 bytes, read whole into clc+0x80268 (0x733432):
 *              +0 version (1..3; >= 2 adds keyframe extras, 0x72FF59), +4 the recorded client's number (set into the
 *              int dvar 919, -1..19, at 0x72AE1F), +8 the film protocol, 187 (measured: the virtualized player
 *              refuses any other value with UI message "1064 187 <value>"), +0xC clip flag (byte; 0 = a whole film),
 *              +0x1C the kind of build that recorded it, 5 = "Ship" (measured: any other gives "203 <kind> Ship",
 *              the names are the table at 0xCCC664: Debug, Test, FastServer, Demo, Screenshot, Ship). The rest is
 *              zero.
 *   stream   records, each a type byte (CL_Demo_ReadMessage 0x72CA00, jump table 0x72CAEC):
 *              2  server message: int serverMessageSequence, int length (<= 0x20000), the message bytes from the
 *                 reliable acknowledge on, already netchan-decoded (0x72C620 then runs MSG_ReadLong and
 *                 CL_ParseServerMessage 0x834B55B on it, exactly what CL_PacketEvent 0x497553 does live after
 *                 Netchan_Process 0x5A27D0, the two MSG_ReadLongs and CL_Netchan_Decode 0x49C5E0).
 *              1  client archive entry: int index, then the 64-byte cl.clientArchive[index] fields as 0x72C8D0 reads
 *                 them (CL_ArchiveClientState 0x49C6B0 fills the ring at cl+0x5CBC, next index at cl+0x9CBC).
 *              (3 is a server message with one more int, clc+0x40150, and 4 a clip command; neither is written.)
 *              0  end of film.
 *   metadata 0x280 bytes (0x72CC90, the list at 0x72BA80): +0 version, +4/+8 first/last snapshot server time (ms),
 *            +0xC gametype[256], +0x10C map[256], +0x20C the player's name[64], +0x27C the start as time_t.
 *            Then two empty tables (int 0, int 0) and the footer {int metadata size 0x288, int version}.
 *
 * What is recorded: from the first server message of a connection (the gamestate) until the client disconnects or
 * the server sends a new gamestate (a map change: the next film starts with it). The live hook copies each message
 * before C1 parses it and writes it after, when the connection state shows whether it was a new level. A film shorter
 * than 10 s of play is dropped. A film is written as <name>.demo.part and renamed to <name>.demo when it is complete,
 * so THEATRE (and another instance) never sees a half-written film; a .part left by a crash is removed at the next
 * start when no running instance holds it open.
 *
 * Growth: the newest TR_KEEP (50) films of the profile are kept, the oldest deleted. 50 is C1's own limit: its local
 * film table (0x6C26AE0..0x6C2FDC0, 0x2F0 bytes a film) and the sort at 0x72C033 take at most 50 files. A film stops
 * growing at TR_MAX_BYTES, and none is started with less than TR_MIN_FREE free on the disk.
 *
 * Several instances: each profile has its own folder (players\<dw_username>\demos), names carry the start second and
 * a counter (CREATE_NEW never reuses a file), the .part is held open without FILE_SHARE_DELETE, and only finished
 * .demo files are ever trimmed.
 */

#define TR_PARSE_CALL_VA     0x00497553u   /* CL_PacketEvent: call CL_ParseServerMessage (edi = msg, push 0) */
#define TR_PARSE_VA          0x0834B55Bu
#define TR_CL_PTR_VA         0x016FC318u
#define TR_CL_KEY            0x9056BBC8u
#define TR_CL_SNAP_TIME      0x3378        /* cl.snap.serverTime */
#define TR_CL_ARCHIVE        0x5CBC        /* cl.clientArchive[256], 0x40 bytes each */
#define TR_CL_ARCHIVE_NEXT   0x9CBC
#define TR_CLC_SEQUENCE      0x40148       /* clc.serverMessageSequence */
#define TR_CLC_DEMOPLAYING   0x80260
#define TR_HEADER_SIZE       0x8020
#define TR_META_SIZE         0x280
#define TR_VERSION           3
#define TR_PROTOCOL          187           /* header +8: what cl_demo_play wants (it shows "1064 187 <found>" otherwise) */
#define TR_BUILD_SHIP        5             /* header +0x1C: the build kind, {Debug, Test, FastServer, Demo, Screenshot, Ship} (0xCCC664) */
#define TR_MSG_MAX           0x20000
#define TR_BUF_SIZE          (512 * 1024)
#define TR_KEEP              50
#define TR_MIN_PLAY_MS       10000
#define TR_MAX_BYTES         (1536u * 1024 * 1024)
#define TR_MIN_FREE          (256ull * 1024 * 1024)

/* players\<profile>\demos\ with a trailing backslash (offline_lobby.c); 0 when there is no players folder. */
static int OflDemoDir(WCHAR *dir, BOOL create);

static struct {
    BOOL installed, active;
    HANDLE file;
    WCHAR part[MAX_PATH], dir[MAX_PATH], stem[96];
    BYTE *out;
    DWORD outLen, bytes, flushedAt;
    BYTE *msg;                   /* the message being parsed, copied before CL_ParseServerMessage */
    DWORD msgLen, stateBefore;
    int msgSeq;
    BOOL msgPending;
    int archiveNext, firstTime, lastTime, clientNum;
    unsigned messages;
    char map[64], mode[32], name[64];
    __time64_t started;
} g_tr;

DWORD g_trParse = TR_PARSE_VA;

static DWORD TrState(void) { return *(volatile DWORD *)(DWORD_PTR)VOICE_CONNSTATE_VA; }

static BYTE *TrClc(void)
{
    DWORD p = *(volatile DWORD *)(DWORD_PTR)TH_CLC_PTR_VA;
    if (!p || p == TH_CLC_KEY) return NULL;
    BYTE *clc = (BYTE *)(DWORD_PTR)(p ^ TH_CLC_KEY);
    return Readable(clc + TR_CLC_DEMOPLAYING, 4) && Readable(clc + TR_CLC_SEQUENCE, 4) ? clc : NULL;
}

static BYTE *TrCl(void)
{
    DWORD p = *(volatile DWORD *)(DWORD_PTR)TR_CL_PTR_VA;
    if (!p || p == TR_CL_KEY) return NULL;
    BYTE *cl = (BYTE *)(DWORD_PTR)(p ^ TR_CL_KEY);
    return Readable(cl + TR_CL_ARCHIVE, 0x4004) ? cl : NULL;
}

static void TrFlush(void)
{
    if (g_tr.file == INVALID_HANDLE_VALUE || !g_tr.outLen) return;
    DWORD w = 0;
    if (!WriteFile(g_tr.file, g_tr.out, g_tr.outLen, &w, NULL) || w != g_tr.outLen)
        Log("theatre: writing the film failed (error %lu)", GetLastError());
    g_tr.outLen = 0;
    g_tr.flushedAt = GetTickCount();
}

static void TrPut(const void *data, DWORD n)
{
    const BYTE *p = (const BYTE *)data;
    while (n) {
        if (g_tr.outLen == TR_BUF_SIZE) TrFlush();
        DWORD room = TR_BUF_SIZE - g_tr.outLen, take = n < room ? n : room;
        memcpy(g_tr.out + g_tr.outLen, p, take);
        g_tr.outLen += take;
        g_tr.bytes += take;
        p += take;
        n -= take;
    }
}

static void TrPutInt(int v) { TrPut(&v, 4); }

static void TrHeader(BYTE *h)
{
    memset(h, 0, TR_HEADER_SIZE);
    *(int *)(h + 0) = TR_VERSION;
    *(int *)(h + 4) = g_tr.clientNum >= 0 && g_tr.clientNum <= 19 ? g_tr.clientNum : -1;
    *(int *)(h + 8) = TR_PROTOCOL;
    *(int *)(h + 0x1C) = TR_BUILD_SHIP;
}

/* Files of the folder matching `pattern` (L"*.demo"), oldest first; returns the count (at most `max`). */
typedef struct TrFile { WCHAR name[MAX_PATH]; FILETIME written; } TrFile;

static int TrCompareFiles(const void *a, const void *b)
{
    return CompareFileTime(&((const TrFile *)a)->written, &((const TrFile *)b)->written);
}

static int TrList(const WCHAR *dir, const WCHAR *pattern, TrFile *files, int max)
{
    WCHAR path[MAX_PATH];
    if (lstrlenW(dir) + lstrlenW(pattern) >= MAX_PATH) return 0;
    lstrcpyW(path, dir);
    lstrcatW(path, pattern);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(path, &fd);
    int n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (n >= max || lstrlenW(dir) + lstrlenW(fd.cFileName) >= MAX_PATH) continue;
        lstrcpyW(files[n].name, fd.cFileName);
        files[n].written = fd.ftLastWriteTime;
        n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    qsort(files, n, sizeof(files[0]), TrCompareFiles);
    return n;
}

/* Keep the newest TR_KEEP films; remove stale .part files nobody holds open. */
static void TrTrim(const WCHAR *dir, BOOL parts)
{
    static TrFile files[1024];
    WCHAR path[MAX_PATH];
    int n = TrList(dir, L"*.demo", files, 1024);
    for (int i = 0; i + TR_KEEP < n; i++) {
        lstrcpyW(path, dir);
        lstrcatW(path, files[i].name);
        if (DeleteFileW(path)) Log("theatre: removed the old film %ls (keeping the newest %d)", files[i].name, TR_KEEP);
    }
    if (!parts) return;
    n = TrList(dir, L"*.demo.part", files, 1024);
    for (int i = 0; i < n; i++) {
        lstrcpyW(path, dir);
        lstrcatW(path, files[i].name);
        if (DeleteFileW(path)) Log("theatre: removed the unfinished film %ls (left by a crash)", files[i].name);
    }
}

static void TrStart(void)
{
    if (!OflDemoDir(g_tr.dir, TRUE)) return;
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExW(g_tr.dir, &freeBytes, NULL, NULL) && freeBytes.QuadPart < TR_MIN_FREE) {
        Log("theatre: only %I64u MB free on the disk; this match is not recorded", freeBytes.QuadPart >> 20);
        return;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(g_tr.stem, sizeof(g_tr.stem) / sizeof(WCHAR) - 1, L"%04u-%02u-%02u_%02u-%02u-%02u", st.wYear, st.wMonth,
               st.wDay, st.wHour, st.wMinute, st.wSecond);
    g_tr.stem[sizeof(g_tr.stem) / sizeof(WCHAR) - 1] = 0;
    g_tr.file = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 100 && g_tr.file == INVALID_HANDLE_VALUE; i++) {
        _snwprintf(g_tr.part, MAX_PATH - 1, i ? L"%ls%ls_%d.demo.part" : L"%ls%ls.demo.part", g_tr.dir, g_tr.stem, i);
        g_tr.part[MAX_PATH - 1] = 0;
        g_tr.file = CreateFileW(g_tr.part, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (g_tr.file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) break;
    }
    if (g_tr.file == INVALID_HANDLE_VALUE) {
        Log("theatre: cannot create a film in %ls (error %lu); this match is not recorded", g_tr.dir, GetLastError());
        return;
    }
    g_tr.active = TRUE;
    g_tr.outLen = g_tr.bytes = g_tr.messages = 0;
    g_tr.firstTime = g_tr.lastTime = 0;
    g_tr.clientNum = -1;
    g_tr.map[0] = g_tr.mode[0] = g_tr.name[0] = 0;
    g_tr.started = _time64(NULL);
    g_tr.flushedAt = GetTickCount();
    g_tr.archiveNext = -1;   /* the gamestate clears cl: the ring is taken from where it is after it */
    static BYTE header[TR_HEADER_SIZE];
    TrHeader(header);
    TrPut(header, TR_HEADER_SIZE);
    Log("theatre: recording to %ls", g_tr.part);
}

/* The names of the level, once the gamestate has set them (main thread). */
static void TrLearn(void)
{
    const char *info = DiscordServerInfo();   /* configstring 0, the serverinfo (discord.c) */
    if (!g_tr.map[0] && info) {
        char v[64];
        DiscordInfoValue(info, "mapname", v, sizeof(v));
        if (v[0] && ThSafe(v)) lstrcpynA(g_tr.map, v, sizeof(g_tr.map));
    }
    if (!g_tr.mode[0]) {
        char v[32] = "";
        if (info) DiscordInfoValue(info, "g_gametype", v, sizeof(v));
        if (!v[0]) C1DvarString("g_gametype", v, sizeof(v));
        if (v[0] && ThSafe(v)) lstrcpynA(g_tr.mode, v, sizeof(g_tr.mode));
    }
    if (!g_tr.name[0]) {
        char v[64];
        if ((C1DvarString("name", v, sizeof(v)) && v[0]) || (C1DvarString("dw_username", v, sizeof(v)) && v[0]))
            lstrcpynA(g_tr.name, v, sizeof(g_tr.name));
    }
    if (g_tr.clientNum < 0 && TrState() == VOICE_CA_ACTIVE) g_tr.clientNum = VoiceLocalClient();
}

static void TrAbandon(const char *why)
{
    if (g_tr.file != INVALID_HANDLE_VALUE) CloseHandle(g_tr.file);
    g_tr.file = INVALID_HANDLE_VALUE;
    g_tr.active = FALSE;
    g_tr.outLen = 0;
    DeleteFileW(g_tr.part);
    Log("theatre: film %ls dropped (%s)", g_tr.part, why);
}

static void TrFinish(const char *why)
{
    if (!g_tr.active) return;
    TrLearn();
    int played = g_tr.lastTime - g_tr.firstTime;
    if (!g_tr.messages || !g_tr.firstTime || played < TR_MIN_PLAY_MS) {
        char note[96];
        _snprintf(note, sizeof(note), "%s after %d ms of play", why, played);
        note[sizeof(note) - 1] = 0;
        TrAbandon(note);
        return;
    }
    BYTE end = 0;
    TrPut(&end, 1);
    static BYTE meta[TR_META_SIZE];
    memset(meta, 0, sizeof(meta));
    *(int *)(meta + 0) = TR_VERSION;
    *(int *)(meta + 4) = g_tr.firstTime;
    *(int *)(meta + 8) = g_tr.lastTime;
    lstrcpynA((char *)meta + 0xC, g_tr.mode, 256);
    lstrcpynA((char *)meta + 0x10C, g_tr.map, 256);
    lstrcpynA((char *)meta + 0x20C, g_tr.name, 64);
    *(int *)(meta + 0x27C) = (int)g_tr.started;
    TrPut(meta, sizeof(meta));
    TrPutInt(0);   /* table at clc+0x888C8 (<= 40 x 24 bytes) */
    TrPutInt(0);   /* table at clc+0x89230 (<= 300 x 8 bytes) */
    TrPutInt(TR_META_SIZE + 8);
    TrPutInt(TR_VERSION);
    TrFlush();
    if (g_tr.clientNum >= 0) {   /* the header again, now that the player's number is known */
        static BYTE header[TR_HEADER_SIZE];
        TrHeader(header);
        DWORD w;
        if (SetFilePointer(g_tr.file, 0, NULL, FILE_BEGIN) == 0) WriteFile(g_tr.file, header, TR_HEADER_SIZE, &w, NULL);
    }
    CloseHandle(g_tr.file);
    g_tr.file = INVALID_HANDLE_VALUE;
    g_tr.active = FALSE;

    WCHAR final[MAX_PATH], wmap[64];
    MultiByteToWideChar(CP_ACP, 0, g_tr.map[0] ? g_tr.map : "unknown", -1, wmap, 64);
    BOOL moved = FALSE;
    for (int i = 0; i < 100 && !moved; i++) {
        _snwprintf(final, MAX_PATH - 1, i ? L"%ls%ls_%ls_%d.demo" : L"%ls%ls_%ls.demo", g_tr.dir, g_tr.stem, wmap, i);
        final[MAX_PATH - 1] = 0;
        moved = MoveFileExW(g_tr.part, final, MOVEFILE_WRITE_THROUGH);
        if (!moved && GetLastError() != ERROR_ALREADY_EXISTS && GetLastError() != ERROR_FILE_EXISTS) break;
    }
    if (!moved) {
        Log("theatre: could not name the film %ls (error %lu)", g_tr.part, GetLastError());
        return;
    }
    Log("theatre: film %ls saved (%s %s, %d s, %u messages, %lu bytes; %s)", final, g_tr.map, g_tr.mode, played / 1000,
        g_tr.messages, (unsigned long)g_tr.bytes, why);
    TrTrim(g_tr.dir, FALSE);
    g_thRevision++;   /* a THEATRE page open now lists it at the next refresh */
}

/* New entries of cl.clientArchive since the last record (0x72C8D0's layout). */
static void TrArchive(void)
{
    BYTE *cl = TrCl();
    if (!cl) return;
    int next = (int)(*(volatile DWORD *)(cl + TR_CL_ARCHIVE_NEXT) & 0xFF);
    if (g_tr.archiveNext < 0) { g_tr.archiveNext = next; return; }
    for (int guard = 0; g_tr.archiveNext != next && guard < 256; guard++) {
        const BYTE *e = cl + TR_CL_ARCHIVE + g_tr.archiveNext * 0x40;
        BYTE type = 1;
        TrPut(&type, 1);
        TrPutInt(g_tr.archiveNext);
        TrPut(e + 0x04, 12);
        TrPut(e + 0x10, 12);
        TrPut(e + 0x20, 4);
        TrPut(e + 0x1C, 4);
        TrPut(e + 0x00, 4);
        TrPut(e + 0x24, 12);
        TrPut(e + 0x30, 4);
        if (*(const int *)(e + 0x30)) { TrPut(e + 0x34, 8); TrPut(e + 0x3C, 4); }
        g_tr.archiveNext = (g_tr.archiveNext + 1) & 0xFF;
    }
}

/* ---- the hook on CL_PacketEvent's CL_ParseServerMessage call (main thread) ---- */

__attribute__((used)) void __cdecl TrBeforeParse(const BYTE *msg)
{
    g_tr.msgPending = FALSE;
    BYTE *clc = TrClc();
    if (!g_tr.installed || !clc || *(volatile DWORD *)(clc + TR_CLC_DEMOPLAYING) || !Readable(msg, 0x20)) return;
    const BYTE *data = *(const BYTE *const *)(msg + 8);
    int size = *(const int *)(msg + 0x14);
    if (size < 8 || size - 4 > TR_MSG_MAX || !data || !Readable(data, (SIZE_T)size)) return;
    memcpy(g_tr.msg, data + 4, (size_t)size - 4);
    g_tr.msgLen = (DWORD)size - 4;
    g_tr.msgSeq = *(volatile int *)(clc + TR_CLC_SEQUENCE);
    g_tr.stateBefore = TrState();
    g_tr.msgPending = TRUE;
}

__attribute__((used)) void __cdecl TrAfterParse(void)
{
    if (!g_tr.msgPending) return;
    g_tr.msgPending = FALSE;
    DWORD after = TrState();
    if (g_tr.active && g_tr.stateBefore == VOICE_CA_ACTIVE && after < VOICE_CA_ACTIVE) TrFinish("new level");
    if (!g_tr.active) {
        if (g_tr.stateBefore >= VOICE_CA_ACTIVE || after == 0) return;   /* only from a gamestate on */
        TrStart();
        if (!g_tr.active) return;
    }
    TrArchive();
    BYTE type = 2;
    TrPut(&type, 1);
    TrPutInt(g_tr.msgSeq);
    TrPutInt((int)g_tr.msgLen);
    TrPut(g_tr.msg, g_tr.msgLen);
    g_tr.messages++;
    BYTE *cl = TrCl();
    int t = cl && after == VOICE_CA_ACTIVE ? *(volatile int *)(cl + TR_CL_SNAP_TIME) : 0;
    if (t > 0) {
        if (!g_tr.firstTime) g_tr.firstTime = t;
        if (t > g_tr.lastTime) g_tr.lastTime = t;
    }
    if (after == VOICE_CA_ACTIVE && (!g_tr.map[0] || !g_tr.mode[0] || g_tr.clientNum < 0)) TrLearn();
    if (g_tr.bytes > TR_MAX_BYTES) TrFinish("size limit");
}

void TR_ParseThunk(void);
__asm__(
    ".text\n"
    ".globl _TR_ParseThunk\n"
    "_TR_ParseThunk:\n"              /* replaces the call at 0x497553; edi = msg, [esp+4] = CL_ParseServerMessage's 0 */
    "    pushal\n"
    "    pushfl\n"
    "    pushl %edi\n"
    "    call _TrBeforeParse\n"
    "    addl $4, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    pushl 4(%esp)\n"
    "    call *_g_trParse\n"
    "    addl $4, %esp\n"
    "    pushal\n"
    "    pushfl\n"
    "    call _TrAfterParse\n"
    "    popfl\n"
    "    popal\n"
    "    ret\n");

/* C1's own local film scan (0x72C0E0: cl_demo_list_local_*, PopulateLocalDemoFileInformation) builds
 * "<fs_homepath>/demo/" (free space, creating the folder) and "<fs_homepath>/demo/<all>.demo" (FindFirstFileA through its
 * own import at 0x9A1D41C, which no wrapper sees). Its two sprintf calls are given the player's folder instead: the
 * pushes of fs_homepath (0x2A9DCF8) and of the formats "%s/demo/" (0xC829E0) and "%s/demo/" "*%s" (0xC82A44). The films
 * it then opens, "/demo/<name>.demo" through the file system, reach OflDemoRedirect. */
static void TrPatchNativeScan(const WCHAR *dir)
{
    static char folder[MAX_PATH];
    static const char kOne[] = "%s", kPattern[] = "%s*%s";
    if (!WideCharToMultiByte(CP_ACP, 0, dir, -1, folder, MAX_PATH, NULL, NULL)) return;
    const struct { DWORD va, was; const void *now; } sites[] = {
        { 0x0072C10Au, 0x02A9DCF8u, folder }, { 0x0072C10Fu, 0x00C829E0u, kOne },
        { 0x0072C1D9u, 0x02A9DCF8u, folder }, { 0x0072C1DEu, 0x00C82A44u, kPattern },
    };
    for (unsigned i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
        const BYTE *p = (const BYTE *)(DWORD_PTR)sites[i].va;
        if (!Readable(p, 5) || p[0] != 0x68 || *(const DWORD *)(p + 1) != sites[i].was) {
            Log("theatre: C1's local film scan differs at 0x%08lX; it keeps its own folder", (unsigned long)sites[i].va);
            return;
        }
    }
    for (unsigned i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
        DWORD v = (DWORD)(DWORD_PTR)sites[i].now;
        WriteCode((BYTE *)(DWORD_PTR)sites[i].va + 1, &v, 4);
    }
    Log("theatre: C1's local film scan reads %s", folder);
}

/* Main thread, every frame: a film ends when the client leaves the match or starts playing a film. */
static void TheatreRecordTick(void)
{
    static BOOL ready;
    if (!ready) {   /* once the lobby has chosen the profile */
        WCHAR dir[MAX_PATH];
        if (!OflDemoDir(dir, TRUE)) return;
        ready = TRUE;
        TrTrim(dir, TRUE);
        TrPatchNativeScan(dir);
        Log("theatre: every match is recorded into %ls", dir);
    }
    if (!g_tr.active) return;
    BYTE *clc = TrClc();
    DWORD state = TrState();
    if (state == 0 || (clc && *(volatile DWORD *)(clc + TR_CLC_DEMOPLAYING))) { TrFinish("left the match"); return; }
    if (state == VOICE_CA_ACTIVE && (!g_tr.map[0] || g_tr.clientNum < 0)) TrLearn();
    if (g_tr.outLen && GetTickCount() - g_tr.flushedAt > 2000) TrFlush();
}

static void TrAtExit(void)
{
    if (g_tr.active) TrFinish("the game closed");
}

static void InstallTheatreRecorder(void)
{
    g_tr.file = INVALID_HANDLE_VALUE;
    BYTE *site = (BYTE *)(DWORD_PTR)TR_PARSE_CALL_VA;
    /* 6A 00  push 0 / 8B FB  mov edi, ebx / E8 rel32  call CL_ParseServerMessage / 83 C4 0C */
    if (!Readable(site - 4, 12) || site[-4] != 0x6A || site[-3] != 0x00 || site[-2] != 0x8B || site[-1] != 0xFB ||
        site[0] != 0xE8 ||
        (DWORD)(TR_PARSE_CALL_VA + 5 + *(const LONG *)(site + 1)) != TR_PARSE_VA || site[5] != 0x83 || site[6] != 0xC4 ||
        site[7] != 0x0C) {
        Log("theatre: CL_PacketEvent's parse call 0x%08X differs; matches are not recorded", TR_PARSE_CALL_VA);
        return;
    }
    g_tr.out = (BYTE *)VirtualAlloc(NULL, TR_BUF_SIZE + TR_MSG_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_tr.out) return;
    g_tr.msg = g_tr.out + TR_BUF_SIZE;
    if (!K33OnMainTick(TheatreRecordTick)) {
        Log("theatre: no main-thread tick left; matches are not recorded");
        return;
    }
    LONG rel = (LONG)((DWORD_PTR)TR_ParseThunk - (TR_PARSE_CALL_VA + 5));
    WriteCode(site + 1, &rel, 4);
    g_tr.installed = TRUE;
    atexit(TrAtExit);
}
