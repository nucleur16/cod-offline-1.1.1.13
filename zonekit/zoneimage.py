# -*- coding: utf-8 -*-
r"""The inflated zone image of 1.1.1.13 (alpha0022 zones/zone_assets.py, MEASURED there on all 90 stock zones):

    0x00  u32 size (len - 0x28)    0x04  u32 x9 (IW blockSize[]; one extra dword)
    0x28  XAssetList {u32 stringCount, u32 stringsPtr, u32 assetCount, u32 assetsPtr}
    0x38  u32 stringPtr[stringCount] (0xFFFFFFFF: an inline string follows; 0: NULL), then the inline strings
          then assetCount 16-byte entries {u32 type, u32 offset (from 0x28), u32 ptr (0xFFFFFFFF), u32 size}
          then the asset blobs, tiling the rest of the image in table order.

The table is a real directory (a CODOL addition to IW's 8-byte XAsset), so every zone splits into one blob per asset
and joins back byte for byte. Inside a blob, pointers are 0xFFFFFFFF (inline data follows), 0xFFFFFFFE (an asset
reference) or ((block << 28) | offset) + 1 (a back reference into a runtime block). A zone with no back references
(every Python-built patch zone) can have any blob resized; in a zone that has them (the C#-written ones), resizing a
blob moves the data later references point at, so those are rebuilt from blobs, not edited.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

ASSET_BASE = 0x28
PTR_INLINE = 0xFFFFFFFF


@dataclass
class ZoneImage:
    blocks: list            # the 9 dwords at 0x04
    string_ptrs: list       # per script string: 0xFFFFFFFF or 0
    strings: list           # str (latin-1) or None
    string_count_field: int
    strings_ptr_field: int
    assets_ptr_field: int
    assets: list = field(default_factory=list)   # [(type, ptr_field, blob)]

    @property
    def flat(self) -> bool:
        return self.string_count_field == 0


def parse(z: bytes) -> ZoneImage:
    size = struct.unpack_from("<I", z, 0)[0]
    if size != len(z) - ASSET_BASE:
        raise ValueError("size field %#x != %#x" % (size, len(z) - ASSET_BASE))
    blocks = list(struct.unpack_from("<9I", z, 4))
    sc, sp, n, ap = struct.unpack_from("<4I", z, ASSET_BASE)
    ptrs = list(struct.unpack_from("<%dI" % sc, z, 0x38)) if sc else []
    p = 0x38 + 4 * sc
    strings = []
    for v in ptrs:
        if v == PTR_INLINE:
            e = z.index(b"\0", p)
            strings.append(z[p:e].decode("latin-1"))
            p = e + 1
        elif v == 0:
            strings.append(None)
        else:
            raise ValueError("string pointer %#x" % v)
    table = p
    img = ZoneImage(blocks, ptrs, strings, sc, sp, ap)
    end = table + 16 * n
    for i in range(n):
        t, off, ptr, sz = struct.unpack_from("<4I", z, table + 16 * i)
        if off + ASSET_BASE != end:
            raise ValueError("asset %d at %#x, expected %#x (blobs do not tile)" % (i, off + ASSET_BASE, end))
        img.assets.append((t, ptr, z[end:end + sz]))
        end += sz
    if end != len(z):
        raise ValueError("%d bytes after the last asset" % (len(z) - end))
    return img


def assemble(img: ZoneImage) -> bytes:
    body = bytearray(struct.pack("<4I", img.string_count_field, img.strings_ptr_field, len(img.assets),
                                 img.assets_ptr_field))
    body += struct.pack("<%dI" % len(img.string_ptrs), *img.string_ptrs)
    for v, s in zip(img.string_ptrs, img.strings):
        if v == PTR_INLINE:
            body += s.encode("latin-1") + b"\0"
    table = len(body)
    body += b"\0" * (16 * len(img.assets))
    for i, (t, ptr, blob) in enumerate(img.assets):
        struct.pack_into("<4I", body, table + 16 * i, t, len(body), ptr, len(blob))
        body += blob
    return struct.pack("<I9I", len(body), *img.blocks) + bytes(body)


TYPE_NAMES = [
    "physpreset", "phys_collmap", "xanim", "xmodelsurfs", "xmodel", "material", "pixelshader", "vertexshader",
    "vertexdecl", "techset", "image", "sound", "sndcurve", "loaded_sound", "col_map_sp", "col_map_mp", "com_map",
    "game_map_sp", "game_map_mp", "map_ents", "fx_map", "gfx_map", "lightdef", "ui_map", "font", "menufile", "menu",
    "localize", "weapon", "snddriverglobals", "fx", "impactfx", "aitype", "mptype", "character", "xmodelalias",
    "rawfile", "stringtable", "leaderboarddef", "structureddatadef", "tracer", "vehicle", "addon_map_ents",
    "weaponattach", "netconststrings", "marketplacedata",
]  # g_assetNames 1.1.1.13 0x00CD55B0 (CODOL-AssetExplorer Zone/XAssetType.cs)
TYPE_IDS = {n: i for i, n in enumerate(TYPE_NAMES)}


def type_name(t: int) -> str:
    return TYPE_NAMES[t] if 0 <= t < len(TYPE_NAMES) else "type_%d" % t


def type_id(name: str) -> int:
    if name in TYPE_IDS:
        return TYPE_IDS[name]
    if name.startswith("type_"):
        return int(name[5:])
    return int(name, 0)
