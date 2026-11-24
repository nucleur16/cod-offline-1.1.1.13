# -*- coding: utf-8 -*-
r"""Sound alias assets (snd_alias_list_t) for Survival's zones, written as 1.1.1.13 zone blobs.

Layout, MEASURED on stock 1.1.1.13 blobs (localized_common_gametype_mp: ab_1mc_achieve_agm; zm_sumpf_sh:
mus_zombie_bossbattle) and on CODOL-AssetExplorer Sounds/SoundAliasReader.cs:

    list    {name*, head*, count}, name, snd_alias_t[count], then per alias the data its pointers name, in this order:
    alias   108 bytes: 00 aliasName 04 subtitle 08 secondary 0C chain 10 mixerGroup 14 SoundFile* 18 sequence
            1C volMin 20 volMax 24 pitchMin 28 pitchMax 2C distMin 30 distMax 34 ? 38 flags 3C masterPriority
            40 master% 44 slave% 48 probability 4C lfe% 50 center% 54 startDelay 58 SndCurve* 5C 60 64 ? 68 SpeakerMap*
    file    {u8 type, u8 exists, u16, dir*, name*} + dir + name; type 2 = streamed ("sound/<dir>/<name>" in the IWDs)
    curve   {name*, knots, vec2[16]} + name
    map     {u32 isDefault, name*, MSSChannelMap[2][2]} + name

Every pointer is 0xFFFFFFFF with its data inline, so a blob has no back references and can sit in any zone.
flags: bit 0 looping, bits 7-8 load type (2 = streamed), bits 9-14 the channel (soundaliases/channels.def:
10 auto2d, 32 music, 35 announcer)."""
from __future__ import annotations

import struct

INLINE = 0xFFFFFFFF
FLAGS_ANNOUNCER = 0x4718   # uk_1mc_* (localized_common_gametype_mp)
FLAGS_PARTNER = 0x4710     # SO_UK_1_inform_bleedout (localized_common_gametype_coop)
FLAGS_MUSIC = 0x4118       # mp_suspense, nuke_music (common_gametype_mp)
FLAGS_AUTO2D = 0x1518      # auto2d, streamed, one shot (2.6.9.7's wave splashes)


def _f(*v):
    return struct.pack("<%df" % len(v), *v)


def _channel(speakers, inputs, levels):
    """One MSSChannelMap: speaker i takes `inputs` levels."""
    out = struct.pack("<I", speakers)
    for i in range(6):
        out += struct.pack("<II", i, inputs) + _f(*levels[i]) if i < speakers else b"\0" * 16
    return out


# the speaker maps of the stock aliases named above, level by level
_STEREO = _channel(2, 1, [(0.5, 0), (0.5, 0)])
_STEREO2 = _channel(2, 2, [(1, 0), (0, 1)])
SPEAKER_MAPS = {
    "radio": [_STEREO, _channel(6, 1, [(0.45, 0), (0.45, 0), (0.1, 0), (0, 0), (0, 0), (0, 0)]), _STEREO2,
              _channel(6, 2, [(0.5, 0), (0, 0.5), (0.5, 0.5), (0, 0), (0, 0), (0, 0)])],
    "music": [_STEREO, _channel(6, 1, [(0.3, 0), (0.3, 0), (0.1, 0), (0.1, 0), (0.15, 0), (0.15, 0)]), _STEREO2,
              _channel(6, 2, [(0.7, 0), (0, 0.7), (0, 0), (0, 0), (0.3, 0), (0, 0.3)])],
}


def _s(text):
    return text.encode("ascii") + b"\0"


