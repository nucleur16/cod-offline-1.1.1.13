# -*- coding: utf-8 -*-
r"""Asset codecs: blob <-> editable file(s).

Every codec is checked when a zone is decompiled: the file it writes must encode back to the exact blob, or the asset
is kept as a raw blob instead (decompile.py). So an encode() here only has to produce the layout the decoder accepted.

Layouts (all MEASURED on the shipped zones; a pointer of 0xFFFFFFFF means the data follows inline):
  rawfile (36)     {u32 namePtr, i32 compressedLen, i32 len, u32 bufferPtr} name\0, then compressedLen zlib bytes or len
                   raw bytes; a raw payload is followed by a NUL, a zlib one by nothing (build_ff / build_font_ff, and C1's
                   own zones), with exceptions recorded as term=
  localize (27)    {u32 valuePtr, u32 namePtr} value\0 name\0
  stringtable (37) {u32 namePtr, u32 columns, u32 rows, u32 cellsPtr} name\0, cells {u32 stringPtr, u32 hash}[rows *
                   columns], then each inline cell string\0 in cell order. hash = C1's cell hash
                   (h = tolower(c) + 31 * h, tools/codol_stringtable_dump.py)
"""
from __future__ import annotations

import csv
import io
import struct
import zlib

INLINE = 0xFFFFFFFF
ENCODINGS = ("ascii", "gbk", "utf-8", "latin-1")


class NotDecodable(Exception):
    pass


def text_encoding(chunks):
    """The first encoding every byte string round-trips through."""
    for enc in ENCODINGS:
        try:
            if all(c.decode(enc).encode(enc) == c for c in chunks):
                return enc
        except (UnicodeDecodeError, UnicodeEncodeError):
            continue
    return "latin-1"


# --- rawfile ------------------------------------------------------------------------------------------------------
def decode_rawfile(blob):
    """-> (name, data, params)."""
    name_ptr, clen, ulen, buf_ptr = struct.unpack_from("<IiiI", blob, 0)
    if name_ptr != INLINE or buf_ptr != INLINE:
        raise NotDecodable("rawfile with a back-referenced name or buffer")
    e = blob.index(b"\0", 16)
    name = blob[16:e].decode("latin-1")
    payload = blob[e + 1:]
    params = {}
    if clen:
        z = payload[:clen]
        rest = payload[clen:]
        data = zlib.decompress(z)
        if len(data) != ulen:
            raise NotDecodable("%s: inflates to %d, header says %d" % (name, len(data), ulen))
        for level in (9, 6, 1, 2, 3, 4, 5, 7, 8, 0):
            if zlib.compress(data, level) == z:
                params["zlib"] = str(level)
                break
        else:
            raise NotDecodable("%s: zlib stream not reproducible" % name)
        if rest:
            params["tail"] = rest.hex()
    else:
        data = payload[:ulen]
        rest = payload[ulen:]
        if rest != b"\0":
            params["tail"] = rest.hex()
            if rest == b"":
                params["tail"] = "-"
    return name, data, params


def encode_rawfile(name, data, params):
    nm = name.encode("latin-1") + b"\0"
    tail = params.get("tail")
    if "zlib" in params:
        z = zlib.compress(data, int(params["zlib"]))
        out = struct.pack("<IiiI", INLINE, len(z), len(data), INLINE) + nm + z
        return out + (bytes.fromhex(tail) if tail and tail != "-" else b"")
    out = struct.pack("<IiiI", INLINE, 0, len(data), INLINE) + nm + data
    if tail is None:
        return out + b"\0"
    return out + (b"" if tail == "-" else bytes.fromhex(tail))


# --- localize -----------------------------------------------------------------------------------------------------
def decode_localize(blob):
    vp, np_ = struct.unpack_from("<II", blob, 0)
    if vp != INLINE or np_ != INLINE:
        raise NotDecodable("localize with back-referenced strings")
    e1 = blob.index(b"\0", 8)
    e2 = blob.index(b"\0", e1 + 1)
    if e2 + 1 != len(blob):
        raise NotDecodable("localize blob has trailing bytes")
    return blob[e1 + 1:e2].decode("latin-1"), blob[8:e1]


def encode_localize(name, value: bytes):
    return struct.pack("<II", INLINE, INLINE) + value + b"\0" + name.encode("latin-1") + b"\0"


def _esc(s):
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")


