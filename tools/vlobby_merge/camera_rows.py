r"""Builds camerapostable_vlobby.csv and framing_vlobby.txt: 1.1.1.13's mp/cameraPosTable.csv (patch_code_post_gfx_mp)
with the rows of the scenes merged from other builds (vlobby_merge), and the cameras those rows name.

    python camera_rows.py <stock camerapostable.csv> <3.19.26.22 camerapostable.csv> <2.6.9.7 camerapostable.csv>
                          <0.0.2.2 camerapostable.csv> <out.csv> <out framing.txt>

The tables are AssetExplorer dumps: CodolSoundDumper zone-stringtables --input <zone.ff> --name camerapos --out <dir>.

The scenes are menu scenes:
- Menu scenes (character_loadout_<name>, the main menu's and the Multiplayer lobby's character view, ui_vlobbyEnv):
  the later builds' lobby, team lobby, champion select, depot and reward scenes, and 0.0.2.2's main menu (v0_menu).
  The 3.x / 2.x ones are framed like 1.1.1.13's own menu (character_loadout_center): a camera at the stock camera's
  offset from the stand (Framing.cs, framing_vlobby.txt), the stock row's distance and field of view; the scene's own
  row gives the rest (vision, light, depth of field). Rows of one stand frame alike, so one row per stand is kept.
  v0_menu is 0.0.2.2's own main menu view: its home page shows character_loadout_<terrain> with terrain urb
  (CharacterLoadoutService.TERRAIN_NAME[0]), the urban stand seen from player_camera_urb.
Rooms (the game lobby, team_<environment>) keep the stock environment scenes.

0.0.2.2's rows have 14 columns of an older layout: 0-9 as 1.1.1.13's (name, game, stand, camera, distance, vision,
light pitch / yaw / roll, light colour), 10-11 the weapon page's second stand and camera, 12 the light intensity
(1.1.1.13: column 10), 13 the field of view (1.1.1.13: 13). The 1.1.1.13 columns 0.0.2.2 has no counterpart for come
from 1.1.1.13's row of the same scene.
"""
import csv
import sys

# menu scenes framed like the stock menu: env name, donor prefix, source row (the donor build's table), stand entity
FRAMED = {
    "v3": [  # CODOL 3.19.26.22 (the same lobby as 3.5.2.6)
        ("v3_lobby", "character_loadout_center", "player_pos_center"),
        ("v3_champion", "champion_select", "champion_select_place1"),
        ("v3_team", "team_lobby", "s1_place1"),
        ("v3_zchamp", "team_lobby_zchamp", "s1_zchamp_place1"),
        ("v3_dmz", "team_lobby_dmz", "s1_dmz_place1"),
        ("v3_depot", "supply_depot", "supply_depot_char_pos"),
        ("v3_reward", "daily_reward", "daily_reward_char_pos"),
    ],
    "v2": [  # CODOL 2.6.9.7
        ("v2_lobby", "character_loadout_center", "player_pos_center"),
        ("v2_team", "team_lobby", "s1_place1"),
    ],
}
# the stock menu view the framed rows copy (distance column 4, field of view column 13, the camera's offset)
STOCK_MENU = ("character_loadout_center", "player_pos_center", "player_camera_center")

# 0.0.2.2: row name in this table, 0.0.2.2 source row, 1.1.1.13 row of the same scene (the columns 0.0.2.2 lacks)
V0 = [
    ("character_loadout_v0_menu", "character_loadout_urb", "character_loadout_urb"),
]


def read(path):
    with open(path, newline="", encoding="latin-1") as f:
        return [row for row in csv.reader(f)]


def main():
    if len(sys.argv) != 7:
        sys.exit(__doc__)
    stock, v3, v2, v0, out, frame_out = sys.argv[1:7]
    rows = read(stock)
    cols = len(rows[0])
    by = {r[0]: r for r in rows}
    sources = {"v3": {r[0]: r for r in read(v3)}, "v2": {r[0]: r for r in read(v2)}}
    menu = by[STOCK_MENU[0]]
    frames = []
    added = []
    for prefix, envs in FRAMED.items():
        for env, src, stand in envs:
            row = list(sources[prefix][src][:cols])
            camera = "%s_frame_%s" % (prefix, env.split("_", 1)[1])
            row[0] = "character_loadout_" + env
            row[2] = prefix + "_" + stand
            row[3] = camera
            row[4] = menu[4]      # distance
            row[13] = menu[13]    # field of view
            frames.append("%s %s %s %s" % (camera, row[2], STOCK_MENU[1], STOCK_MENU[2]))
            added.append(row)
    old = {r[0]: r for r in read(v0)}
    for name, src, like in V0:
        o, s = old[src], by[like]
        row = list(s)
        row[0] = name
        row[2] = "v0_" + o[2]
        row[3] = "v0_" + o[3]
        for c in range(4, 10):
            if o[c].strip():
                row[c] = o[c]
        if o[12].strip():
            row[10] = o[12]
        if o[13].strip():
            row[13] = o[13]
        added.append(row)
    names = {r[0] for r in rows}
    for row in added:
        assert row[0] not in names, row[0]
        names.add(row[0])
    rows += added
    with open(out, "w", newline="", encoding="latin-1") as f:
        csv.writer(f, lineterminator="\n").writerows(rows)
    with open(frame_out, "w", newline="\n", encoding="latin-1") as f:
        f.write("# <new camera> <donor stand> <stock stand> <stock camera> (camera_rows.py; Framing.cs)\n")
        f.write("\n".join(frames) + "\n")
    print("%s: %d rows (%d added); %s: %d cameras" % (out, len(rows), len(added), frame_out, len(frames)))


if __name__ == "__main__":
    main()