def _alias(name, v):
    """(the 108-byte struct, the data that follows it) of one variant."""
    map_name = v.get("speakers", "radio")
    raw = struct.pack("<6I", INLINE, INLINE if v.get("subtitle") else 0, 0, 0, INLINE if v.get("mixer") else 0, INLINE)
    raw += struct.pack("<I", 0)
    dist = v.get("dist", (360, 600))
    raw += _f(v["vol"], v["vol"], 1, 1, dist[0], dist[1], 0)
    raw += struct.pack("<II", v["flags"], v.get("priority", 75))
    raw += _f(0, v.get("slave", 1), 1, 0, v.get("center", 0))
    raw += struct.pack("<II", 0, INLINE) + _f(0, 0, 0) + struct.pack("<I", INLINE)
    assert len(raw) == 108, len(raw)
    d = _s(name)
    if v.get("subtitle"):
        d += _s(v["subtitle"])
    if v.get("mixer"):
        d += _s(v["mixer"])
    folder, _, file = v["file"].rpartition("/")
    d += struct.pack("<BBHII", 2, 1, 0, INLINE, INLINE) + _s(folder) + _s(file)
    d += struct.pack("<II", INLINE, 2) + _f(0, 1, 1, 0) + b"\0" * (8 * 14) + _s("$default")
    d += struct.pack("<II", 0, INLINE) + b"".join(SPEAKER_MAPS[map_name]) + _s(map_name)
    return raw, d


def blob(name, variants):
    """The zone blob of alias list `name`."""
    structs, data = b"", b""
    for v in variants:
        raw, d = _alias(name, v)
        structs += raw
        data += d
    return struct.pack("<3I", INLINE, INLINE, len(variants)) + _s(name) + structs + data


def announcer(files, subtitles=None, vol=0.7):
    out = []
    for i, f in enumerate(files):
        v = {"file": f, "vol": vol, "flags": FLAGS_ANNOUNCER, "mixer": "max", "center": 0.1}
        if subtitles:
            v["subtitle"] = subtitles[i]
        out.append(v)
    return out


def music(file, vol):
    return [{"file": file, "vol": vol, "flags": FLAGS_MUSIC, "mixer": "music", "dist": (120, 600), "priority": 50,
             "speakers": "music"}]


def sting(file, vol=0.81):
    return [{"file": file, "vol": vol, "flags": FLAGS_AUTO2D, "dist": (120, 600), "speakers": "music"}]


def partner(files, vol=0.7):
    """A co-op partner's line over the radio."""
    return [{"file": f, "vol": vol, "flags": FLAGS_PARTNER, "center": 0.1} for f in files]


# --------------------------------------------------------------------------------------------------------------------
# zones/patch_common_gametype_survival: aliases over files every 1.1.1.13 install streams (iw_sound_*.iwd).
# --------------------------------------------------------------------------------------------------------------------
_TF_SO = "battlechatter/TF/SO/SO_UK_1_inform_"
BASE = {
    # the Survival theme: 1.1.1.13 ships it as the zombies room music (music_inroom_zm, variant 0)
    "survival_music_start": music("music/cyborg/bt_so_survival.mp3", 0.6),
    # co-op Crash's battle tracks (crash_escape, crash_data_defense): the boss music without the audio pack
    "survival_music_boss_chopper": music("music/crash/bt_warlord_mortar_inbound_lp.mp3", 0.5),
    "survival_music_boss_jug": music("music/crash/bt_warlord_go_hot_lp.mp3", 0.5),
    # "More hostiles, do not let them pass", "Multiple fire teams spotted" (co-op Favela and Crash radio)
    "survival_vo_enemies": partner(["voiceovers/_codo_favela/favela_pri_hostiles01.mp3",
                                    "voiceovers/dcburn/dcburn_bhp_30_16_r.mp3"]),
    # Spec Ops partner lines (so_uk_1_inform_suppressed, _bleedout, _revived, localized_common_gametype_coop):
    # "I'm down! Help me out!", "I'm bleeding out!", "Come on, get up!"
    "survival_vo_player_down": partner([_TF_SO + "suppressed_%02d.mp3" % i for i in (1, 2, 3, 4)]),
    "survival_vo_player_bleeding": partner([_TF_SO + "bleedout_%02d.mp3" % i for i in (1, 2, 3, 4)]),
    "survival_vo_player_revived": partner([_TF_SO + "revived_%02d.mp3" % i for i in (1, 2)]),
}

