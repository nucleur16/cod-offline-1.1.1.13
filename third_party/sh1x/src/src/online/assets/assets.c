/* Native zone directory/rawfile loader, derived from alpha0022/zone_assets.py
 * and ffwriter/codol_ff.py. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "assets.h"
#include "zone_image.h"
#include "zlib/zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int OpenZoneImage(zone_t *out, const void *data, size_t size)
{
    zone_t z = {0}; const uint8_t *bytes = data;
    size_t pos, pointers, end; uint32_t i;
    if (!out || !bytes || size < 0x38 || size > ASSET_LIMIT || u32(bytes) != size - 0x28)
        return 0;
    z.string_count = u32(bytes + 0x28); z.asset_count = u32(bytes + 0x30);
    for (i = 0; i < 8; ++i) z.block_size[i] = u32(bytes + 8 + i * 4);
    pointers = 0x38;
    if (z.string_count > (size - pointers) / 4) return 0;
    pos = pointers + z.string_count * 4;
    for (i = 0; i < z.string_count; ++i) {
        if (u32(bytes + pointers + 4 * i) == UINT32_MAX) {
            const uint8_t *term = memchr(bytes + pos, 0, size - pos);
            if (!term) return 0;
            pos = (size_t)(term - bytes) + 1;
        }
    }
    z.table_offset = pos;
    if (z.asset_count > (size - pos) / 16) return 0;
    end = pos + z.asset_count * 16;
    for (i = 0; i < z.asset_count; ++i) {
        const uint8_t *entry = bytes + pos + i * 16;
        uint32_t offset = u32(entry + 4), extent = u32(entry + 12);
        /* Every asset extent tiles the stream, so pointers never become native
         * addresses and even unsupported asset types can be skipped safely. */
        if (offset > size - 0x28 || offset + (size_t)0x28 != end || extent > size - end)
            return 0;
        end += extent;
    }
    if (end != size) return 0;
    z.data = ZoneImageAllocate(size);
    if (!z.data) return 0;
    memcpy(z.data, data, size); z.size = size;
    *out = z; return 1;
}

int OpenZoneFile(zone_t *out, const char *path)
{
    FILE *file; long length; uint8_t *data; int ok = 0;
    if (!out || !path || !(file = fopen(path, "rb"))) return 0;
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
        (unsigned long)length > ASSET_LIMIT || fseek(file, 0, SEEK_SET)) {
        fclose(file); return 0;
    }
    data = malloc(length ? (size_t)length : 1);
    if (data && fread(data, 1, length, file) == (size_t)length)
        ok = OpenZoneImage(out, data, length);
    free(data); fclose(file); return ok;
}

void CloseZone(zone_t *zone)
{
    if (zone) { ZoneImageFree(zone->data); memset(zone, 0, sizeof(*zone)); }
}

int ZoneAsset(const zone_t *zone, unsigned index, asset_t *asset)
{
    const uint8_t *entry;
    if (!zone || !zone->data || !asset || index >= zone->asset_count) return 0;
    entry = zone->data + zone->table_offset + index * 16;
    asset->type = u32(entry); asset->size = u32(entry + 12);
    asset->data = zone->data + 0x28 + u32(entry + 4); return 1;
}

int ReadScriptStrings(const zone_t *zone, script_string_t **out)
{
    script_string_t *strings = NULL;
    size_t pos;
    unsigned i;
    if (!zone || !out || !zone->data || zone->size < 0x38 ||
        zone->table_offset > zone->size || zone->table_offset < 0x38 ||
        zone->string_count > (zone->table_offset - 0x38)/4 ||
        zone->string_count > ASSET_LIMIT/sizeof(*strings)) return 0;
    pos = 0x38 + (size_t)zone->string_count*4;
    if (zone->string_count && !(strings = calloc(zone->string_count, sizeof(*strings)))) return 0;
    for (i = 0; i < zone->string_count; ++i) {
        strings[i].reference = u32(zone->data + 0x38 + (size_t)i*4);
        if (strings[i].reference == UINT32_MAX) {
            const uint8_t *end = memchr(zone->data + pos, 0, zone->table_offset - pos);
            if (!end) goto fail;
            strings[i].text = (const char *)zone->data + pos;
            pos = (size_t)(end - zone->data) + 1;
        }
    }
    if (pos != zone->table_offset) goto fail;
    *out = strings; return 1;
fail:
    free(strings); return 0;
}

const char *RawFileName(const asset_t *asset)
{
    if (!asset || asset->type != ASSET_RAWFILE || asset->size < 17 ||
        u32(asset->data) != UINT32_MAX ||
        !memchr(asset->data + 16, 0, asset->size - 16)) return NULL;
    return (const char *)asset->data + 16;
}

