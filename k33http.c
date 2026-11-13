/*
 * k33http.c - a small HTTPS client and SHA-256 for the features ported from codol-classic's d3d9.dll (theatre.c's
 * recording service, name_display.c's optional display-name service). Included by kernel33.c after c1game.c.
 *
 * WinHTTP is loaded on first use (LoadLibrary, not an import), so a build that never calls it does not change the
 * DLL's imports and every build script links as before. Worker threads only: every call blocks (bounded timeouts).
 *
 *   K33HttpGet       GET into a malloc'd buffer (capped), optional "Authorization: Bearer", no redirects.
 *   K33HttpDownload  GET into <path>.part, exact Content-Length and SHA-256 checked, then renamed over <path>.
 *   K33Sha256Hex     SHA-256 of memory or of a file (CryptoAPI, advapi32), lowercase hex.
 * This client is offline only: a URL is accepted only when its host is 127.0.0.1 / localhost (http:// or
 * https://, a local test service). Every other host is refused here, whatever a dvar or the command line
 * names, so the optional services built on this file (theatre.c, name_display.c) cannot go online.
 */
#include <winhttp.h>
#include <wincrypt.h>

typedef HINTERNET(WINAPI *K33WinHttpOpenFn)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef HINTERNET(WINAPI *K33WinHttpConnectFn)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
typedef HINTERNET(WINAPI *K33WinHttpOpenRequestFn)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR *, DWORD);
typedef BOOL(WINAPI *K33WinHttpSendRequestFn)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL(WINAPI *K33WinHttpReceiveResponseFn)(HINTERNET, LPVOID);
typedef BOOL(WINAPI *K33WinHttpQueryHeadersFn)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
typedef BOOL(WINAPI *K33WinHttpReadDataFn)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI *K33WinHttpCloseHandleFn)(HINTERNET);
typedef BOOL(WINAPI *K33WinHttpSetTimeoutsFn)(HINTERNET, int, int, int, int);
typedef BOOL(WINAPI *K33WinHttpSetOptionFn)(HINTERNET, DWORD, LPVOID, DWORD);
typedef BOOL(WINAPI *K33WinHttpCrackUrlFn)(LPCWSTR, DWORD, DWORD, LPURL_COMPONENTS);

static struct {
    K33WinHttpOpenFn Open;
    K33WinHttpConnectFn Connect;
    K33WinHttpOpenRequestFn OpenRequest;
    K33WinHttpSendRequestFn SendRequest;
    K33WinHttpReceiveResponseFn ReceiveResponse;
    K33WinHttpQueryHeadersFn QueryHeaders;
    K33WinHttpReadDataFn ReadData;
    K33WinHttpCloseHandleFn CloseHandle;
    K33WinHttpSetTimeoutsFn SetTimeouts;
    K33WinHttpSetOptionFn SetOption;
    K33WinHttpCrackUrlFn CrackUrl;
} g_wh;
static volatile LONG g_whState; /* 0 not loaded, 1 ok, -1 unavailable */

static BOOL K33HttpLoad(void)
{
    if (g_whState) return g_whState > 0;
    HMODULE m = LoadLibraryA("winhttp.dll");
    if (m) {
        g_wh.Open = (K33WinHttpOpenFn)GetProcAddress(m, "WinHttpOpen");
        g_wh.Connect = (K33WinHttpConnectFn)GetProcAddress(m, "WinHttpConnect");
        g_wh.OpenRequest = (K33WinHttpOpenRequestFn)GetProcAddress(m, "WinHttpOpenRequest");
        g_wh.SendRequest = (K33WinHttpSendRequestFn)GetProcAddress(m, "WinHttpSendRequest");
        g_wh.ReceiveResponse = (K33WinHttpReceiveResponseFn)GetProcAddress(m, "WinHttpReceiveResponse");
        g_wh.QueryHeaders = (K33WinHttpQueryHeadersFn)GetProcAddress(m, "WinHttpQueryHeaders");
        g_wh.ReadData = (K33WinHttpReadDataFn)GetProcAddress(m, "WinHttpReadData");
        g_wh.CloseHandle = (K33WinHttpCloseHandleFn)GetProcAddress(m, "WinHttpCloseHandle");
        g_wh.SetTimeouts = (K33WinHttpSetTimeoutsFn)GetProcAddress(m, "WinHttpSetTimeouts");
        g_wh.SetOption = (K33WinHttpSetOptionFn)GetProcAddress(m, "WinHttpSetOption");
        g_wh.CrackUrl = (K33WinHttpCrackUrlFn)GetProcAddress(m, "WinHttpCrackUrl");
    }
    BOOL ok = m && g_wh.Open && g_wh.Connect && g_wh.OpenRequest && g_wh.SendRequest && g_wh.ReceiveResponse &&
              g_wh.QueryHeaders && g_wh.ReadData && g_wh.CloseHandle && g_wh.SetTimeouts && g_wh.SetOption &&
              g_wh.CrackUrl;
    InterlockedExchange(&g_whState, ok ? 1 : -1);
    if (!ok) Log("http: winhttp.dll unavailable");
    return ok;
}

