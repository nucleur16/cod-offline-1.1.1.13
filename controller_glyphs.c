/*
 * controller_glyphs.c - gamepad button icons inside C1's text (feat/controller phase 3). Included by
 * controller.c after CtlApplyPadNames.
 *
 * C1's text renderer still has IW4's inline icon escape: '^', 1 or 2, width, height, then a raw 4-byte
 * Material pointer (0xAAB26E / 0xAAB8B2 step over 7 bytes after the '^'; 0xAAA7D0 draws the icon at
 * ((size - 16) * fontHeight + 16) / 32 pixels, so 48 is one line high). C1's zones have no button
 * materials, so Kernel33 makes them:
 *
 *   art        Xelu's CC0 controller prompts (glyphs/xelu, glyphs/LICENSE.md): Xbox 360, PS3, Wii U and a generic
 *              set, 17 icons each in kCtlButtons order; glyphs/make_glyphs.py converts them to 64x64 with mips,
 *              B8G8R8A8, into glyphs/glyphs.bin (zlib), linked in with .incbin.
 *   materials  one per button: a copy of C1's "white" material (2D, alpha blended) whose texture table points
 *              at a copy of its GfxImage. They sit at addresses whose four bytes are letters (base 0xHHHH0000
 *              with HH a letter, + (0x41 + button) * 0x100 + 0x44), so the escape survives every string copy
 *              and never holds a 0, '^', '%' or '&'.
 *   textures   made on the render thread, where C1 draws the icon: the calls 0xAAB320 / 0xAAB92D to 0xAAA7D0
 *              come here first. For the set in use (gpad_glyphs, or the pad's SDL type) the button's D3D9
 *              texture is created once (CreateTexture on the device of white's texture, managed pool, all mips)
 *              and put in that button's image. A set change only swaps the image's texture pointer: live.
 *   names      Key_GetBindingNames 0x493920 (every "[{+attack}]" in hints, prompts and old-style menus)
 *              converts a key with Key_KeynumToString at 0x49397C and 0x4A19A1. Those calls come here: a pad
 *              key gives its icon escape. Scaleform (0x5B080D, AS key-binding names: Controls screen, pause
 *              menu) and GSC (0x55A8B6) keep the family's text names (Scaleform has no IW4 icon escape).
 *
 * If a layout check fails (white's material, its texture table, its image or texture), the icons stay off
 * and every hint keeps the text names; gpad_glyphdump prints what was found.
 */

#define CTL_GLYPH_SIZE         64
#define CTL_GLYPH_LEVELS       7
#define CTL_GLYPH_SETS         4
#define CTL_GLYPH_ESC_SIZE     56            /* 48 = one text line high; a little more reads better */
#define CTL_MATERIAL_REGISTER  0x00A78170u   /* Material_RegisterHandle(name, imageTrack) */
#define CTL_KEYNUM_TO_STRING   0x00492AF0u   /* Key_KeynumToString(eax = key, edi = translate) */
#define CTL_BINDING_NAMES      0x00493920u   /* Key_GetBindingNames(eax = buffer, ecx = lc; command) */
#define CTL_ICON_DRAW          0x00AAA7D0u   /* text icon drawer (ecx = escape after '^', eax = font) */
/* IW4 layouts, checked at run time */
#define CTL_MAT_TEXCOUNT       0x48
#define CTL_MAT_TEXTABLE       0x54
#define CTL_MAT_COPY           0xB0          /* bytes of the material copied (IW4 0x60, plus room) */
#define CTL_TEXDEF_SIZE        12
#define CTL_TEXDEF_IMAGE       8
#define CTL_IMAGE_COPY         0x40

__asm__(".section .rdata,\"dr\"\n"
        ".globl _g_glyphBlob\n"
        "_g_glyphBlob:\n"
        ".incbin \"glyphs/glyphs.bin\"\n"
        ".globl _g_glyphBlobEnd\n"
        "_g_glyphBlobEnd:\n"
        ".text\n");
extern const BYTE g_glyphBlob[], g_glyphBlobEnd[];

typedef struct {
    BYTE *mat;                       /* the material (at its letter address) */
    BYTE texTable[16 * CTL_TEXDEF_SIZE];
    BYTE image[CTL_IMAGE_COPY];
    void *tex[CTL_GLYPH_SETS];       /* IDirect3DTexture9 *, made on the render thread */
    char esc[9];                     /* '^', 1, w, h, Material * (4 bytes), 0 */
} CtlGlyph;

