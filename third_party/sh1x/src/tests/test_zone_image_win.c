/* Windows regression test for freeZoneImages (src/online/assets/zone_image.c).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The live server on 2026-10-27 lost every rawfile and string-table name in the middle of a reused map's fourth
 * match. Its replayed blocks were views of delete-on-close temporary files whose handles were closed right after
 * mapping, which deletes the file: once memory pressure evicted a written-out page it read back as zeros, and the
 * zone images, rebuilt on touch from those blocks, lost their names. The images are now read-only temporary-file
 * views of their own, and every view keeps its file handle. This test pins the properties that make that failure
 * impossible:
 *   - an image's bytes after release, trims and working-set purges are exactly the bytes it was released with;
 *   - they do not depend on the replayed blocks (scribbling every block after the release changes nothing);
 *   - the view is read-only after release (a write would fault at the writer);
 *   - the integrity check sees no difference, and finds one when a page really changes;
 *   - the temporary file behind a view exists for as long as the view does (the live failure: its handle was closed
 *     at once, which deleted the file, and pages the system evicted later read back as zeros);
 *   - freeing an image unmaps it and deletes its file.
 * Build: make test-zone-image-win (mingw), run on Windows. */
#include <windows.h>
#define PSAPI_VERSION 2
#include <psapi.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zone_image.h"

/* temporary files with this module's prefix */
static unsigned temp_files(const char *prefix)
{
    char dir[MAX_PATH], pat[MAX_PATH]; WIN32_FIND_DATAA fd; HANDLE h; unsigned n = 0;
    GetTempPathA(sizeof(dir), dir);
    snprintf(pat, sizeof(pat), "%s%s*.tmp", dir, prefix);
    if ((h = FindFirstFileA(pat, &fd)) == INVALID_HANDLE_VALUE) return 0;
    do ++n; while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}
static uint8_t pattern(size_t i) { return (uint8_t)((i * 2654435761u) >> 13); }

int main(void)
{
    const size_t size = 3u * 1024 * 1024 + 123; /* not a page multiple */
    zone_t zone; load_blocks_t blocks; uint8_t *data, *block; size_t i; int round;
    MEMORY_BASIC_INFORMATION mbi; unsigned written = 1, drifted = 1; zone_image_stats_t stats;

    unsigned images0 = temp_files("szi"), blocks0 = temp_files("sbk");
    SetFileBackedImages(1);
    SetFileBackedBlocks(1);
    data = ZoneImageAllocate(size);
    assert(data);
    assert(temp_files("szi") == images0 + 1); /* the view's file is kept */
    for (i = 0; i < size; ++i) data[i] = pattern(i);
    memset(&zone, 0, sizeof(zone)); zone.data = data; zone.size = size;
    memset(&blocks, 0, sizeof(blocks));
    block = BlockAllocate(2u * 1024 * 1024);
    assert(block);
    assert(temp_files("sbk") == blocks0 + 1);
    memset(block, 0x5a, 2u * 1024 * 1024);
    blocks.data[1] = block; blocks.capacity[1] = 2u * 1024 * 1024;

    assert(ZoneImageRelease(&zone, &blocks) == 1);
    /* read-only from the release */
    assert(VirtualQuery(data, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READONLY);
    /* the replayed blocks change after the release: the image must not */
    memset(block, 0xa5, 2u * 1024 * 1024);
    for (round = 0; round < 4; ++round) {
        ZoneImageTrim();
        EmptyWorkingSet(GetCurrentProcess());
        for (i = 0; i < size; ++i) if (data[i] != pattern(i)) { printf("byte %zu differs after trim %d\n", i, round); return 1; }
    }
    ZoneImageCheck(&written, &drifted);
    assert(written == 0 && drifted == 0);
    ZoneImageStats(&stats);
    assert(stats.regions == 1 && stats.released == size && stats.modified_pages == 0);

    /* the check finds a page that really changed (made writable on purpose here) */
    {
        DWORD old; const zone_image_event_t *ev = NULL;
        assert(VirtualProtect(data + 8192, 4096, PAGE_READWRITE, &old));
        data[8192 + 7] ^= 0xff;
        ZoneImageCheck(&written, NULL);
        assert(written == 1 && ZoneImageEvents(&ev) >= 1 && ev[0].offset == 8192 && ev[0].kind == ZONE_IMAGE_WRITTEN);
        data[8192 + 7] ^= 0xff;
        ZoneImageCheck(&written, NULL);
        assert(written == 0);
    }

    /* a plain allocation (switch off) is not released */
    SetFileBackedImages(0);
    {
        zone_t plain = {0};
        plain.data = ZoneImageAllocate(1u << 20); plain.size = 1u << 20;
        assert(plain.data && ZoneImageRelease(&plain, &blocks) == 0);
        ZoneImageFree(plain.data);
    }

    assert(temp_files("szi") == images0 + 1); /* still there after release, trims and checks */
    ZoneImageFree(data);
    assert(VirtualQuery(data, &mbi, sizeof(mbi)) && mbi.State == MEM_FREE);
    assert(temp_files("szi") == images0); /* deleted with the view */
    ZoneImageStats(&stats);
    assert(stats.regions == 0 && stats.released == 0);
    BlockFree(block);
    assert(temp_files("sbk") == blocks0);
    printf("test_zone_image_win: ok\n");
    return 0;
}
