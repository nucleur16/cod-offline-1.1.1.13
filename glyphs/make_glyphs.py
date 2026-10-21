# -*- coding: utf-8 -*-
r"""make_glyphs.py -- Kernel33's gamepad button glyphs (controller.c), drawn from scratch.

Four sets, all original flat art made here with Pillow (no console art is copied; only the usual colours and
shapes of each family are used):
    0 xbox360   A green, B red, X blue, Y yellow face buttons, LB/RB, LT/RT, Back/Start, LS/RS, D-pad
    1 ps3       Cross / Circle / Square / Triangle shapes, L1/R1, L2/R2, Select/Start, L3/R3, D-pad
    2 wiiu      A/B/X/Y by Nintendo position (bottom B, right A, left Y, top X), L/R, ZL/ZR, -/+, LS/RS, D-pad
    3 generic   face buttons as a diamond of dots (the pressed position filled), shoulder/trigger 1/2, sticks, D-pad
Buttons in controller.c's kCtlButtons order:
    A(bottom) B(right) X(left) Y(top) LB RB Start Back LS RS Up Down Left Right LT RT HoldX
Each glyph is drawn at 256x256, reduced to 64x64 and given a full mip chain (64..1, premultiplied for the
reduction), then written as B,G,R,A rows (D3DFMT_A8R8G8B8).

    python glyphs/make_glyphs.py            writes glyphs/glyphs.bin (zlib) and glyphs/preview.png
glyphs.bin: zlib( "K33G" u32 version=1, u32 sets, u32 buttons, u32 size, u32 levels, then for each set, button,
level: (size>>level)^2 * 4 bytes ). controller.c embeds it with .incbin.
"""
import os
import struct
import zlib

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = r"C:\Windows\Fonts\arialbd.ttf"
S = 256          # drawing size
OUT = 64         # glyph size
LEVELS = 7       # 64 .. 1
BUTTONS = ["A", "B", "X", "Y", "LB", "RB", "START", "BACK", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT", "HOLDX"]
SETS = ["xbox360", "ps3", "wiiu", "generic"]

WHITE = (255, 255, 255, 255)
DARK = (34, 36, 40, 255)
MID = (92, 96, 104, 255)
SHADOW = (0, 0, 0, 150)


def font(px):
    return ImageFont.truetype(FONT, px)


def canvas():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))


def text(d, s, px, fill=WHITE, center=(S / 2, S / 2), shadow=True):
    f = font(px)
    x0, y0, x1, y1 = d.textbbox((0, 0), s, font=f)
    x = center[0] - (x1 - x0) / 2 - x0
    y = center[1] - (y1 - y0) / 2 - y0
    if shadow:
        d.text((x + 5, y + 6), s, font=f, fill=SHADOW)
    d.text((x, y), s, font=f, fill=fill)


def disc(d, fill, rim=None, r=118, highlight=True):
    c = S / 2
    d.ellipse((c - r - 6, c - r - 2, c + r + 6, c + r + 10), fill=SHADOW)          # drop shadow
    d.ellipse((c - r, c - r, c + r, c + r), fill=rim or DARK)
    inner = r - 14
    d.ellipse((c - inner, c - inner, c + inner, c + inner), fill=fill)
    if highlight:
        hi = tuple(int(v + (255 - v) * 0.16) for v in fill[:3]) + (255,)   # opaque: ImageDraw replaces, it does not blend
        d.chord((c - inner + 10, c - inner + 6, c + inner - 10, c + inner - 10), 205, 335, fill=hi)


def darker(col, k=0.55):
    return tuple(int(v * k) for v in col[:3]) + (255,)


def rounded(d, box, r, fill, outline=None, width=0):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 + 5, y0 + 8, x1 + 5, y1 + 8), r, fill=SHADOW)
    d.rounded_rectangle(box, r, fill=fill, outline=outline, width=width)


def bumper(d, label, fill=MID, px=124, ink=WHITE):
    rounded(d, (4, 46, 250, 210), 64, fill, outline=DARK, width=12)
    text(d, label, px, fill=ink, shadow=ink == WHITE)