static CtlGlyph g_glyphs[CTL_NBUTTONS];
static BYTE *g_glyphPix;             /* inflated glyphs.bin (header skipped) */
static BYTE *g_glyphBase;            /* the 64 KB block that holds the materials */
static const BYTE *g_glyphTemplate;  /* C1's "white" */
static void *g_glyphTemplateTex;     /* its texture: the device comes from it */
static volatile LONG g_glyphReady;   /* 1 icons usable, -1 off for good */
static volatile LONG g_glyphSet;     /* the set the render thread uses; -1 text names */
static volatile LONG g_glyphFailed;  /* the render thread could not make a texture */
__attribute__((used)) int g_ctlTextOnly;           /* inside the Scaleform / GSC callers of Key_GetBindingNames */
static int g_glyphTries;
static DWORD g_glyphNextTry;
static char g_glyphWhy[160] = "not tried yet";

/* A D3D9 object: its vtable's first entry is code (d3d9.dll's, or a wrapper's). */
static BOOL CtlIsD3dObject(const void *p)
{
    if (!p || !Readable(p, 4)) return FALSE;
    const void *vt = *(const void *const *)p;
    if (!vt || !Readable(vt, 0x60)) return FALSE;
    const void *f = ((const void *const *)vt)[0];
    MEMORY_BASIC_INFORMATION mbi;
    if (!f || !VirtualQuery(f, &mbi, sizeof(mbi))) return FALSE;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

static size_t CtlGlyphBytes(void)
{
    size_t n = 0;
    for (int l = 0; l < CTL_GLYPH_LEVELS; l++) n += (size_t)(CTL_GLYPH_SIZE >> l) * (CTL_GLYPH_SIZE >> l) * 4;
    return n;
}

static BOOL CtlGlyphInflate(void)
{
    size_t per = CtlGlyphBytes();
    uLongf len = (uLongf)(24 + (size_t)CTL_GLYPH_SETS * CTL_NBUTTONS * per);
    BYTE *buf = (BYTE *)malloc(len);
    if (!buf) return FALSE;
    int rc = uncompress(buf, &len, g_glyphBlob, (uLong)(g_glyphBlobEnd - g_glyphBlob));
    const DWORD *h = (const DWORD *)(buf + 4);
    if (rc != Z_OK || memcmp(buf, "K33G", 4) || h[0] != 1 || h[1] != CTL_GLYPH_SETS || h[2] != (DWORD)CTL_NBUTTONS ||
        h[3] != CTL_GLYPH_SIZE || h[4] != CTL_GLYPH_LEVELS || len != 24 + (size_t)CTL_GLYPH_SETS * CTL_NBUTTONS * per) {
        _snprintf(g_glyphWhy, sizeof(g_glyphWhy), "glyphs.bin does not match this build (zlib %d, %lu bytes)", rc, (unsigned long)len);
        free(buf);
        return FALSE;
    }
    g_glyphPix = buf + 24;
    return TRUE;
}

/* The template's image (first texture def whose image holds a D3D texture), or NULL. */
static const BYTE *CtlGlyphTemplateImage(const BYTE *mat, int *count)
{
    *count = *(const BYTE *)(mat + CTL_MAT_TEXCOUNT);
    const BYTE *table = *(const BYTE *const *)(mat + CTL_MAT_TEXTABLE);
    if (*count < 1 || *count > 16 || !Readable(table, (SIZE_T)*count * CTL_TEXDEF_SIZE)) return NULL;
    for (int i = 0; i < *count; i++) {
        const BYTE *img = *(const BYTE *const *)(table + i * CTL_TEXDEF_SIZE + CTL_TEXDEF_IMAGE);
        if (img && Readable(img, CTL_IMAGE_COPY) && CtlIsD3dObject(*(void *const *)img)) return img;
    }
    return NULL;
}

/* Main thread, once the renderer has white's texture: the 17 materials. */
static void CtlGlyphInit(void)
{
    typedef const BYTE *(__cdecl *RegFn)(const char *, int);
    static const BYTE kReg[] = { 0x8B, 0x44, 0x24, 0x04, 0x80, 0x38, 0x00 };
    if (!Readable((void *)(DWORD_PTR)CTL_MATERIAL_REGISTER, sizeof(kReg)) ||
        memcmp((void *)(DWORD_PTR)CTL_MATERIAL_REGISTER, kReg, sizeof(kReg))) {
        lstrcpynA(g_glyphWhy, "Material_RegisterHandle bytes differ", sizeof(g_glyphWhy));
        InterlockedExchange(&g_glyphReady, -1);
        return;
    }
    const BYTE *mat = ((RegFn)(DWORD_PTR)CTL_MATERIAL_REGISTER)("white", 7);
    const char *name = mat && Readable(mat, CTL_MAT_COPY) ? *(const char *const *)mat : NULL;
    if (!name || !Readable(name, 6) || strcmp(name, "white")) {
        _snprintf(g_glyphWhy, sizeof(g_glyphWhy), "material \"white\" not found (got %p)", (const void *)mat);
        return;   /* try again later */
    }
    int count;
    const BYTE *img = CtlGlyphTemplateImage(mat, &count);
    if (!img) {
        _snprintf(g_glyphWhy, sizeof(g_glyphWhy), "white (%p): no texture table / image with a D3D texture (count %d)", (const void *)mat, count);
        return;   /* its texture may not be loaded yet */
    }
    if (!g_glyphPix && !CtlGlyphInflate()) { InterlockedExchange(&g_glyphReady, -1); return; }
    if (!g_glyphBase) {
        static const WORD kHi[] = { 0x4B4B, 0x4C4C, 0x4D4D, 0x4E4E, 0x4F4F, 0x5050, 0x5151, 0x5252, 0x5353, 0x5454,
                                    0x5555, 0x5656, 0x5757, 0x5858, 0x5959, 0x5A5A, 0x4A4A, 0x4949, 0x4848, 0x4747 };
        for (unsigned i = 0; i < sizeof(kHi) / sizeof(kHi[0]) && !g_glyphBase; i++)
            g_glyphBase = (BYTE *)VirtualAlloc((void *)(DWORD_PTR)((DWORD)kHi[i] << 16), 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!g_glyphBase) {
            lstrcpynA(g_glyphWhy, "no free 64 KB block at a letter address", sizeof(g_glyphWhy));
            InterlockedExchange(&g_glyphReady, -1);
            return;
        }
    }
    g_glyphTemplate = mat;
    const BYTE *table = *(const BYTE *const *)(mat + CTL_MAT_TEXTABLE);
    for (int b = 0; b < CTL_NBUTTONS; b++) {
        CtlGlyph *g = &g_glyphs[b];
        g->mat = g_glyphBase + ((0x41 + b) << 8) + 0x44;
        memcpy(g->image, img, CTL_IMAGE_COPY);
        memcpy(g->texTable, table, (size_t)count * CTL_TEXDEF_SIZE);
        for (int i = 0; i < count; i++) {
            BYTE **slot = (BYTE **)(g->texTable + i * CTL_TEXDEF_SIZE + CTL_TEXDEF_IMAGE);
            if (*slot == img) *slot = g->image;
        }
        memcpy(g->mat, mat, CTL_MAT_COPY);
        *(BYTE **)(g->mat + CTL_MAT_TEXTABLE) = g->texTable;
        DWORD p = (DWORD)(DWORD_PTR)g->mat;
        g->esc[0] = '^';
        g->esc[1] = 1;
        g->esc[2] = CTL_GLYPH_ESC_SIZE;
        g->esc[3] = CTL_GLYPH_ESC_SIZE;
        memcpy(g->esc + 4, &p, 4);
        g->esc[8] = 0;
    }
    g_glyphTemplateTex = *(void *const *)img;
    _snprintf(g_glyphWhy, sizeof(g_glyphWhy), "ready: white %p, %d texture(s), image %p, materials at %p", (const void *)mat, count,
              (const void *)img, (void *)g_glyphBase);
    Log("controller: glyphs %s", g_glyphWhy);
    InterlockedExchange(&g_glyphReady, 1);
}

/* The set the render thread draws (-1 = text names). */
static void CtlGlyphFrame(void)
{
    if (g_glyphReady == 0 && g_ctlDvarsDone && C1DvarsReady() && g_glyphTries < 60 && (LONG)(GetTickCount() - g_glyphNextTry) >= 0) {
        g_glyphTries++;
        g_glyphNextTry = GetTickCount() + 1000;
        CtlGlyphInit();
        if (g_glyphReady == 0 && g_glyphTries == 60) {
            Log("controller: glyphs off: %s", g_glyphWhy);
            InterlockedExchange(&g_glyphReady, -1);
        }
    }
    LONG set = C1DvarInt(g_gpGlyphs, 0) == 5 || g_glyphReady != 1 || g_glyphFailed ? -1 : CtlPadFamily();
    if (set != g_glyphSet) {
        InterlockedExchange(&g_glyphSet, set);
        static const char *const kSet[] = { "Xbox 360", "PS3", "Wii U", "generic" };
        Log("controller: button icons: %s", set < 0 ? "off (text names)" : kSet[set]);
    }
}

/* Key_KeynumToString in Key_GetBindingNames: the icon escape for a pad key, or NULL (C1's own name). */
__attribute__((used)) const char *__cdecl CtlGlyphName(int key)
{
    if (g_ctlTextOnly || g_glyphSet < 0 || g_glyphReady != 1) return NULL;
    for (int b = 0; b < CTL_NBUTTONS; b++)
        if (kCtlButtons[b].key == key) return g_glyphs[b].esc;
    return NULL;
}

/* ---- render thread ---- */
typedef struct { INT Pitch; void *pBits; } CtlLockedRect;

static void *CtlGlyphMakeTexture(int set, int button)
{
    void *tmpl = g_glyphTemplateTex, *dev = NULL, *tex = NULL;
    if (!tmpl) return NULL;
    /* IDirect3DResource9::GetDevice (3) */
    if (((HRESULT(WINAPI *)(void *, void **))(*(void ***)tmpl)[3])(tmpl, &dev) < 0 || !dev) return NULL;
    typedef HRESULT(WINAPI * CreateTex)(void *, UINT, UINT, UINT, DWORD, DWORD, DWORD, void **, HANDLE *);
    CreateTex create = (CreateTex)(*(void ***)dev)[23];
    HRESULT hr = create(dev, CTL_GLYPH_SIZE, CTL_GLYPH_SIZE, CTL_GLYPH_LEVELS, 0, 21 /* A8R8G8B8 */, 1 /* MANAGED */, &tex, NULL);
    DWORD lockFlags = 0;
    if (hr < 0 || !tex) {   /* a D3D9Ex device has no managed pool */
        tex = NULL;
        hr = create(dev, CTL_GLYPH_SIZE, CTL_GLYPH_SIZE, CTL_GLYPH_LEVELS, 0x200 /* DYNAMIC */, 21, 0 /* DEFAULT */, &tex, NULL);
        lockFlags = 0x2000; /* DISCARD */
    }
    ((ULONG(WINAPI *)(void *))(*(void ***)dev)[2])(dev);   /* Release the GetDevice reference */
    if (hr < 0 || !tex) { Log("controller: glyphs: CreateTexture failed (0x%08lX)", (unsigned long)hr); return NULL; }
    const BYTE *pix = g_glyphPix + ((size_t)set * CTL_NBUTTONS + button) * CtlGlyphBytes();
    for (int l = 0; l < CTL_GLYPH_LEVELS; l++) {
        int sz = CTL_GLYPH_SIZE >> l;
        CtlLockedRect r;
        if (((HRESULT(WINAPI *)(void *, UINT, CtlLockedRect *, const RECT *, DWORD))(*(void ***)tex)[19])(tex, l, &r, NULL, lockFlags) >= 0) {
            for (int y = 0; y < sz; y++) memcpy((BYTE *)r.pBits + y * r.Pitch, pix + y * sz * 4, (size_t)sz * 4);
            ((HRESULT(WINAPI *)(void *, UINT))(*(void ***)tex)[20])(tex, l);
        }
        pix += (size_t)sz * sz * 4;
    }
    return tex;
}

/* Returns 1 for one of our icons, 0 for anything else. For ours the hook sets the vertex colour to white
 * (alpha kept): C1 multiplies text icons by the text colour, and hint key names are yellow, so white is
 * what leaves the art's own colours (green A, red B, ...) untouched. */
__attribute__((used)) int __cdecl CtlGlyphPrepare(const BYTE *esc)
{
    if (!g_glyphBase || g_glyphReady != 1) return 0;
    const BYTE *mat = *(const BYTE *const *)(esc + 3);
    if (mat < g_glyphBase || mat >= g_glyphBase + 0x10000) return 0;
    int b = (int)(((DWORD_PTR)(mat - g_glyphBase) >> 8) - 0x41);
    LONG set = g_glyphSet;
    if (b < 0 || b >= CTL_NBUTTONS || g_glyphs[b].mat != mat) return 0;
    if (set < 0) set = CtlPadFamily();   /* a string made before a switch to text names */
    CtlGlyph *g = &g_glyphs[b];
    if (!g->tex[set]) {
        g->tex[set] = CtlGlyphMakeTexture(set, b);
        if (!g->tex[set]) { InterlockedExchange(&g_glyphFailed, 1); return 0; }
    }
    *(void **)g->image = g->tex[set];
    return 1;
}

/* ---- hooks ---- */
__attribute__((used)) DWORD g_ctlKeyToString = CTL_KEYNUM_TO_STRING;
__attribute__((used)) DWORD g_ctlBindingNames = CTL_BINDING_NAMES;
__attribute__((used)) DWORD g_ctlIconDraw = CTL_ICON_DRAW;
void CTL_KeyNameHook(void);
void CTL_BindNamesTextHook(void);
void CTL_IconDrawHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_KeyNameHook\n"
    "_CTL_KeyNameHook:\n"             /* call to Key_KeynumToString: eax = key, edi = translate */
    "    pushl %edx\n"
    "    pushl %ecx\n"
    "    pushl %eax\n"
    "    call _CtlGlyphName\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    popl %eax\n"
    "    popl %ecx\n"
    "    popl %edx\n"
    "    jmp *_g_ctlKeyToString\n"
    "1:  addl $4, %esp\n"
    "    popl %ecx\n"
    "    popl %edx\n"
    "    ret\n"
    ".globl _CTL_BindNamesTextHook\n"
    "_CTL_BindNamesTextHook:\n"       /* call to Key_GetBindingNames from Scaleform / GSC: text names */
    "    incl _g_ctlTextOnly\n"
    "    pushl 4(%esp)\n"
    "    call *_g_ctlBindingNames\n"
    "    addl $4, %esp\n"
    "    decl _g_ctlTextOnly\n"
    "    ret\n"
    ".globl _CTL_IconDrawHook\n"
    "_CTL_IconDrawHook:\n"            /* call to the text icon drawer: ecx = escape after '^', [esp+1Ch] = colour */
    "    pushal\n"
    "    pushl %ecx\n"
    "    call _CtlGlyphPrepare\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jz 1f\n"
    "    movl 0x3C(%esp), %eax\n"        /* packed RGBA; the shadow pass (black) stays black */
    "    testl $0x00FFFFFF, %eax\n"
    "    jz 1f\n"
    "    orl $0x00FFFFFF, %eax\n"
    "    movl %eax, 0x3C(%esp)\n"
    "1:  popal\n"
    "    jmp *_g_ctlIconDraw\n");