typedef struct K33HttpReq {
    HINTERNET session, connection, request;
} K33HttpReq;

static void K33HttpClose(K33HttpReq *r)
{
    if (r->request) g_wh.CloseHandle(r->request);
    if (r->connection) g_wh.CloseHandle(r->connection);
    if (r->session) g_wh.CloseHandle(r->session);
    memset(r, 0, sizeof(*r));
}

/* Opens and sends a GET; *status gets the HTTP status. FALSE on any transport failure or a refused URL. */
static BOOL K33HttpSend(K33HttpReq *r, const char *url, const char *bearer, int timeoutMs, int *status)
{
    memset(r, 0, sizeof(*r));
    *status = 0;
    if (!url || !K33HttpLoad()) return FALSE;
    WCHAR wurl[2048], host[256], path[2048];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url, -1, wurl, 2048)) return FALSE;
    URL_COMPONENTS u;
    memset(&u, 0, sizeof(u));
    u.dwStructSize = sizeof(u);
    u.lpszHostName = host;
    u.dwHostNameLength = 256;
    u.lpszUrlPath = path;
    u.dwUrlPathLength = 2048;
    WCHAR extra[2048];
    u.lpszExtraInfo = extra;
    u.dwExtraInfoLength = 2048;
    if (!g_wh.CrackUrl(wurl, 0, 0, &u)) return FALSE;
    BOOL https = u.nScheme == INTERNET_SCHEME_HTTPS;
    BOOL local = !lstrcmpiW(host, L"127.0.0.1") || !lstrcmpiW(host, L"localhost");
    if (!local || !(https || u.nScheme == INTERNET_SCHEME_HTTP)) return FALSE; /* loopback only: offline client */
    if (lstrlenW(path) + lstrlenW(extra) >= 2047) return FALSE;
    lstrcatW(path, extra);

    r->session = g_wh.Open(L"Kernel33/1.0", local ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!r->session) return FALSE;
    g_wh.SetTimeouts(r->session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    r->connection = g_wh.Connect(r->session, host, u.nPort, 0);
    if (!r->connection) { K33HttpClose(r); return FALSE; }
    r->request = g_wh.OpenRequest(r->connection, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  https ? WINHTTP_FLAG_SECURE : 0);
    if (!r->request) { K33HttpClose(r); return FALSE; }
    DWORD never = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    g_wh.SetOption(r->request, WINHTTP_OPTION_REDIRECT_POLICY, &never, sizeof(never));
    WCHAR header[600];
    header[0] = 0;
    if (bearer && *bearer) {
        WCHAR wtok[512];
        if (strlen(bearer) >= 500 || !MultiByteToWideChar(CP_UTF8, 0, bearer, -1, wtok, 512)) { K33HttpClose(r); return FALSE; }
        lstrcpyW(header, L"Authorization: Bearer ");
        lstrcatW(header, wtok);
        SecureZeroMemory(wtok, sizeof(wtok));
    }
    BOOL sent = g_wh.SendRequest(r->request, header[0] ? header : WINHTTP_NO_ADDITIONAL_HEADERS, header[0] ? (DWORD)-1L : 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                g_wh.ReceiveResponse(r->request, NULL);
    SecureZeroMemory(header, sizeof(header));
    if (!sent) { K33HttpClose(r); return FALSE; }
    DWORD code = 0, len = sizeof(code);
    if (!g_wh.QueryHeaders(r->request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                           &code, &len, WINHTTP_NO_HEADER_INDEX)) { K33HttpClose(r); return FALSE; }
    *status = (int)code;
    return TRUE;
}

/* GET url; on success *body (malloc'd, NUL-terminated, caller frees) and *length. The body is read for any status. */
__attribute__((unused)) static BOOL K33HttpGet(const char *url, const char *bearer, int timeoutMs, DWORD cap, char **body, DWORD *length,
                       int *status)
{
    K33HttpReq r;
    *body = NULL;
    *length = 0;
    if (!K33HttpSend(&r, url, bearer, timeoutMs, status)) return FALSE;
    char *buf = (char *)malloc(cap + 1);
    DWORD used = 0;
    BOOL ok = buf != NULL;
    DWORD started = GetTickCount();
    while (ok) {
        DWORD got = 0;
        if (used >= cap) { ok = FALSE; break; }
        if (!g_wh.ReadData(r.request, buf + used, cap - used, &got)) { ok = FALSE; break; }
        if (!got) break;
        used += got;
        if (GetTickCount() - started > (DWORD)timeoutMs * 4) ok = FALSE;
    }
    K33HttpClose(&r);
    if (!ok) { free(buf); return FALSE; }
    buf[used] = 0;
    *body = buf;
    *length = used;
    return TRUE;
}

static void K33HexOf(const BYTE *digest, char hex[65])
{
    static const char kHex[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[i * 2] = kHex[digest[i] >> 4];
        hex[i * 2 + 1] = kHex[digest[i] & 15];
    }
    hex[64] = 0;
}

/* SHA-256 of data (path NULL) or of a file (data NULL). */
static BOOL K33Sha256Hex(const void *data, DWORD size, const WCHAR *path, char hex[65])
{
    HCRYPTPROV prov = 0;
    HCRYPTHASH h = 0;
    BOOL ok = CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
              CryptCreateHash(prov, CALG_SHA_256, 0, 0, &h);
    if (ok && path) {
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                               OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        ok = f != INVALID_HANDLE_VALUE;
        BYTE *b = (BYTE *)malloc(65536);
        if (!b) ok = FALSE;
        while (ok) {
            DWORD got = 0;
            if (!ReadFile(f, b, 65536, &got, NULL)) { ok = FALSE; break; }
            if (!got) break;
            ok = CryptHashData(h, b, got, 0);
        }
        free(b);
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    } else if (ok) {
        ok = CryptHashData(h, (const BYTE *)data, size, 0);
    }
    BYTE digest[32];
    DWORD dl = sizeof(digest);
    if (ok) ok = CryptGetHashParam(h, HP_HASHVAL, digest, &dl, 0) && dl == 32;
    if (h) CryptDestroyHash(h);
    if (prov) CryptReleaseContext(prov, 0);
    if (ok) K33HexOf(digest, hex);
    else hex[0] = 0;
    return ok;
}

/* GET url into path (via path.part): exactly `size` bytes whose SHA-256 is sha256hex; no bearer is sent. */
__attribute__((unused)) static BOOL K33HttpDownload(const char *url, const WCHAR *path, const char *sha256hex, ULONGLONG size)
{
    K33HttpReq r;
    int status = 0;
    if (!size || size > 512ULL * 1024 * 1024 || !sha256hex || strlen(sha256hex) != 64) return FALSE;
    if (!K33HttpSend(&r, url, NULL, 30000, &status)) return FALSE;
    WCHAR part[MAX_PATH + 8];
    BOOL ok = status == 200 && lstrlenW(path) < MAX_PATH;
    if (ok) {
        lstrcpyW(part, path);
        lstrcatW(part, L".part");
    }
    HANDLE f = ok ? CreateFileW(part, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                  : INVALID_HANDLE_VALUE;
    ok = ok && f != INVALID_HANDLE_VALUE;
    ULONGLONG done = 0;
    BYTE *b = ok ? (BYTE *)malloc(65536) : NULL;
    if (!b) ok = FALSE;
    while (ok) {
        DWORD got = 0, wrote = 0;
        if (!g_wh.ReadData(r.request, b, 65536, &got)) { ok = FALSE; break; }
        if (!got) break;
        if (got > size - done || !WriteFile(f, b, got, &wrote, NULL) || wrote != got) { ok = FALSE; break; }
        done += got;
    }
    free(b);
    K33HttpClose(&r);
    if (f != INVALID_HANDLE_VALUE && !CloseHandle(f)) ok = FALSE;
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    char hex[65];
    if (ok) ok = done == size && K33Sha256Hex(NULL, 0, part, hex) && !strcmp(hex, sha256hex);
    if (ok) ok = MoveFileExW(part, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(part);
    return ok;
}