# --------------------------------------------------------------------------------------------------------------------
# The audio pack: CoD Online 2.6.9.7's own Survival aliases (common_gametype_survival and
# localized_common_gametype_survival there; the missile chopper's line and the subtitle names are 3.5.6.2's).
# --------------------------------------------------------------------------------------------------------------------
_VO = "voiceovers/codo_mp/ancr/tf141/uk_1mc_"
_SUB = "SUBTITLE_SURVIVAL_SO_HQ_"


def _vo(stem, n, sub):
    r = range(1, n + 1)
    return announcer([_VO + "%s_%02d.mp3" % (stem, i) for i in r], [_SUB + "%s_%02d" % (sub, i) for i in r])


PACK = {
    "so_hq_mission_intro": _vo("mission_intro", 6, "MISSION_INTRO"),
    # alone: 2.6.9.7 lists these three of the six
    "so_hq_mission_intro_sp": [_vo("mission_intro", 6, "MISSION_INTRO")[i - 1] for i in (2, 4, 5)],
    "so_hq_enemy_intel_generic": _vo("enemy_intel_gnrc", 6, "ENEMY_INTEL_GENERIC"),
    "so_hq_enemy_intel_martyrdom": _vo("enemy_intel_mrtdm", 2, "ENEMY_INTEL_MARTYRDOM"),
    "so_hq_enemy_intel_chemical": _vo("enemy_intel_chmcl", 2, "ENEMY_INTEL_CHEMICAL"),
    "so_hq_enemy_intel_dog_reg": _vo("enemy_intel_dogs", 2, "ENEMY_INTEL_DOGS"),
    "so_hq_enemy_intel_dog_splode": _vo("enemy_intel_dog_xp", 2, "ENEMY_INTEL_DOGS_SPLODE"),
    "so_hq_enemy_intel_boss_transport": _vo("enemy_intel_trnspt", 2, "ENEMY_INTEL_TRANSPORT"),
    "so_hq_enemy_intel_boss_transport_many": announcer([_VO + "enemy_intel_trs_my_01.mp3"],
                                                       [_SUB + "ENEMY_INTEL_TRANSPORT_MANY"]),
    "so_hq_boss_intel_chopper": _vo("boss_intel_chopper", 2, "BOSS_INTEL_CHOPPER"),
    "so_hq_boss_intel_chopper_many": _vo("boss_intel_cpr_my", 2, "BOSS_INTEL_CHOPPER_MANY"),
    "so_hq_boss_intel_missile_chopper": announcer([_VO + "chopper_inbound.mp3"],
                                                  [_SUB + "BOSS_INTEL_MISSILE_CHOPPER_01"]),
    "so_hq_enemy_update_claymore": _vo("enemy_update_clmr", 2, "ENEMY_UPDATE_CLAYMORE"),
    "so_hq_wave_over_flavor": _vo("wave_over_flavor", 6, "WAVE_OVER_FLAVOR"),
    "so_survival_boss_music_01": music("music/bt_london_warehouse.mp3", 0.648),
    "so_survival_boss_music_02": music("music/bt_pragueescape_defend.mp3", 0.648),
    "survival_wave_start_splash": sting("mp/mp_bonus_end_02.mp3"),
    "survival_wave_end_splash": sting("mp/mp_bonus_start_02.mp3"),
}


def files(aliases):
    """The streamed files (lower case, below sound/) `aliases` play."""
    return sorted({v["file"].lower() for vs in aliases.values() for v in vs})


def subtitles(aliases):
    return sorted({v["subtitle"] for vs in aliases.values() for v in vs if v.get("subtitle")})