static void CtlInstallGlyphs(BOOL namesOk)
{
    static const DWORD kNameCalls[] = { 0x0049397Cu, 0x004A19A1u };
    static const DWORD kTextCalls[] = { 0x005B080Du, 0x0055A8B6u };
    static const DWORD kDrawCalls[] = { 0x00AAB320u, 0x00AAB92Du };
    BOOL ok = namesOk;
    for (int i = 0; i < 2; i++) {
        ok = ok && CallsTarget(kNameCalls[i], CTL_KEYNUM_TO_STRING) && *(const BYTE *)(DWORD_PTR)(kNameCalls[i] - 5) == 0xBF &&
             *(const DWORD *)(DWORD_PTR)(kNameCalls[i] - 4) == 1;   /* mov edi, 1 */
        ok = ok && CallsTarget(kTextCalls[i], CTL_BINDING_NAMES) && CallsTarget(kDrawCalls[i], CTL_ICON_DRAW);
    }
    if (!ok) {
        lstrcpynA(g_glyphWhy, "call sites differ (0x49397C / 0x4A19A1 / 0x5B080D / 0x55A8B6 / 0xAAB320 / 0xAAB92D)", sizeof(g_glyphWhy));
        g_glyphReady = -1;
        Log("controller: glyphs off: %s", g_glyphWhy);
        return;
    }
    for (int i = 0; i < 2; i++) {
        RedirectCall(kTextCalls[i], (void *)CTL_BindNamesTextHook);
        RedirectCall(kDrawCalls[i], (void *)CTL_IconDrawHook);
        RedirectCall(kNameCalls[i], (void *)CTL_KeyNameHook);
    }
    Log("controller: glyph hooks installed (names 0x49397C/0x4A19A1, text-only 0x5B080D/0x55A8B6, draw 0xAAB320/0xAAB92D)");
}