def _unesc(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            out.append({"n": "\n", "r": "\r", "t": "\t", "\\": "\\"}.get(n, "\\" + n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def write_str(entries, enc):
    """entries [(name, value bytes)] -> text of a .str-like table: NAME<TAB>value, escapes \\n \\r \\t \\\\."""
    lines = ["# localize entries in zone order: NAME<TAB>value (\\n \\r \\t \\\\ escaped); values are %s in the zone" % enc]
    for name, value in entries:
        lines.append("%s\t%s" % (name, _esc(value.decode(enc))))
    return "\n".join(lines) + "\n"


def read_str(text, enc):
    out = []
    for line in text.split("\n"):
        if not line or line.startswith("#"):
            continue
        name, _, value = line.partition("\t")
        out.append((name, _unesc(value).encode(enc)))
    return out


# --- stringtable --------------------------------------------------------------------------------------------------
def st_hash(data: bytes):
    h = 0
    for c in data:
        if 0x41 <= c <= 0x5A:
            c += 0x20
        h = (c + 31 * h) & 0xFFFFFFFF
    return h


def decode_stringtable(blob):
    """-> (name, rows [[bytes]], params)."""
    name_ptr, cols, rows, cells_ptr = struct.unpack_from("<4I", blob, 0)
    if name_ptr != INLINE:
        raise NotDecodable("stringtable with a back-referenced name")
    e = blob.index(b"\0", 16)
    name = blob[16:e].decode("latin-1")
    at = e + 1
    n = cols * rows
    params = {"columns": str(cols)}
    if n == 0:
        if at != len(blob) or cells_ptr not in (0, INLINE):
            raise NotDecodable("empty stringtable with data")
        params["cellsptr"] = "%#x" % cells_ptr
        return name, [], params
    if cells_ptr != INLINE:
        raise NotDecodable("cells not inline")
    cur = at + 8 * n
    null_cells = 0
    out = []
    for r in range(rows):
        row = []
        for c in range(cols):
            p, h = struct.unpack_from("<2I", blob, at + 8 * (r * cols + c))
            if p == INLINE:
                end = blob.index(b"\0", cur)
                s = blob[cur:end]
                cur = end + 1
            elif p == 0:
                s = b""
                null_cells += 1
            else:
                raise NotDecodable("back-referenced cell")
            if h != st_hash(s):
                raise NotDecodable("cell hash is not C1's")
            row.append(s)
        out.append(row)
    if cur != len(blob):
        raise NotDecodable("stringtable has trailing bytes")
    if null_cells:
        params["emptyptr"] = "null"
    return name, out, params


def encode_stringtable(name, rows, params):
    cols = int(params["columns"])
    if any(len(r) != cols for r in rows):
        raise ValueError("%s: every row must have %d cells" % (name, cols))
    head = name.encode("latin-1") + b"\0"
    if not rows:
        return struct.pack("<4I", INLINE, cols, 0, int(params.get("cellsptr", "0"), 0)) + head
    null_empty = params.get("emptyptr") == "null"
    cells, strings = bytearray(), bytearray()
    for row in rows:
        for s in row:
            if null_empty and s == b"":
                cells += struct.pack("<2I", 0, 0)
            else:
                cells += struct.pack("<2I", INLINE, st_hash(s))
                strings += s + b"\0"
    return struct.pack("<4I", INLINE, cols, len(rows), INLINE) + head + bytes(cells) + bytes(strings)


def write_csv(rows, enc):
    buf = io.StringIO(newline="")
    w = csv.writer(buf, lineterminator="\n")
    for row in rows:
        w.writerow([c.decode(enc) for c in row])
    return buf.getvalue()


def read_csv(text, enc):
    return [[c.encode(enc) for c in row] for row in csv.reader(io.StringIO(text, newline=""))]


# --- names of other assets (file naming only) ---------------------------------------------------------------------
def guess_name(t, blob):
    """The asset's name when it is the first inline string (most types), else None. ',name' is a reference stub."""
    if len(blob) < 4:
        return None
    if t == 10:     # GfxImage: name pointer at +0x20 (CODOL-AssetExplorer PapCommands.NameOf)
        off = 0x20
    else:
        off = 0
    if len(blob) < off + 4 or struct.unpack_from("<I", blob, off)[0] != INLINE:
        return None
    # the name is the first inline string after the fixed part; find the first printable NUL-terminated run
    i = 4
    best = None
    lim = min(len(blob), 0x400)
    while i < lim:
        c = blob[i]
        if 0x21 <= c < 0x7F:
            j = i
            while j < len(blob) and 0x20 <= blob[j] < 0x7F:
                j += 1
            if j < len(blob) and blob[j] == 0 and j - i >= 2:
                best = blob[i:j].decode("latin-1")
                break
            i = j + 1
        else:
            i += 1
    return best


# --- image (10) with its pixels in the zone --------------------------------------------------------------------------
# GfxImage {u32 texture (0xFFFFFFFF: the GfxImageLoadDef follows the name), u8 mapType, u8 semantic, u8 category, u8,
#           u16 picmip, u16 noPicmip, u32 cardMemory[2], u16 width, u16 height, u16 depth, u16, u32, u32 namePtr}
# then name\0, then GfxImageLoadDef {u32 levelCount, u32 flags, u32 format, u32 resourceSize, data[resourceSize]}.
# Only uncompressed 2D A8R8G8B8 (D3DFMT 21; bytes B G R A) is decoded: the PNG is level 0, <name>.mips.png the smaller
# levels side by side (delete it to have the build make them with a 2x2 box filter). Streamed images (resourceSize 0,
# pixels in an IWD) stay blobs.
D3DFMT_A8R8G8B8 = 21


def decode_image(blob):
    """-> (name, [level bytes (BGRA)], params)."""
    if len(blob) < 0x24 or blob[:4] != b"\xff\xff\xff\xff" or blob[0x20:0x24] != b"\xff\xff\xff\xff":
        raise NotDecodable("image without an inline name and load definition")
    map_type = blob[4]
    w, h, d = struct.unpack_from("<HHH", blob, 0x14)
    e = blob.index(b"\0", 0x24)
    name = blob[0x24:e].decode("latin-1")
    levels, flags, fmt, size = struct.unpack_from("<IIII", blob, e + 1)
    data = blob[e + 17:]
    if map_type != 3 or d != 1 or fmt != D3DFMT_A8R8G8B8 or size == 0 or len(data) != size or name.startswith(","):
        raise NotDecodable("not an uncompressed 2D image with its pixels in the zone")
    out, at = [], 0
    for lv in range(levels):
        n = max(w >> lv, 1) * max(h >> lv, 1) * 4
        out.append(data[at:at + n])
        at += n
    if at != size:
        raise NotDecodable("image levels do not add up to its data")
    return name, out, {"hdr": blob[4:0x20].hex(), "levels": str(levels), "lflags": "%#x" % flags}


def box_mip(rgba, w, h):
    from PIL import Image
    im = Image.frombytes("RGBA", (w, h), rgba)
    return im.resize((max(w // 2, 1), max(h // 2, 1)), Image.BOX).tobytes()


def encode_image(name, levels_bgra, w, h, params):
    hdr = bytearray(bytes.fromhex(params["hdr"]))
    data = b"".join(levels_bgra)
    struct.pack_into("<IIHH", hdr, 0x8, len(data), len(data), w, h)     # cardMemory[2], width, height (blob +0xC..)
    return (b"\xff\xff\xff\xff" + bytes(hdr) + b"\xff\xff\xff\xff" + name.encode("latin-1") + b"\0" +
            struct.pack("<IIII", len(levels_bgra), int(params["lflags"], 0), D3DFMT_A8R8G8B8, len(data)) + data)


def bgra_rgba(b):
    a = bytearray(b)
    a[0::4], a[2::4] = b[2::4], b[0::4]
    return bytes(a)


def write_image_files(path, levels_bgra, w, h):
    """<path>.png (level 0) and, with more levels, <path>.mips.png (levels 1.. side by side, top aligned)."""
    from PIL import Image
    Image.frombytes("RGBA", (w, h), bgra_rgba(levels_bgra[0])).save(path + ".png", optimize=True)
    if len(levels_bgra) > 1:
        sizes = [(max(w >> i, 1), max(h >> i, 1)) for i in range(1, len(levels_bgra))]
        strip = Image.new("RGBA", (sum(s[0] for s in sizes), sizes[0][1]), (0, 0, 0, 0))
        x = 0
        for (sw, sh), lv in zip(sizes, levels_bgra[1:]):
            strip.paste(Image.frombytes("RGBA", (sw, sh), bgra_rgba(lv)), (x, 0))
            x += sw
        strip.save(path + ".mips.png", optimize=True)


def read_image_files(path, levels):
    """-> ([level bytes BGRA], width, height) from write_image_files' files (mips made when .mips.png is missing)."""
    import os
    from PIL import Image
    im = Image.open(path + ".png").convert("RGBA")
    w, h = im.size
    rgba = [im.tobytes()]
    sizes = [(max(w >> i, 1), max(h >> i, 1)) for i in range(1, levels)]
    if levels > 1 and os.path.isfile(path + ".mips.png"):
        strip = Image.open(path + ".mips.png").convert("RGBA")
        x = 0
        for sw, sh in sizes:
            rgba.append(strip.crop((x, 0, x + sw, sh)).tobytes())
            x += sw
    else:
        for (sw, sh), (pw, ph) in zip(sizes, [(w, h)] + sizes):
            rgba.append(box_mip(rgba[-1], pw, ph))
    return [bgra_rgba(x) for x in rgba], w, h
