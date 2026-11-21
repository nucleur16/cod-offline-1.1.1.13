r"""Builds camerapostable_vlobby.csv: 1.1.1.13's mp/cameraPosTable.csv (patch_code_post_gfx_mp) with one
character_loadout_<env> row per scene merged from another build (vlobby_merge). A row takes the scene's own camera
table row of its build (its first 22 columns, the columns 1.1.1.13 reads; the later builds append 11), with the stand
and camera entities under their merged names (the donor's prefix).

    python camera_rows.py <stock camerapostable.csv> <3.19.26.22 camerapostable.csv> <2.6.9.7 camerapostable.csv> <out.csv>

The tables are AssetExplorer dumps: CodolSoundDumper zone-stringtables --input <zone.ff> --name camerapos --out <dir>.
"""
import csv
import sys

# env name, donor prefix, source row (the donor build's table), stand entity, camera entity
ENVIRONMENTS = {
    "v3": [  # CODOL 3.19.26.22 (the same lobby as 3.5.2.6)
        ("v3_lobby", "character_loadout_center", "player_pos_center", "player_camera_center"),
        ("v3_lobby2", "character_loadout_center2", "player_pos_center", "player_camera_center2"),
        ("v3_details", "char_details", "player_pos_center2", "player_camera_center3"),
        ("v3_career", "career", "player_pos_center", "player_camera_career"),
        ("v3_champion", "champion_select", "champion_select_place1", "champion_select_camera"),
        ("v3_team", "team_lobby", "s1_place1", "team_camera_urb"),
        ("v3_zchamp", "team_lobby_zchamp", "s1_zchamp_place1", "team_camera_urb_zchamp"),
        ("v3_dmz", "team_lobby_dmz", "s1_dmz_place1", "team_camera_urb_dmz"),
        ("v3_depot", "supply_depot", "supply_depot_char_pos", "team_camera_master"),
        ("v3_reward", "daily_reward", "daily_reward_char_pos", "daily_reward_camera"),
    ],
    "v0": [  # CODOL 0.0.2.2: earlier versions of 1.1.1.13's scenes; 1.1.1.13's rows of the same scenes (0.0.2.2's
             # table has 14 columns of the older layout)
        ("v0_urb", "character_loadout_urb", "player_pos_urb", "player_camera_urb"),
        ("v0_woo", "character_loadout_woo", "player_pos_woo", "player_camera_woo"),
        ("v0_des", "character_loadout_des", "player_pos_des", "player_camera_des"),
        ("v0_arc", "character_loadout_arc", "player_pos_arc", "player_camera_arc"),
        ("v0_zm", "team_zombie", "player_pos_zm", "player_camera_zm"),
    ],
    "v2": [  # CODOL 2.6.9.7
        ("v2_lobby", "character_loadout_center", "player_pos_center", "player_camera_center"),
        ("v2_lobby2", "character_loadout_center2", "player_pos_center", "player_camera_center2"),
        ("v2_team", "team_lobby", "s1_place1", "team_camera_urb"),
    ],
}


def read(path):
    with open(path, newline="", encoding="latin-1") as f:
        return [row for row in csv.reader(f)]


def main():
    stock, v3, v2, out = sys.argv[1:5]
    rows = read(stock)
    cols = len(rows[0])
    sources = {"v3": {r[0]: r for r in read(v3)}, "v2": {r[0]: r for r in read(v2)}, "v0": {r[0]: r for r in rows}}
    names = {r[0] for r in rows}
    for prefix, envs in ENVIRONMENTS.items():
        for env, src, stand, camera in envs:
            row = list(sources[prefix][src][:cols])
            row[0] = "character_loadout_" + env
            row[2] = prefix + "_" + stand
            row[3] = prefix + "_" + camera
            assert row[0] not in names, row[0]
            rows.append(row)
    with open(out, "w", newline="", encoding="latin-1") as f:
        csv.writer(f, lineterminator="\n").writerows(rows)
    print("%s: %d rows (%d added)" % (out, len(rows), len(rows) - len(read(stock))))


if __name__ == "__main__":
    main()