static void __cdecl CtlCmdGlyphDump(void)
{
    static const char *const kSet[] = { "Xbox 360", "PS3", "Wii U", "generic" };
    C1Printf("glyphs: %s; set %s (gpad_glyphs %d, pad family %d)%s\n", g_glyphWhy,
             g_glyphSet < 0 ? "text names" : kSet[g_glyphSet], C1DvarInt(g_gpGlyphs, 0), (int)g_padType,
             g_glyphFailed ? "; texture creation FAILED" : "");
    typedef const BYTE *(__cdecl *RegFn)(const char *, int);
    const BYTE *mat = ((RegFn)(DWORD_PTR)CTL_MATERIAL_REGISTER)(VoiceArgc() > 1 ? VoiceArgv(1) : "white", 7);
    if (!mat || !Readable(mat, 0x80)) { C1Printf("glyphs: no material\n"); return; }
    char line[200];
    for (int r = 0; r < 0x80; r += 0x20) {
        int n = _snprintf(line, sizeof(line), "mat %p +%02X:", (const void *)mat, r);
        for (int i = 0; i < 0x20; i += 4) n += _snprintf(line + n, sizeof(line) - n, " %08lX", (unsigned long)*(const DWORD *)(mat + r + i));
        C1Printf("%s\n", line);
        Log("controller: %s", line);
    }
    int count;
    const BYTE *img = CtlGlyphTemplateImage(mat, &count);
    C1Printf("glyphs: textures %d, image %p, texture %p%s\n", count, (const void *)img, img ? *(void *const *)img : NULL,
             img && CtlIsD3dObject(*(void *const *)img) ? " (D3D object)" : "");
    Log("controller: glyphdump textures %d image %p texture %p", count, (const void *)img, img ? *(void *const *)img : NULL);
    if (img) {
        int n = _snprintf(line, sizeof(line), "image:");
        for (int i = 0; i < 0x20; i += 4) n += _snprintf(line + n, sizeof(line) - n, " %08lX", (unsigned long)*(const DWORD *)(img + i));
        C1Printf("%s\n", line);
        Log("controller: %s", line);
    }
}

