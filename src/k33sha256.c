/*
 * k33sha256.c - SHA-256 of memory or of a file (CryptoAPI, advapi32), lowercase hex. Included by kernel33.c after
 * c1game.c. theatre.c names the recordings on this PC with it; the opt-in kernel migrations (kernel_room_context.c,
 * kernel_name_display.c) check the exe with it.
 *
 *   K33Sha256Hex  SHA-256 of data (path NULL) or of a file (data NULL).
 *
 * This was k33http.c. Its HTTPS client (K33HttpGet / K33HttpDownload, loopback only), used by nothing but theatre.c's
 * recording service and name_display.c's display-name service, was removed with them: this client is offline only.
 */
#include <wincrypt.h>

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