static voidpf allocate(voidpf opaque, uInt count, uInt size)
{
    (void)opaque;
    if (size && count > SIZE_MAX / size) return NULL;
    return calloc(count, size);
}
static void deallocate(voidpf opaque, voidpf ptr) { (void)opaque; free(ptr); }

int OL_Inflate(const uint8_t *in, size_t input_size, uint8_t **out,
                  size_t *output_size, size_t exact_size, int padded)
{
    z_stream stream = {0}; uint8_t *buffer; size_t capacity;
    int code, ok = 0;
    if (!in || !out || !output_size || input_size > ASSET_LIMIT ||
        exact_size > ASSET_LIMIT) return 0;
    capacity = exact_size ? exact_size : 65536;
    buffer = malloc(capacity + 1);
    if (!buffer) return 0;
    stream.zalloc = allocate; stream.zfree = deallocate;
    stream.next_in = (Bytef *)in; stream.avail_in = input_size;
    if (inflateInit(&stream) != Z_OK) { free(buffer); return 0; }
    for (;;) {
        stream.next_out = buffer + stream.total_out;
        /* One spare byte detects streams claiming fewer output bytes than they produce. */
        stream.avail_out = capacity + 1 - stream.total_out;
        code = inflate(&stream, Z_NO_FLUSH);
        if (stream.total_out > capacity) {
            uint8_t *larger;
            if (exact_size || capacity >= ASSET_LIMIT) break;
            capacity *= 2;
            if (capacity > ASSET_LIMIT) capacity = ASSET_LIMIT;
            larger = realloc(buffer, capacity + 1);
            if (!larger) break;
            buffer = larger;
        }
        if (code == Z_STREAM_END) {
            if ((!exact_size || stream.total_out == exact_size) &&
                (padded || stream.avail_in == 0)) ok = 1;
            break;
        }
        if (code != Z_OK || (!stream.avail_in && stream.avail_out)) break;
    }
    if (ok) {
        buffer[stream.total_out] = 0;
        *out = buffer; *output_size = stream.total_out;
    } else free(buffer);
    inflateEnd(&stream); return ok;
}

int ReadRawFile(const asset_t *asset, uint8_t **out, size_t *size)
{
    uint32_t compressed, uncompressed; size_t start = 16;
    const char *name; uint8_t *payload; size_t decoded;
    if (!asset || asset->type != ASSET_RAWFILE || asset->size < 16 || !out || !size)
        return 0;
    compressed = u32(asset->data + 4); uncompressed = u32(asset->data + 8);
    if (compressed > ASSET_LIMIT || uncompressed > ASSET_LIMIT) return 0;
    if (u32(asset->data) == UINT32_MAX) {
        name = RawFileName(asset);
        if (!name) return 0;
        start += strlen(name) + 1;
    }
    if (start > asset->size) return 0;
    if (compressed) {
        if (compressed != asset->size - start ||
            !OL_Inflate(asset->data + start, compressed, &payload, &decoded, uncompressed, 0))
            return 0;
        if (decoded != uncompressed) { free(payload); return 0; }
    } else {
        if ((size_t)uncompressed + 1 != asset->size - start ||
            asset->data[asset->size - 1] != 0) return 0;
        payload = malloc((size_t)uncompressed + 1);
        if (!payload) return 0;
        memcpy(payload, asset->data + start, (size_t)uncompressed + 1);
    }
    *out = payload; *size = uncompressed; return 1;
}

static int same_name(const char *a, const char *b)
{
    unsigned ca, cb;
    do {
        ca = (uint8_t)*a++; cb = (uint8_t)*b++;
        if (ca == '\\') ca = '/';
        if (cb == '\\') cb = '/';
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return 0;
    } while (ca);
    return 1;
}

int FindRawFileStatus(const zone_t *zone, const char *name, uint8_t **out, size_t *size)
{
    unsigned i; asset_t asset;
    if (!zone || !name || !out || !size) return -1;
    /* The first definition wins, as C1's DB_LinkXAssetEntry links it. */
    for (i = 1; i <= zone->asset_count; ++i) {
        const char *candidate;
        if (!ZoneAsset(zone, i - 1, &asset)) return -1;
        candidate = RawFileName(&asset);
        if (candidate && same_name(candidate, name)) return ReadRawFile(&asset, out, size) ? 1 : -1;
    }
    return 0;
}

int FindRawFile(const zone_t *zone, const char *name, uint8_t **out, size_t *size)
{
    return FindRawFileStatus(zone, name, out, size) == 1;
}