/* gpad_glyphtest: a test row of every button's icon, added next to a text C1 already draws each frame
 * (R_AddCmdDrawText 0xA7EF80 is watched while the test is on; "gpad_glyphtest <caller>" picks the call
 * site, default the first one seen). Tests only. */
#define CTL_ADDCMD_TEXT 0x00A7EF80u
__attribute__((used)) DWORD g_ctlAddTextResume = CTL_ADDCMD_TEXT + 5;
static volatile LONG g_glyphOverlay;
static DWORD g_overlayCaller, g_seenCallers[32];
static int g_seenCount, g_inOverlay;
void CTL_AddTextHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_AddTextHook\n"
    "_CTL_AddTextHook:\n"             /* jmp from R_AddCmdDrawText: [esp] = return, [esp+4] = text ... */
    "    pushal\n"
    "    leal 36(%esp), %eax\n"
    "    pushl %eax\n"
    "    pushl 36(%esp)\n"
    "    call _CtlAddTextSeen\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    movl 4(%esp), %eax\n"         /* the displaced mov eax, [esp+4] ; push ebx */
    "    pushl %ebx\n"
    "    jmp *_g_ctlAddTextResume\n");

typedef void(__cdecl *CtlAddTextFn)(const char *, int, void *, float, float, float, float, float, const float *, int);
__attribute__((used)) void __cdecl CtlAddTextSeen(DWORD ret, const DWORD *args)
{
    if (!g_glyphOverlay || g_inOverlay) return;
    if (ret >= 0x80000000u) {   /* a frame-stage call (see CtlCmdGlyphTest): no font from C1, register one */
        static void *font;
        if (!font) font = ((void *(__cdecl *)(const char *, int))(DWORD_PTR)0x00A78540u)("fonts/normalfont", 0);
        static DWORD args2[10];
        float one = 0.6f;
        args2[2] = (DWORD)(DWORD_PTR)font;
        memcpy(&args2[5], &one, 4);
        args = args2;
    }
    int known = 0;
    for (int i = 0; i < g_seenCount; i++) known |= g_seenCallers[i] == ret;
    if (!known && g_seenCount < 32) {
        g_seenCallers[g_seenCount++] = ret;
        Log("controller: glyphtest: R_AddCmdDrawText caller %08lX font %p scale %.2f \"%.40s\"", (unsigned long)ret,
            (void *)(DWORD_PTR)args[2], *(const float *)&args[5], Readable((const char *)(DWORD_PTR)args[0], 1) ? (const char *)(DWORD_PTR)args[0] : "");
    }
    if (!g_overlayCaller) g_overlayCaller = ret;
    if (ret != g_overlayCaller || g_glyphReady != 1) return;
    char line[600];
    int n = 0;
    for (int b = 0; b < CTL_NBUTTONS && n < (int)sizeof(line) - 40; b++)
        n += _snprintf(line + n, sizeof(line) - n, "%s%s", b ? " " : "", g_glyphs[b].esc);
    static const float white[4] = { 1, 1, 1, 1 };
    float sc = *(const float *)&args[5];
    if (sc < 0.3f) sc = 0.3f;
    g_inOverlay = 1;
    ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f, 62.0f, sc * 1.25f, sc * 1.25f, 0.0f, white, 0);
    _snprintf(line, sizeof(line), "Press %s to reload, hold %s to use, %s to aim", g_glyphs[2].esc, g_glyphs[16].esc, g_glyphs[14].esc);
    ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f, 128.0f, sc * 1.25f, sc * 1.25f, 0.0f, white, 0);
    g_inOverlay = 0;
}

