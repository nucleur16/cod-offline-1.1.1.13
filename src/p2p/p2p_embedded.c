/*
 * p2p_embedded.c - the sh1x match server carried inside Kernel33.dll (included by p2p_inproc.c).
 *
 * Kernel33.dll holds the server repo's host-dll output (sh1x_host.dll, stripped) as RCDATA 2, zlib-packed by
 * scripts/pack_server.py (header: magic "K33S", format 1, size, packed size, SHA-256, server commit). The client
 * needs no separate server DLL: every hosted session inflates the image, checks its SHA-256 and writes it to the
 * session's private copy (%TEMP%\k33_sh1x_host_<pid>_<n>.dll), which p2p_inproc.c loads exactly as it loaded the
 * copy of p2p\sh1x_host.dll before.
 *
 * Why a loaded image and not the server's objects linked into Kernel33.dll (docs/P2P_HOSTING.md, "The server
 * inside Kernel33.dll"): the server's globals cannot be re-initialised once a session ended (server
 * win32/sys_host.c: one server per DLL instance), and every offline match is a new session with a fresh image;
 * its .bss is 227 MB, which a static link would add to Kernel33.dll's image in every client for the whole run,
 * hosting or not, in a 32-bit address space with about 400 MB of headroom in a hosted match; and the teardown
 * (p2p_inproc_retire.c) unloads the image and tracks its allocations through the image's own import table.
 *
 * While the copy is written and loaded, a read handle that refuses writers is held on it, and the bytes read back
 * through that handle must equal the checked image: nothing can swap the file between the check and the load.
 *
 * p2p_host.ini dll=<path> still loads a server DLL file instead (a server developer's build). A Kernel33.dll built
 * without the server (K33_EMBED_SERVER=0, or no server build) falls back to p2p\sh1x_host.dll.
 */

#define PE_MAGIC  0x5333334Bu /* "K33S" */
#define PE_FORMAT 1u
#define PE_MAX_IMAGE (256u << 20)

typedef struct {
    DWORD magic, format, size, packed;
    BYTE sha256[32];
    char commit[40];
} PeHeader;

/* The embedded server's header, checked for shape; NULL when this Kernel33.dll carries none. */
static const PeHeader *PeEmbedded(void)
{
    HRSRC res = g_self ? FindResourceA(g_self, MAKEINTRESOURCEA(2), (LPCSTR)RT_RCDATA) : NULL;
    HGLOBAL mem = res ? LoadResource(g_self, res) : NULL;
    const PeHeader *h = mem ? (const PeHeader *)LockResource(mem) : NULL;
    DWORD len = res ? SizeofResource(g_self, res) : 0;
    if (!h || len < sizeof(*h)) return NULL;
    if (h->magic != PE_MAGIC || h->format != PE_FORMAT || !h->size || h->size > PE_MAX_IMAGE ||
        !h->packed || h->packed > len - sizeof(*h))
        return NULL;
    return h;
}

static void PeCommit(const PeHeader *h, char out[41])
{
    memcpy(out, h->commit, 40);
    out[40] = 0;
    for (int i = 0; i < 40; i++) if (out[i] && (out[i] < '!' || out[i] > '~')) out[i] = '?';
}

/* Which server a session loads: 1 the one inside Kernel33.dll; 2 the DLL file in out (dll= in p2p_host.ini, or
 * p2p\sh1x_host.dll when this build carries no server); 0 none. */
static int PiServerSource(const struct P2PConfig *c, char out[MAX_PATH])
{
    out[0] = 0;
    if (c->dll[0]) {
        lstrcpynA(out, c->dll, MAX_PATH);
        return GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES ? 0 : 2;
    }
    if (PeEmbedded()) return 1;
    _snprintf(out, MAX_PATH, "%s\\sh1x_host.dll", c->dir);
    out[MAX_PATH - 1] = 0;
    return GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES ? 0 : 2;
}

/* Writes the embedded server to path. Returns a handle on the written file that refuses writers, to be closed
 * once LoadLibrary has mapped it, or INVALID_HANDLE_VALUE (why says what failed; nothing is left at path). */
static HANDLE PiEmbeddedWrite(const char *path, char *why, size_t whyLen)
{
    const PeHeader *h = PeEmbedded();
    BYTE *image = NULL, *back = NULL;
    HANDLE f = INVALID_HANDLE_VALUE;
    char want[65], got[65];
    DWORD done = 0;
    lstrcpynA(why, "", (int)whyLen);
    if (!h) { lstrcpynA(why, "no embedded server", (int)whyLen); return INVALID_HANDLE_VALUE; }
    image = (BYTE *)VirtualAlloc(NULL, h->size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    back = (BYTE *)VirtualAlloc(NULL, h->size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!image || !back) { lstrcpynA(why, "out of memory", (int)whyLen); goto fail; }
    uLongf n = h->size;
    if (uncompress(image, &n, (const Bytef *)(h + 1), h->packed) != Z_OK || n != h->size) {
        lstrcpynA(why, "the embedded server is corrupt (inflate)", (int)whyLen);
        goto fail;
    }
    K33HexOf(h->sha256, want);
    if (!K33Sha256Hex(image, h->size, NULL, got) || strcmp(want, got)) {
        lstrcpynA(why, "the embedded server is corrupt (SHA-256)", (int)whyLen);
        goto fail;
    }
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { _snprintf(why, whyLen, "cannot create the session copy (%lu)", GetLastError()); goto fail; }
    BOOL wrote = WriteFile(f, image, h->size, &done, NULL) && done == h->size;
    wrote = CloseHandle(f) && wrote;
    f = INVALID_HANDLE_VALUE;
    if (!wrote) { lstrcpynA(why, "cannot write the session copy", (int)whyLen); goto fail_file; }
    /* Read access only, shared only with readers: the loader can map it, nobody can rewrite it until it is mapped. */
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { _snprintf(why, whyLen, "cannot reopen the session copy (%lu)", GetLastError()); goto fail_file; }
    if (GetFileSize(f, NULL) != h->size || !ReadFile(f, back, h->size, &done, NULL) || done != h->size ||
        memcmp(back, image, h->size)) {
        lstrcpynA(why, "the session copy does not read back as written", (int)whyLen);
        CloseHandle(f);
        f = INVALID_HANDLE_VALUE;
        goto fail_file;
    }
    VirtualFree(image, 0, MEM_RELEASE);
    VirtualFree(back, 0, MEM_RELEASE);
    return f;
fail_file:
    DeleteFileA(path);
fail:
    if (image) VirtualFree(image, 0, MEM_RELEASE);
    if (back) VirtualFree(back, 0, MEM_RELEASE);
    return INVALID_HANDLE_VALUE;
}

/* For the session's log line: what the embedded server is. */
static void PiEmbeddedDescribe(char *out, size_t len)
{
    const PeHeader *h = PeEmbedded();
    char commit[41], hex[65];
    if (!h) { lstrcpynA(out, "none", (int)len); return; }
    PeCommit(h, commit);
    K33HexOf(h->sha256, hex);
    _snprintf(out, len, "sh1x %s, %lu KB, sha256 %.16s", commit[0] ? commit : "unknown", (unsigned long)(h->size >> 10), hex);
    out[len - 1] = 0;
}