def trigger(d, label, fill=MID, px=116, ink=WHITE):
    # a trigger: tall shape with a rounded top and a flat bottom
    d.rounded_rectangle((22 + 5, 6 + 8, 234 + 5, 246 + 8), 84, fill=SHADOW)
    d.rounded_rectangle((22, 6, 234, 246), 84, fill=DARK)
    d.rounded_rectangle((36, 20, 220, 232), 72, fill=fill)
    d.rectangle((36, 170, 220, 232), fill=fill)
    text(d, label, px, center=(S / 2, 138), fill=ink, shadow=ink == WHITE)


def stick(d, label, fill=MID, px=104, ink=WHITE):
    c = S / 2
    d.ellipse((10 + 5, 10 + 8, 246 + 5, 246 + 8), fill=SHADOW)
    d.ellipse((10, 10, 246, 246), fill=DARK)
    d.ellipse((36, 36, 220, 220), fill=fill)
    d.ellipse((60, 60, 196, 196), outline=darker(fill, 0.7), width=8)
    text(d, label, px, fill=ink, shadow=ink == WHITE)


def pill(d, fill=MID):
    rounded(d, (24, 76, 232, 180), 52, fill, outline=DARK, width=10)


def dpad(d, arm, fill=(70, 72, 80, 255), hi=WHITE, separate=False):
    w = 92
    c = S / 2
    arms = {
        "UP": (c - w / 2, 4, c + w / 2, c),
        "DOWN": (c - w / 2, c, c + w / 2, S - 4),
        "LEFT": (4, c - w / 2, c, c + w / 2),
        "RIGHT": (c, c - w / 2, S - 4, c + w / 2),
    }
    gap = 16 if separate else 0
    for name, (x0, y0, x1, y1) in arms.items():
        if separate:   # PlayStation: four separate pointed keys
            if name == "UP":
                y1 -= gap
            if name == "DOWN":
                y0 += gap
            if name == "LEFT":
                x1 -= gap
            if name == "RIGHT":
                x0 += gap
        d.rounded_rectangle((x0 + 5, y0 + 8, x1 + 5, y1 + 8), 18, fill=SHADOW)
    for name, (x0, y0, x1, y1) in arms.items():
        if separate:
            if name == "UP":
                y1 -= gap
            if name == "DOWN":
                y0 += gap
            if name == "LEFT":
                x1 -= gap
            if name == "RIGHT":
                x0 += gap
        d.rounded_rectangle((x0, y0, x1, y1), 18, fill=DARK)
        inset = 10
        col = hi if name == arm else fill
        d.rounded_rectangle((x0 + inset, y0 + inset, x1 - inset, y1 - inset), 10, fill=col)
    if not separate:
        d.rectangle((c - w / 2 + 12, c - w / 2 + 12, c + w / 2 - 12, c + w / 2 - 12), fill=fill)
    # an arrow on the pressed arm
    ax = {"UP": (c, 50), "DOWN": (c, S - 50), "LEFT": (50, c), "RIGHT": (S - 50, c)}[arm]
    t = 26
    tri = {
        "UP": [(ax[0], ax[1] - t), (ax[0] - t, ax[1] + t), (ax[0] + t, ax[1] + t)],
        "DOWN": [(ax[0], ax[1] + t), (ax[0] - t, ax[1] - t), (ax[0] + t, ax[1] - t)],
        "LEFT": [(ax[0] - t, ax[1]), (ax[0] + t, ax[1] - t), (ax[0] + t, ax[1] + t)],
        "RIGHT": [(ax[0] + t, ax[1]), (ax[0] - t, ax[1] - t), (ax[0] - t, ax[1] + t)],
    }[arm]
    d.polygon(tri, fill=DARK)


def hold_ring(img):
    """A 'hold' marker: a three-quarter ring around the glyph (the image is shrunk to fit inside)."""
    small = img.resize((196, 196), Image.LANCZOS)
    out = canvas()
    out.alpha_composite(small, (30, 30))
    d = ImageDraw.Draw(out)
    d.arc((6, 6, 250, 250), -90, 180, fill=(255, 214, 64, 255), width=18)
    d.arc((6, 6, 250, 250), 180, 270, fill=(255, 255, 255, 90), width=18)
    return out


