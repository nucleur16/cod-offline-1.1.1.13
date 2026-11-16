/*
 * k33shot.c - back-buffer screenshots and per-render-target draw statistics for test harnesses.
 *
 * Diagnostic profile only (K33_DIAGNOSTICS, with console.c): shipping and quiet-QA builds do not compile this
 * file. It rides on the console's Direct3D hooks (console.c: Present of the device and of swap chain 0), so no
 * second d3d9 proxy DLL is needed next to the client. A screen copy of the game window is black while the display
 * is off (Windows drops the frames of a windowed Direct3D 9 program), but the game still renders its back buffer.
 *
 *   r_screenshot <file.png|file.bmp>   console command: the next presented frame is written to that file
 *   com_frameStats <file.txt>            console command: what the next complete frame draws into (below)
 *   <exe dir>\k33_shot.req               one line: the path of the picture to write (.png or .bmp)
 *   <exe dir>\k33_stats.req              one line: the path of the statistics text file
 *
 * The two request files are for a harness outside the process (no input focus needed). They are polled on the
 * thread that presents, at most 20 times a second, and only on a test client: "+set r_shotFile 1" or the
 * existing "+set com_cmdFile 1" on the command line. A request file is deleted when it is read. The picture is
 * written to <path>.tmp and renamed, so a reader never sees half a file.
 *
 * The picture is the back buffer just before Present, after Kernel33's own overlays (console, FPS line) were
 * drawn: what the player would see. A multisampled back buffer is resolved first. Only 32-bit
 * X8R8G8B8 / A8R8G8B8 back buffers are written (what C1 creates).
 *
 * Statistics: one line per pair of colour render target and depth buffer that the frame drew into,
 *   color WxH format F usage 0xU, depth WxH format F usage 0xU: N draws, M primitives
 * counted from DrawPrimitive and DrawIndexedPrimitive (the calls C1's renderer uses). A shadow-map pass shows as
 * draws into a depth buffer that has not the back buffer's size. The four device entries are hooked on the first
 * statistics request, not before, so a client that is never asked pays nothing; nothing the game draws changes.
 */

#include <d3d9.h>
#include <zlib.h>

typedef HRESULT(WINAPI *ShotSetTargetFn)(IDirect3DDevice9 *, DWORD, IDirect3DSurface9 *);
typedef HRESULT(WINAPI *ShotSetDepthFn)(IDirect3DDevice9 *, IDirect3DSurface9 *);
typedef HRESULT(WINAPI *ShotDrawFn)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT(WINAPI *ShotDrawIndexedFn)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);

typedef struct { UINT w, h; D3DFORMAT format; DWORD usage; } ShotTarget;
typedef struct { ShotTarget color, depth; LONG draws, primitives; } ShotPass;
#define SHOT_MAX_PASSES 48

static CRITICAL_SECTION g_shotLock;
static BOOL g_shotInit;
static char g_shotCmdPath[MAX_PATH], g_shotCmdStats[MAX_PATH]; /* from the console commands, under g_shotLock */
static char g_shotReqFile[MAX_PATH], g_shotStatsReqFile[MAX_PATH];
static int g_shotFileRequests = -1;
static volatile LONG g_shotCaptures, g_shotStatsWritten;

static ShotSetTargetFn g_shotSetTarget;
static ShotSetDepthFn g_shotSetDepth;
static ShotDrawFn g_shotDraw;
static ShotDrawIndexedFn g_shotDrawIndexed;
static void *g_shotHookedDevice;
static ShotPass g_shotFrame[SHOT_MAX_PASSES];
static int g_shotFramePasses;
static ShotTarget g_shotColor, g_shotDepth;
static char g_shotStatsPending[MAX_PATH]; /* presenting thread only: written at the next Present */
static BOOL g_shotCounting;

static int VoiceArgc(void);              /* voice.c */
static const char *VoiceArgv(int n);

/* ---- files --------------------------------------------------------------------------------------------- */

