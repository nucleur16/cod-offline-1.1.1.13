/* Standalone archive verification using the same loader compiled into the server.
 * Prints metadata/digests, never key material or asset contents. */
#include "assets.h"
#include "mbedtls/sha256.h"
#include "zlib/zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The bundled zlib normally delegates these to engine memory. This standalone
 * inspector has no engine zone heap. OL_Inflate supplies its own allocator. */
voidpf zcalloc(voidpf unused, unsigned count, unsigned size)
{
    (void)unused;
    return size && count > SIZE_MAX / size ? NULL : calloc(count, size);
}
void zcfree(voidpf unused, voidpf ptr) { (void)unused; free(ptr); }

int main(int argc, char **argv)
{
    zone_t zone = {0}; unsigned i, j; asset_t asset;
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: sh1x_archive <inflated.zone> OR <archive.ff> <C1.exe>\n"); return 2;
    }
    if (!(argc == 3 ? OpenFastFile(&zone, argv[1], argv[2]) : OpenZoneFile(&zone, argv[1]))) {
        fprintf(stderr, "Archive failed validation\n"); return 1;
    }
    printf("zone %zu %u %u\n", zone.size, zone.asset_count, zone.string_count);
    {
        script_string_t *strings;
        if (!ReadScriptStrings(&zone, &strings)) { CloseZone(&zone); return 1; }
        for (i = 0; i < zone.string_count; ++i) {
            uint8_t digest[32];
            if (!strings[i].text) {
                printf("string %u %u unresolved\n", i, strings[i].reference);
                continue;
            }
            if (mbedtls_sha256_ret((const uint8_t *)strings[i].text, strlen(strings[i].text), digest, 0)) {
                free(strings); CloseZone(&zone); return 1;
            }
            printf("string %u %u ", i, strings[i].reference);
            for (j = 0; j < sizeof(digest); ++j) printf("%02x", digest[j]);
            putchar('\n');
        }
        free(strings);
    }
    for (i = 0; i < zone.asset_count; ++i) {
        uint8_t digest[32], *payload = NULL; size_t length;
        if (!ZoneAsset(&zone, i, &asset)) { CloseZone(&zone); return 1; }
        length = asset.size;
        if (asset.type == ASSET_RAWFILE && !ReadRawFile(&asset, &payload, &length)) {
            fprintf(stderr, "Rawfile %u failed validation\n", i); CloseZone(&zone); return 1;
        }
        if (mbedtls_sha256_ret(payload ? payload : asset.data, length, digest, 0)) {
            free(payload); CloseZone(&zone); return 1;
        }
        printf("asset %u %u %zu %zu ", i, asset.type, asset.size, length);
        for (j = 0; j < sizeof(digest); ++j) printf("%02x", digest[j]);
        putchar('\n'); free(payload);
    }
    CloseZone(&zone); return 0;
}