# ---- the four families -------------------------------------------------------------------------------

def xbox(name):
    img = canvas()
    d = ImageDraw.Draw(img)
    face = {"A": (76, 175, 60, 255), "B": (216, 50, 42, 255), "X": (47, 111, 214, 255), "Y": (232, 181, 28, 255)}
    if name in face:
        disc(d, face[name], rim=darker(face[name], 0.45))
        text(d, name, 150)
    elif name in ("LB", "RB"):
        bumper(d, name)
    elif name in ("LT", "RT"):
        trigger(d, name)
    elif name in ("LS", "RS"):
        stick(d, name[0])
    elif name == "START":
        disc(d, MID)
        d.polygon([(100, 78), (100, 178), (178, 128)], fill=WHITE)
    elif name == "BACK":
        disc(d, MID)
        d.polygon([(156, 78), (156, 178), (78, 128)], fill=WHITE)
    elif name in ("UP", "DOWN", "LEFT", "RIGHT"):
        dpad(d, name)
    elif name == "HOLDX":
        img = hold_ring(xbox("X"))
    return img


def ps3(name):
    img = canvas()
    d = ImageDraw.Draw(img)
    c = S / 2
    if name == "A":      # Cross
        disc(d, DARK, rim=(18, 18, 20, 255), highlight=False)
        col = (111, 159, 224, 255)
        d.line((c - 52, c - 52, c + 52, c + 52), fill=col, width=22)
        d.line((c - 52, c + 52, c + 52, c - 52), fill=col, width=22)
    elif name == "B":    # Circle
        disc(d, DARK, rim=(18, 18, 20, 255), highlight=False)
        d.ellipse((c - 58, c - 58, c + 58, c + 58), outline=(224, 90, 90, 255), width=20)
    elif name == "X":    # Square
        disc(d, DARK, rim=(18, 18, 20, 255), highlight=False)
        d.rectangle((c - 52, c - 52, c + 52, c + 52), outline=(228, 138, 216, 255), width=20)
    elif name == "Y":    # Triangle
        disc(d, DARK, rim=(18, 18, 20, 255), highlight=False)
        col = (79, 209, 164, 255)
        pts = [(c, c - 64), (c - 62, c + 46), (c + 62, c + 46), (c, c - 64)]
        d.line(pts, fill=col, width=20, joint="curve")
    elif name in ("LB", "RB"):
        bumper(d, name[0] + "1", fill=(60, 62, 68, 255))
    elif name in ("LT", "RT"):
        trigger(d, name[0] + "2", fill=(60, 62, 68, 255))
    elif name in ("LS", "RS"):
        stick(d, name[0] + "3", fill=(60, 62, 68, 255), px=92)
    elif name == "START":
        pill(d, (60, 62, 68, 255))
        d.polygon([(104, 96), (104, 160), (160, 128)], fill=WHITE)
    elif name == "BACK":    # Select: a flat bar
        pill(d, (60, 62, 68, 255))
        d.rounded_rectangle((86, 112, 170, 144), 8, fill=WHITE)
    elif name in ("UP", "DOWN", "LEFT", "RIGHT"):
        dpad(d, name, fill=(60, 62, 68, 255), separate=True)
    elif name == "HOLDX":
        img = hold_ring(ps3("X"))
    return img