static void ShotPngChunk(FILE *f, const char type[4], const BYTE *data, DWORD size)
{
    BYTE head[8];
    head[0] = (BYTE)(size >> 24); head[1] = (BYTE)(size >> 16); head[2] = (BYTE)(size >> 8); head[3] = (BYTE)size;
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    if (size) fwrite(data, 1, size, f);
    uLong crc = crc32(0L, head + 4, 4);
    if (size) crc = crc32(crc, data, size);
    BYTE tail[4] = { (BYTE)(crc >> 24), (BYTE)(crc >> 16), (BYTE)(crc >> 8), (BYTE)crc };
    fwrite(tail, 1, 4, f);
}

/* 24-bit RGB, top-down rows of `width` pixels from a 32-bit BGRX surface. */
static BOOL ShotWritePng(FILE *f, const BYTE *bits, int pitch, UINT width, UINT height)
{
    static const BYTE kSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    size_t rawSize = (size_t)height * (1 + (size_t)width * 3);
    BYTE *raw = (BYTE *)malloc(rawSize);
    uLongf packedSize = compressBound((uLong)rawSize);
    BYTE *packed = raw ? (BYTE *)malloc(packedSize) : NULL;
    BOOL ok = FALSE;
    if (raw && packed) {
        BYTE *out = raw;
        for (UINT y = 0; y < height; y++) {
            const BYTE *src = bits + (size_t)pitch * y;
            *out++ = 0; /* filter: none */
            for (UINT x = 0; x < width; x++) { out[0] = src[x * 4 + 2]; out[1] = src[x * 4 + 1]; out[2] = src[x * 4]; out += 3; }
        }
        if (compress2(packed, &packedSize, raw, (uLong)rawSize, 1) == Z_OK) {
            BYTE ihdr[13] = { (BYTE)(width >> 24), (BYTE)(width >> 16), (BYTE)(width >> 8), (BYTE)width,
                              (BYTE)(height >> 24), (BYTE)(height >> 16), (BYTE)(height >> 8), (BYTE)height,
                              8, 2, 0, 0, 0 };
            fwrite(kSig, 1, 8, f);
            ShotPngChunk(f, "IHDR", ihdr, 13);
            ShotPngChunk(f, "IDAT", packed, (DWORD)packedSize);
            ShotPngChunk(f, "IEND", NULL, 0);
            ok = !ferror(f);
        }
    }
    free(packed);
    free(raw);
    return ok;
}

static BOOL ShotWriteBmp(FILE *f, const BYTE *bits, int pitch, UINT width, UINT height)
{
    UINT row = (width * 3 + 3) & ~3u;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + row * height;
    ih.biSize = sizeof(ih);
    ih.biWidth = (LONG)width;
    ih.biHeight = (LONG)height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    BYTE *line = (BYTE *)calloc(1, row);
    if (!line) return FALSE;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    for (UINT y = 0; y < height; y++) {
        const BYTE *src = bits + (size_t)pitch * (height - 1 - y);
        for (UINT x = 0; x < width; x++) { line[x * 3] = src[x * 4]; line[x * 3 + 1] = src[x * 4 + 1]; line[x * 3 + 2] = src[x * 4 + 2]; }
        fwrite(line, row, 1, f);
    }
    free(line);
    return !ferror(f);
}

static BOOL ShotHasExt(const char *path, const char *ext)
{
    size_t n = strlen(path), e = strlen(ext);
    return n > e && !_stricmp(path + n - e, ext);
}

