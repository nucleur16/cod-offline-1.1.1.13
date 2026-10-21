# -*- coding: utf-8 -*-
r"""make_glyphs.py -- Kernel33's gamepad button icons (controller_glyphs.c) from Xelu's prompt pack.

Source art: "Xelu's FREE Controllers & Keyboard Prompts" by Nicolae "Xelu" Berbece (Those Awesome Guys),
CC0 1.0 / public domain (glyphs/xelu/Readme.txt, glyphs/LICENSE.md). Downloaded from
https://thoseawesomeguys.com/prompts/ (Xelu_Free_Controller&Key_Prompts.zip, sha256
2848fd4259437a6f594f81ff8d8d7a1f922397c7e9da163b2535aa82ae1df037); the 100x100 PNGs used are copied
unchanged into glyphs/xelu/<set>/.

Sets (controller_glyphs.c order):
    0 xbox360   Others/Xbox 360
    1 ps3       Others/PS3
    2 wiiu      Others/WiiU (by position: bottom B, right A, left Y, top X, as SDL reads Nintendo pads)
    3 generic   Others/Positional Prompts for the face buttons, Steam Deck for the rest
Buttons in kCtlButtons order:
    A(bottom) B(right) X(left) Y(top) LB RB Start Back LS RS Up Down Left Right LT RT HoldX
The pack has no "hold" prompt: HoldX is the set's left face button shrunk inside a ring drawn here.
Each icon is resized to 64x64 (premultiplied alpha) with a full mip chain and written as B,G,R,A rows.

    python glyphs/make_glyphs.py      writes glyphs/glyphs.bin (zlib) and glyphs/preview.png
glyphs.bin: zlib( "K33G" u32 version=1, u32 sets, u32 buttons, u32 size, u32 levels, then for each set,
button, level: (size>>level)^2 * 4 bytes ). controller.c links it with .incbin.
"""
import os
import struct
import zlib

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "xelu")
OUT = 64
LEVELS = 7
BUTTONS = ["A", "B", "X", "Y", "LB", "RB", "START", "BACK", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT", "HOLDX"]

SETS = [
    ("xbox360", ["360_A", "360_B", "360_X", "360_Y", "360_LB", "360_RB", "360_Start", "360_Back",
                 "360_Left_Stick_Click", "360_Right_Stick_Click", "360_Dpad_Up", "360_Dpad_Down", "360_Dpad_Left",
                 "360_Dpad_Right", "360_LT", "360_RT", "@360_X"]),
    ("ps3", ["PS3_Cross", "PS3_Circle", "PS3_Square", "PS3_Triangle", "PS3_L1", "PS3_R1", "PS3_Start", "PS3_Select",
             "PS3_Left_Stick_Click", "PS3_Right_Stick_Click", "PS3_Dpad_Up", "PS3_Dpad_Down", "PS3_Dpad_Left",
             "PS3_Dpad_Right", "PS3_L2", "PS3_R2", "@PS3_Square"]),
    ("wiiu", ["WiiU_B", "WiiU_A", "WiiU_Y", "WiiU_X", "WiiU_L", "WiiU_R", "WiiU_Plus", "WiiU_Minus", "WiiU_Left_Stick",
              "WiiU_Right_Stick", "WiiU_Dpad_Up", "WiiU_Dpad_Down", "WiiU_Dpad_Left", "WiiU_Dpad_Right", "WiiU_ZL",
              "WiiU_ZR", "@WiiU_Y"]),
    ("generic", ["Positional_Prompts_Down", "Positional_Prompts_Right", "Positional_Prompts_Left", "Positional_Prompts_Up",
                 "SteamDeck_L1", "SteamDeck_R1", "SteamDeck_Menu", "SteamDeck_Inventory", "SteamDeck_Left_Stick_Click",
                 "SteamDeck_Right_Stick_Click", "SteamDeck_Dpad_Up", "SteamDeck_Dpad_Down", "SteamDeck_Dpad_Left",
                 "SteamDeck_Dpad_Right", "SteamDeck_L2", "SteamDeck_R2", "@Positional_Prompts_Left"]),
]


def load(set_name, name):
    return Image.open(os.path.join(SRC, set_name, name + ".png")).convert("RGBA")


def hold(img):
    """The pack's button inside a 'hold' ring (amber three quarters, faint rest), drawn at 256 px."""
    S = 256
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    out.alpha_composite(img.resize((188, 188), Image.LANCZOS), (34, 34))
    ring = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(ring)
    d.ellipse((4, 4, 252, 252), outline=(0, 0, 0, 140), width=24)
    ring2 = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d2 = ImageDraw.Draw(ring2)
    d2.arc((8, 8, 248, 248), -90, 180, fill=(255, 200, 40, 255), width=16)
    d2.arc((8, 8, 248, 248), 180, 270, fill=(255, 255, 255, 110), width=16)
    out.alpha_composite(ring)
    out.alpha_composite(ring2)
    return out


def mips(img):
    pm = img.convert("RGBa")
    return [pm.resize((OUT >> lv, OUT >> lv), Image.LANCZOS).convert("RGBA") for lv in range(LEVELS)]


def bgra(im):
    r, g, b, a = im.split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()


def main():
    blob = bytearray(b"K33G" + struct.pack("<5I", 1, len(SETS), len(BUTTONS), OUT, LEVELS))
    preview = Image.new("RGBA", (len(BUTTONS) * (OUT + 8) + 8, len(SETS) * (OUT + 8) + 8), (90, 110, 130, 255))
    for si, (set_name, names) in enumerate(SETS):
        assert len(names) == len(BUTTONS)
        for bi, name in enumerate(names):
            img = hold(load(set_name, name[1:])) if name.startswith("@") else load(set_name, name)
            levels = mips(img)
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
