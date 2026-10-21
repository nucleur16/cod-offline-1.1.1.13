# -*- coding: utf-8 -*-
r"""make_diagrams.py -- the controller diagrams of Options > Controller (voice_ui/OptionView.as).

Original line art, drawn here as data (no image is copied from anyone): a smooth body outline (closed
quadratic B-spline through the points), and the controls as circles, rounded rectangles, sticks and a
D-pad cross. One layout per glyph family, in the same order as controller_glyphs.c:

    0 Xbox 360      offset sticks: left stick high, D-pad low; face buttons high right, right stick low
    1 PS3           D-pad and face buttons high, both sticks low and inward
    2 Wii U Pro     sticks high and outward, D-pad and face buttons below them
    3 generic       an Xbox-like layout without labels on the face buttons

Coordinates are in a 440 x 250 box. Each layout also names an anchor point per button (A B X Y LB RB LT
RT LS RS BACK START UP DOWN LEFT RIGHT) where Options > Controller starts that button's callout line.

    python glyphs/make_diagrams.py     writes voice_ui/k33_pad_diagrams.as.inc (spliced into OptionView.as
                                       at "// @K33DIAGRAMS@" by build_voice_ui.py)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "voice_ui", "k33_pad_diagrams.as.inc")


def body_360():
    return [(110, 40), (160, 52), (280, 52), (330, 40), (380, 60), (405, 120), (420, 200), (400, 238), (360, 230),
            (320, 180), (220, 170), (120, 180), (80, 230), (40, 238), (20, 200), (35, 120), (60, 60)]


def body_ps3():
    return [(105, 50), (170, 58), (270, 58), (335, 50), (385, 70), (408, 125), (418, 200), (395, 238), (355, 228),
            (318, 175), (220, 168), (122, 175), (85, 228), (45, 238), (22, 200), (32, 125), (55, 70)]


def body_wiiu():
    return [(100, 38), (170, 48), (270, 48), (340, 38), (395, 62), (415, 130), (412, 205), (385, 235), (345, 222),
            (310, 185), (220, 178), (130, 185), (95, 222), (55, 235), (28, 205), (25, 130), (45, 62)]


LAYOUTS = [
    {   # Xbox 360
        "body": body_360(),
        "shapes": [("r", 82, 14, 80, 22, 10, "LT"), ("r", 278, 14, 80, 22, 10, "RT"),
                   ("r", 88, 40, 74, 14, 7, "LB"), ("r", 278, 40, 74, 14, 7, "RB"),
                   ("s", 105, 100, 24, "LS"), ("d", 160, 150, 22, ""), ("s", 280, 150, 24, "RS"),
                   ("c", 345, 125, 12, "A"), ("c", 370, 100, 12, "B"), ("c", 320, 100, 12, "X"), ("c", 345, 75, 12, "Y"),
                   ("c", 190, 100, 8, "<"), ("c", 250, 100, 8, ">"), ("c", 220, 72, 15, "")],
        "anchors": {"LT": (100, 22), "RT": (340, 22), "LB": (100, 46), "RB": (340, 46), "LS": (105, 100), "RS": (280, 150),
                    "A": (345, 125), "B": (370, 100), "X": (320, 100), "Y": (345, 75), "BACK": (190, 100), "START": (250, 100),
                    "UP": (160, 135), "DOWN": (160, 165), "LEFT": (145, 150), "RIGHT": (175, 150)},
    },
    {   # PS3
        "body": body_ps3(),
        "shapes": [("r", 82, 18, 78, 22, 8, "L2"), ("r", 280, 18, 78, 22, 8, "R2"),
                   ("r", 88, 44, 72, 14, 6, "L1"), ("r", 280, 44, 72, 14, 6, "R1"),
                   ("d", 100, 105, 24, ""), ("s", 165, 160, 22, "L3"), ("s", 275, 160, 22, "R3"),
                   ("c", 340, 128, 12, "×"), ("c", 365, 105, 12, "○"), ("c", 315, 105, 12, "□"), ("c", 340, 82, 12, "△"),
                   ("r", 180, 100, 22, 10, 4, ""), ("c", 262, 105, 8, ">"), ("c", 220, 128, 10, "")],
        "anchors": {"LT": (100, 30), "RT": (340, 30), "LB": (100, 50), "RB": (340, 50), "LS": (165, 160), "RS": (275, 160),
                    "A": (340, 128), "B": (365, 105), "X": (315, 105), "Y": (340, 82), "BACK": (191, 105), "START": (262, 105),
                    "UP": (100, 89), "DOWN": (100, 121), "LEFT": (84, 105), "RIGHT": (116, 105)},
    },
    {   # Wii U Pro
        "body": body_wiiu(),
        "shapes": [("r", 82, 12, 78, 22, 8, "ZL"), ("r", 280, 12, 78, 22, 8, "ZR"),
                   ("r", 88, 38, 72, 14, 6, "L"), ("r", 280, 38, 72, 14, 6, "R"),
                   ("s", 95, 95, 24, "L"), ("s", 345, 95, 24, "R"), ("d", 150, 150, 22, ""),
                   ("c", 315, 165, 11, "B"), ("c", 338, 142, 11, "A"), ("c", 292, 142, 11, "Y"), ("c", 315, 119, 11, "X"),
                   ("c", 195, 110, 8, "-"), ("c", 245, 110, 8, "+"), ("c", 220, 140, 9, "")],
        "anchors": {"LT": (100, 22), "RT": (340, 22), "LB": (100, 44), "RB": (340, 44), "LS": (95, 95), "RS": (345, 95),
                    "A": (315, 165), "B": (338, 142), "X": (292, 142), "Y": (315, 119), "BACK": (195, 110), "START": (245, 110),
                    "UP": (150, 135), "DOWN": (150, 165), "LEFT": (135, 150), "RIGHT": (165, 150)},
    },
    {   # generic
        "body": body_360(),
        "shapes": [("r", 82, 14, 80, 22, 10, "L2"), ("r", 278, 14, 80, 22, 10, "R2"),
                   ("r", 88, 40, 74, 14, 7, "L1"), ("r", 278, 40, 74, 14, 7, "R1"),
                   ("s", 105, 100, 24, "L"), ("d", 160, 150, 22, ""), ("s", 280, 150, 24, "R"),
                   ("c", 345, 125, 12, ""), ("c", 370, 100, 12, ""), ("c", 320, 100, 12, ""), ("c", 345, 75, 12, ""),
                   ("c", 190, 100, 8, ""), ("c", 250, 100, 8, "")],
        "anchors": {"LT": (100, 22), "RT": (340, 22), "LB": (100, 46), "RB": (340, 46), "LS": (105, 100), "RS": (280, 150),
                    "A": (345, 125), "B": (370, 100), "X": (320, 100), "Y": (345, 75), "BACK": (190, 100), "START": (250, 100),
                    "UP": (160, 135), "DOWN": (160, 165), "LEFT": (145, 150), "RIGHT": (175, 150)},
    },
]


def as_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    out = ["      // generated by glyphs/make_diagrams.py: controller line art for Options > Controller",
           "      private function k33Diagram(t:int):Object",
           "      {",
           "         switch(t)",
           "         {"]
    for i, lay in enumerate(LAYOUTS):
        body = ",".join("%d,%d" % p for p in lay["body"])
        shapes = ",".join("[" + ",".join(as_str(v) if isinstance(v, str) else str(v) for v in sh) + "]" for sh in lay["shapes"])
        anchors = ",".join("%s:[%d,%d]" % (k, v[0], v[1]) for k, v in lay["anchors"].items())
        out.append("            case %d:" % i if i < len(LAYOUTS) - 1 else "            default:")
        out.append("               return {body:[%s],shapes:[%s],anchors:{%s}};" % (body, shapes, anchors))
    out += ["         }", "      }", ""]
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