/* -> NULL when written, else why not. Presenting thread. */
static const char *ShotCapture(IDirect3DDevice9 *dev, const char *path)
{
    BOOL png = ShotHasExt(path, ".png");
    if (!png && !ShotHasExt(path, ".bmp")) return "the file name must end in .png or .bmp";
    IDirect3DSurface9 *back = NULL, *resolved = NULL, *memory = NULL;
    const char *why = "the back buffer could not be read";
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || !back) return "no back buffer";
    D3DSURFACE_DESC desc;
    memset(&desc, 0, sizeof(desc));
    IDirect3DSurface9_GetDesc(back, &desc);
    if (desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_X8R8G8B8) why = "the back buffer is not a 32-bit RGB surface";
    /* the back buffer is multisampled as a rule: resolve it into a plain render target first */
    else if (SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0,
                                                           FALSE, &resolved, NULL)) &&
             SUCCEEDED(IDirect3DDevice9_StretchRect(dev, back, NULL, resolved, NULL, D3DTEXF_NONE)) &&
             SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, desc.Width, desc.Height, desc.Format,
                                                                    D3DPOOL_SYSTEMMEM, &memory, NULL)) &&
             SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, resolved, memory))) {
        D3DLOCKED_RECT lock;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(memory, &lock, NULL, D3DLOCK_READONLY))) {
            char tmp[MAX_PATH + 8];
            _snprintf(tmp, sizeof(tmp), "%s.tmp", path);
            tmp[sizeof(tmp) - 1] = 0;
            FILE *f = fopen(tmp, "wb");
            if (!f) why = "the file could not be created";
            else {
                BOOL ok = png ? ShotWritePng(f, (const BYTE *)lock.pBits, lock.Pitch, desc.Width, desc.Height)
                              : ShotWriteBmp(f, (const BYTE *)lock.pBits, lock.Pitch, desc.Width, desc.Height);
                if (fclose(f)) ok = FALSE;
                if (ok && MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) why = NULL;
                else { DeleteFileA(tmp); why = "the file could not be written"; }
            }
            IDirect3DSurface9_UnlockRect(memory);
        }
    }
    if (memory) IDirect3DSurface9_Release(memory);
    if (resolved) IDirect3DSurface9_Release(resolved);
    IDirect3DSurface9_Release(back);
    if (!why) {
        InterlockedIncrement(&g_shotCaptures);
        Log("shot: %ux%u back buffer written to %s", desc.Width, desc.Height, path);
    } else Log("shot: %s not written: %s", path, why);
    return why;
}

/* ---- what a frame draws into (observation only: every call is passed on unchanged) ------------------------- */

static ShotTarget ShotDescribe(IDirect3DSurface9 *surface)
{
    ShotTarget t;
    D3DSURFACE_DESC desc;
    memset(&t, 0, sizeof(t));
    if (surface && SUCCEEDED(IDirect3DSurface9_GetDesc(surface, &desc))) {
        t.w = desc.Width; t.h = desc.Height; t.format = desc.Format; t.usage = desc.Usage;
    }
    return t;
}

static void ShotCountDraw(UINT primitives)
{
    if (!g_shotCounting) return;
    for (int k = 0; k < g_shotFramePasses; k++)
        if (!memcmp(&g_shotFrame[k].color, &g_shotColor, sizeof(ShotTarget)) &&
            !memcmp(&g_shotFrame[k].depth, &g_shotDepth, sizeof(ShotTarget))) {
            g_shotFrame[k].draws++;
            g_shotFrame[k].primitives += (LONG)primitives;
            return;
        }
    if (g_shotFramePasses < SHOT_MAX_PASSES) {
        ShotPass *p = &g_shotFrame[g_shotFramePasses++];
        p->color = g_shotColor; p->depth = g_shotDepth; p->draws = 1; p->primitives = (LONG)primitives;
    }
}

static HRESULT WINAPI ShotHookSetTarget(IDirect3DDevice9 *dev, DWORD index, IDirect3DSurface9 *surface)
{
    if (index == 0) g_shotColor = ShotDescribe(surface);
    return g_shotSetTarget(dev, index, surface);
}
static HRESULT WINAPI ShotHookSetDepth(IDirect3DDevice9 *dev, IDirect3DSurface9 *surface)
{
    g_shotDepth = ShotDescribe(surface);
    return g_shotSetDepth(dev, surface);
}
static HRESULT WINAPI ShotHookDraw(IDirect3DDevice9 *dev, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    ShotCountDraw(count);
    return g_shotDraw(dev, type, start, count);
}
static HRESULT WINAPI ShotHookDrawIndexed(IDirect3DDevice9 *dev, D3DPRIMITIVETYPE type, INT base, UINT minIndex, UINT vertices,
                                          UINT start, UINT count)
{
    ShotCountDraw(count);
    return g_shotDrawIndexed(dev, type, base, minIndex, vertices, start, count);
}

