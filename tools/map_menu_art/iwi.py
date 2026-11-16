"""IWI (1.1.1.13 version 9; reads IW4/IW5 version 8 and CoD4 version 6 too) <-> PIL images.

Reads formats 1-5 and 11-13 (RGBA8, RGB8, LA8, L8, A8, DXT1/3/5), top level only; writes version 9 RGBA8 or RGB8
with a full mip chain (or one level with --no-mips), the layout 1.1.1.13's loader reads (smallest level first).
DXT is decoded through Pillow's DDS reader.
"""
import io
import struct
import sys

from PIL import Image

RGBA32, RGB24, LA8, L8, A8, DXT1, DXT3, DXT5 = 1, 2, 3, 4, 5, 11, 12, 13
F_NOPICMIP, F_NOMIPS, F_CLAMPU, F_CLAMPV, F_SRGB = 0x1, 0x2, 0x10, 0x20, 0x100


def header(data):
    if data[:3] != b"IWi":
        raise ValueError("not an IWI file")
    ver = data[3]
    if ver == 13:  # T5 (Black Ops): format u8, flags u8, w, h, depth u16, sizes at 16, data at 48
        fmt, flags = data[4], data[5]
        w, h, d = struct.unpack_from("<HHH", data, 6)
        return ver, flags, fmt, w, h, d, 48
    if ver == 6:  # CoD4: format u8, flags u8, w, h, depth u16, sizes[4] at 12
        fmt, flags = data[4], data[5]
        w, h, d = struct.unpack_from("<HHH", data, 6)
        return ver, flags, fmt, w, h, d, 28
    flags, = struct.unpack_from("<I", data, 4)
    fmt = data[8]
    w, h, d = struct.unpack_from("<hhh", data, 10)
    return ver, flags, fmt, w, h, d, 32


def surface_size(fmt, w, h):
    if fmt == RGBA32:
        return 4 * w * h
    if fmt == RGB24:
        return 3 * w * h
    if fmt == LA8:
        return 2 * w * h
    if fmt in (L8, A8):
        return w * h
    if fmt == DXT1:
        return 8 * ((w + 3) // 4) * ((h + 3) // 4)
    if fmt in (DXT3, DXT5):
        return 16 * ((w + 3) // 4) * ((h + 3) // 4)
    raise ValueError("unsupported IWI format %d" % fmt)


def mip_count(flags, w, h, ver):
    nomips = F_NOMIPS if ver != 6 else 0x2
    if flags & nomips:
        return 1
    n, s = 1, 1
    while s < w or s < h:
        s *= 2
        n += 1
    return n


def _dds(fmt, w, h, payload):
    fourcc = {DXT1: b"DXT1", DXT3: b"DXT3", DXT5: b"DXT5"}[fmt]
    hdr = struct.pack("<4sIIIIIII44xII4s20xI16x", b"DDS ", 124, 0x1007 | 0x80000, h, w, len(payload), 0, 1,
                      32, 0x4, fourcc, 0x1000)
    return hdr + payload


def read(data):
    """-> PIL image (RGBA or RGB) of the top level."""
    ver, flags, fmt, w, h, d, hsize = header(data)
    if ver in (6, 13):
        fmt = {0x01: RGBA32, 0x02: RGB24, 0x03: LA8, 0x04: L8, 0x05: A8, 0x0B: DXT1, 0x0C: DXT3, 0x0D: DXT5}[fmt]
    top = surface_size(fmt, w, h)
    # levels are stored smallest first: the top level is the last `top` bytes
    payload = data[len(data) - top:]
    if fmt in (DXT1, DXT3, DXT5):
        return Image.open(io.BytesIO(_dds(fmt, w, h, payload))).convert("RGBA")
    if fmt == RGBA32:  # stored B,G,R,A
        return Image.frombytes("RGBA", (w, h), payload, "raw", "BGRA")
    if fmt == RGB24:
        return Image.frombytes("RGB", (w, h), payload, "raw", "BGR")
    if fmt == LA8:
        return Image.frombytes("LA", (w, h), payload).convert("RGBA")
    if fmt == L8:
        return Image.frombytes("L", (w, h), payload).convert("RGB")
    a = Image.frombytes("L", (w, h), payload)
    img = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    img.putalpha(a)
    return img


def write(img, alpha=False, mips=True, flags=F_NOPICMIP | F_CLAMPU | F_CLAMPV):
    """PIL image -> IWI v9 bytes (RGBA8 when alpha, else RGB8)."""
    fmt = RGBA32 if alpha else RGB24
    img = img.convert("RGBA" if alpha else "RGB")
    w, h = img.size
    if not mips:
        flags |= F_NOMIPS
    levels = []
    n = mip_count(flags, w, h, 9)
    for lv in range(n):
        lw, lh = max(1, w >> lv), max(1, h >> lv)
        im = img if lv == 0 else img.resize((lw, lh), Image.LANCZOS)
        levels.append(im.tobytes("raw", "BGRA" if alpha else "BGR"))
    body = b"".join(reversed(levels))
    total = 32 + len(body)
    sizes = [total] * 4
    return b"IWi" + bytes([9]) + struct.pack("<I", flags) + bytes([fmt, 0]) + struct.pack("<hhh", w, h, 1) + \
        struct.pack("<4i", *sizes) + body


if __name__ == "__main__":
    # iwi.py topng <in.iwi> <out.png>
    if sys.argv[1] == "topng":
        read(open(sys.argv[2], "rb").read()).save(sys.argv[3])
    elif sys.argv[1] == "info":
        for p in sys.argv[2:]:
            d = open(p, "rb").read()
            print(p, header(d)[:6], len(d))