/* frame-stage test calls: the Scaleform calls 0x4A07E9 / 0x4A0810 in the screen update, then the test row */
__attribute__((used)) DWORD g_ctlStageA = 0x00A7E5F0u, g_ctlStageB = 0x00A90D30u;
void CTL_StageAHook(void);
void CTL_StageBHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_StageAHook\n"
    "_CTL_StageAHook:\n"
    "    pushal\n"
    "    pushl $0\n"
    "    pushl $0x80000001\n"
    "    call _CtlAddTextSeen\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    jmp *_g_ctlStageA\n"
    ".globl _CTL_StageBHook\n"
    "_CTL_StageBHook:\n"
    "    pushl 4(%esp)\n"
    "    call *_g_ctlStageB\n"
    "    addl $4, %esp\n"
    "    pushal\n"
    "    pushl $0\n"
    "    pushl $0x80000002\n"
    "    call _CtlAddTextSeen\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    ret\n");

static void __cdecl CtlCmdGlyphTest(void)
{
    static BOOL hooked;
    if (g_glyphReady != 1) { C1Printf("glyphs: %s\n", g_glyphWhy); return; }
    if (!hooked) {
        static const BYTE kText[] = { 0x8B, 0x44, 0x24, 0x04, 0x53, 0x33, 0xDB };
        if (memcmp((void *)(DWORD_PTR)CTL_ADDCMD_TEXT, kText, sizeof(kText))) { C1Printf("glyphtest: R_AddCmdDrawText bytes differ\n"); return; }
        BYTE j[5] = { 0xE9 };
        LONG r = (LONG)((DWORD_PTR)CTL_AddTextHook - (CTL_ADDCMD_TEXT + 5));
        memcpy(j + 1, &r, 4);
        WriteCode((void *)(DWORD_PTR)CTL_ADDCMD_TEXT, j, 5);
        if (CallsTarget(0x0060AB19u, g_ctlStageA)) RedirectCall(0x0060AB19u, (void *)CTL_StageAHook);
        if (CallsTarget(0x004A0810u, g_ctlStageB)) RedirectCall(0x004A0810u, (void *)CTL_StageBHook);
        hooked = TRUE;
    }
    if (VoiceArgc() > 1) {
        g_overlayCaller = strtoul(VoiceArgv(1), NULL, 16);
        g_glyphOverlay = g_overlayCaller != 0 || strcmp(VoiceArgv(1), "0");
        if (!strcmp(VoiceArgv(1), "off")) g_glyphOverlay = 0;
    } else g_glyphOverlay = !g_glyphOverlay;
    C1Printf("glyphtest %s (caller %08lX)\n", g_glyphOverlay ? "on" : "off", (unsigned long)g_overlayCaller);
}