static void *ShotPatchVtable(void *obj, int index, void *hook)
{
    void **vt = *(void ***)obj;
    DWORD old;
    void *orig = vt[index];
    if (orig == hook) return NULL;
    if (!VirtualProtect(&vt[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return NULL;
    vt[index] = hook;
    VirtualProtect(&vt[index], sizeof(void *), old, &old);
    return orig;
}

/* Presenting thread, on the first statistics request: the device's SetRenderTarget (37), SetDepthStencilSurface
 * (39), DrawPrimitive (81) and DrawIndexedPrimitive (82). */
static BOOL ShotHookDraws(IDirect3DDevice9 *dev)
{
    if (g_shotHookedDevice != dev) {
        void *p;
        if ((p = ShotPatchVtable(dev, 37, (void *)ShotHookSetTarget)) != NULL) g_shotSetTarget = (ShotSetTargetFn)p;
        if ((p = ShotPatchVtable(dev, 39, (void *)ShotHookSetDepth)) != NULL) g_shotSetDepth = (ShotSetDepthFn)p;
        if ((p = ShotPatchVtable(dev, 81, (void *)ShotHookDraw)) != NULL) g_shotDraw = (ShotDrawFn)p;
        if ((p = ShotPatchVtable(dev, 82, (void *)ShotHookDrawIndexed)) != NULL) g_shotDrawIndexed = (ShotDrawIndexedFn)p;
        if (!g_shotSetTarget || !g_shotSetDepth || !g_shotDraw || !g_shotDrawIndexed) return FALSE;
        g_shotHookedDevice = dev;
        Log("shot: draw statistics hooked on device %p", (void *)dev);
    }
    IDirect3DSurface9 *now = NULL;
    if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev, 0, &now)) && now) { g_shotColor = ShotDescribe(now); IDirect3DSurface9_Release(now); }
    now = NULL;
    if (SUCCEEDED(IDirect3DDevice9_GetDepthStencilSurface(dev, &now)) && now) { g_shotDepth = ShotDescribe(now); IDirect3DSurface9_Release(now); }
    else memset(&g_shotDepth, 0, sizeof(g_shotDepth));
    return TRUE;
}

static void ShotWriteStats(const char *path)
{
    char tmp[MAX_PATH + 8];
    _snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    tmp[sizeof(tmp) - 1] = 0;
    FILE *f = fopen(tmp, "wb");
    if (!f) { Log("shot: statistics file %s could not be created", path); return; }
    for (int k = 0; k < g_shotFramePasses; k++) {
        const ShotPass *p = &g_shotFrame[k];
        fprintf(f, "color %ux%u format %d usage 0x%lX, depth %ux%u format %d usage 0x%lX: %ld draws, %ld primitives\n",
                p->color.w, p->color.h, (int)p->color.format, (unsigned long)p->color.usage, p->depth.w, p->depth.h,
                (int)p->depth.format, (unsigned long)p->depth.usage, (long)p->draws, (long)p->primitives);
    }
    fclose(f);
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) { DeleteFileA(tmp); Log("shot: statistics file %s could not be written", path); return; }
    InterlockedIncrement(&g_shotStatsWritten);
    Log("shot: frame statistics (%d target pair(s)) written to %s", g_shotFramePasses, path);
}

/* ---- requests ------------------------------------------------------------------------------------------ */

/* One line of a request file; the file is deleted. */
static BOOL ShotReadRequest(const char *file, char *out, size_t outSize)
{
    out[0] = 0;
    if (GetFileAttributesA(file) == INVALID_FILE_ATTRIBUTES) return FALSE;
    FILE *f = fopen(file, "rb");
    if (!f) return FALSE;
    size_t got = fread(out, 1, outSize - 1, f);
    fclose(f);
    out[got] = 0;
    for (char *c = out; *c; c++)
        if (*c == '\r' || *c == '\n') { *c = 0; break; }
    DeleteFileA(file);
    return out[0] != 0;
}

static BOOL ShotFileRequests(void)
{
    if (g_shotFileRequests < 0) {
        const char *cl = GetCommandLineA();
        g_shotFileRequests = cl && (strstr(cl, "r_shotFile 1") || strstr(cl, "com_cmdFile 1"));
        DWORD n = GetModuleFileNameA(NULL, g_shotReqFile, MAX_PATH - 20);
        while (n && g_shotReqFile[n - 1] != '\\') n--;
        g_shotReqFile[n] = 0;
        lstrcpyA(g_shotStatsReqFile, g_shotReqFile);
        lstrcatA(g_shotReqFile, "k33_shot.req");
        lstrcatA(g_shotStatsReqFile, "k33_stats.req");
        if (g_shotFileRequests) Log("shot: request files %s and %s are polled", g_shotReqFile, g_shotStatsReqFile);
    }
    return g_shotFileRequests;
}