def wiiu(name):
    img = canvas()
    d = ImageDraw.Draw(img)
    light = (232, 232, 236, 255)
    labels = {"A": "B", "B": "A", "X": "Y", "Y": "X"}   # by position: bottom B, right A, left Y, top X
    if name in labels:
        disc(d, light, rim=(150, 152, 160, 255))
        text(d, labels[name], 150, fill=(60, 62, 70, 255), shadow=False)
    elif name in ("LB", "RB"):
        bumper(d, name[0], fill=(206, 208, 214, 255), ink=(52, 54, 62, 255))
    elif name in ("LT", "RT"):
        trigger(d, "Z" + name[0], fill=(206, 208, 214, 255), px=104, ink=(52, 54, 62, 255))
    elif name in ("LS", "RS"):
        stick(d, name[0], fill=(206, 208, 214, 255), ink=(52, 54, 62, 255))
    elif name == "START":
        disc(d, (200, 202, 208, 255), rim=(120, 122, 130, 255))
        d.rectangle((S / 2 - 52, S / 2 - 13, S / 2 + 52, S / 2 + 13), fill=DARK)
        d.rectangle((S / 2 - 13, S / 2 - 52, S / 2 + 13, S / 2 + 52), fill=DARK)
    elif name == "BACK":
        disc(d, (200, 202, 208, 255), rim=(120, 122, 130, 255))
        d.rectangle((S / 2 - 52, S / 2 - 13, S / 2 + 52, S / 2 + 13), fill=DARK)
    elif name in ("UP", "DOWN", "LEFT", "RIGHT"):
        dpad(d, name, fill=(120, 122, 130, 255))
    elif name == "HOLDX":
        img = hold_ring(wiiu("X"))
    return img


def generic(name):
    img = canvas()
    d = ImageDraw.Draw(img)
    c = S / 2
    pos = {"A": (c, c + 70), "B": (c + 70, c), "X": (c - 70, c), "Y": (c, c - 70)}
    if name in pos:
        d.ellipse((10 + 5, 10 + 8, 246 + 5, 246 + 8), fill=SHADOW)
        d.ellipse((10, 10, 246, 246), fill=DARK)
        for k, (x, y) in pos.items():
            r = 34
            if k == name:
                d.ellipse((x - r, y - r, x + r, y + r), fill=WHITE)
            else:
                d.ellipse((x - r + 6, y - r + 6, x + r - 6, y + r - 6), outline=MID, width=8)
    elif name in ("LB", "RB"):
        bumper(d, name[0] + "1")
    elif name in ("LT", "RT"):
        trigger(d, name[0] + "2")
    elif name in ("LS", "RS"):
        stick(d, name[0])
    elif name == "START":   # three lines
        pill(d)
        for y in (100, 128, 156):
            d.rounded_rectangle((92, y - 8, 164, y + 8), 6, fill=WHITE)
    elif name == "BACK":    # two squares
        pill(d)
        d.rectangle((84, 104, 120, 140), outline=WHITE, width=8)
        d.rectangle((136, 116, 172, 152), fill=WHITE)
    elif name in ("UP", "DOWN", "LEFT", "RIGHT"):
        dpad(d, name)
    elif name == "HOLDX":
        img = hold_ring(generic("X"))
    return img


DRAW = [xbox, ps3, wiiu, generic]


def mips(img):
    """Premultiplied reduction to 64x64 and every lower level."""
    out = []
    pm = img.convert("RGBa")
    for lv in range(LEVELS):
        size = OUT >> lv
        out.append(pm.resize((size, size), Image.LANCZOS).convert("RGBA"))
    return out


def bgra(im):
    r, g, b, a = im.split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()


def main():
    blob = bytearray(b"K33G" + struct.pack("<5I", 1, len(SETS), len(BUTTONS), OUT, LEVELS))
    preview = Image.new("RGBA", (len(BUTTONS) * (OUT + 8) + 8, len(SETS) * (OUT + 8) + 8), (90, 110, 130, 255))
    for si, draw in enumerate(DRAW):
        for bi, name in enumerate(BUTTONS):
            levels = mips(draw(name))
            preview.alpha_composite(levels[0], (8 + bi * (OUT + 8), 8 + si * (OUT + 8)))
            for lv in levels:
                blob += bgra(lv)
    data = zlib.compress(bytes(blob), 9)
    with open(os.path.join(HERE, "glyphs.bin"), "wb") as f:
        f.write(data)
    preview.save(os.path.join(HERE, "preview.png"))
    print("glyphs.bin: %d sets x %d buttons, %d bytes raw, %d compressed" % (len(SETS), len(BUTTONS), len(blob), len(data)))


if __name__ == "__main__":
    main()