/* console.c's Present hooks call this once per presented frame, after the overlays are drawn. */
static void ShotBeforePresent(IDirect3DDevice9 *dev)
{
    static DWORD lastPoll;
    char path[MAX_PATH], stats[MAX_PATH];
    if (!g_shotInit || !dev) return;
    /* the frame that just ended is complete: its statistics were asked for one Present ago */
    if (g_shotStatsPending[0]) {
        ShotWriteStats(g_shotStatsPending);
        g_shotStatsPending[0] = 0;
        g_shotCounting = FALSE;
    }
    path[0] = stats[0] = 0;
    EnterCriticalSection(&g_shotLock);
    if (g_shotCmdPath[0]) { lstrcpynA(path, g_shotCmdPath, MAX_PATH); g_shotCmdPath[0] = 0; }
    if (g_shotCmdStats[0]) { lstrcpynA(stats, g_shotCmdStats, MAX_PATH); g_shotCmdStats[0] = 0; }
    LeaveCriticalSection(&g_shotLock);
    DWORD now = GetTickCount();
    if (ShotFileRequests() && now - lastPoll >= 50) {
        lastPoll = now;
        if (!stats[0]) ShotReadRequest(g_shotStatsReqFile, stats, sizeof(stats));
        if (!path[0]) ShotReadRequest(g_shotReqFile, path, sizeof(path));
    }
    if (stats[0]) {
        if (ShotHookDraws(dev)) {
            g_shotFramePasses = 0;
            g_shotCounting = TRUE; /* the frame that starts after this Present */
            lstrcpynA(g_shotStatsPending, stats, MAX_PATH);
        } else Log("shot: draw statistics could not be hooked; %s not written", stats);
    }
    if (path[0] && IDirect3DDevice9_TestCooperativeLevel(dev) == D3D_OK) ShotCapture(dev, path);
    else if (path[0]) Log("shot: %s not written: the device is lost", path);
}

static BOOL ShotWanted(void)
{
    return g_shotInit && (g_shotStatsPending[0] || g_shotCmdPath[0] || g_shotCmdStats[0] || ShotFileRequests());
}

/* ---- commands (main thread) ---------------------------------------------------------------------------- */

static void ShotCmdQueue(char *slot, const char *what)
{
    if (VoiceArgc() != 2 || strlen(VoiceArgv(1)) >= MAX_PATH || strchr(VoiceArgv(1), '"')) {
        C1Printf("%s <file>\n", what);
        return;
    }
    EnterCriticalSection(&g_shotLock);
    lstrcpynA(slot, VoiceArgv(1), MAX_PATH);
    LeaveCriticalSection(&g_shotLock);
    C1Printf("%s: %s is written with the next frame\n", what, VoiceArgv(1));
}
static void __cdecl ShotCmdScreenshot(void)
{
    if (VoiceArgc() == 2 && !ShotHasExt(VoiceArgv(1), ".png") && !ShotHasExt(VoiceArgv(1), ".bmp")) {
        C1Printf("r_screenshot <file.png|file.bmp>\n");
        return;
    }
    ShotCmdQueue(g_shotCmdPath, "r_screenshot");
}
static void __cdecl ShotCmdStats(void) { ShotCmdQueue(g_shotCmdStats, "com_frameStats"); }

static C1Cmd g_shotCmds[] = {
    { NULL, "r_screenshot", (void *)ShotCmdScreenshot },
    { NULL, "com_frameStats", (void *)ShotCmdStats },
};

/* TakeOver, after InstallConsole (whose Direct3DCreate9 hook leads here). */
static void InstallShot(void)
{
    if (!g_shotInit) { InitializeCriticalSection(&g_shotLock); g_shotInit = TRUE; }
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_shotCmds) / sizeof(g_shotCmds[0]); i++) added += C1AddCommand(&g_shotCmds[i]);
    Log("shot: %d commands (r_screenshot, com_frameStats); request files %s", added,
        ShotFileRequests() ? "polled" : "off (r_shotFile 1 or com_cmdFile 1 turns them on)");
}
