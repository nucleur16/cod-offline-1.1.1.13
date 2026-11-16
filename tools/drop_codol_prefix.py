#!/usr/bin/env python3
"""Drops the "codol" branding from Kernel33's own names (identifiers, macros, dvars, console commands, the
names the client forwards to the hosted sh1x match server) with an explicit table, so the same rename can be
applied again to another branch (for example after a rebase onto the branch that first ran it).

Step 2 (IW_TABLE) renames Kernel33's k33 console names (dvars, commands, command-line switches) to IW engine
prefixes chosen by function: cg_ HUD/drawing, cl_ client/voice/input, snd_ sound, con_ console, r_ renderer,
com_ common, fs_ filesystem, ui_ front end, loc_ language, net_ network, sv_ hosting, gpad_ gamepad. One run
applies both steps (a chain codol_voice -> k33_voice -> cl_voiceChat resolves to the last name). Step 2 also
covers the native menu sources under zones/ (menufile .json/.txt, sources/*.menu; a stored .bin menulist is
re-encoded from its .json with zonekit.menus), refuses any name a shipped UI movie spells (KEEP_K33 lists them),
and writes src/dvar_renames.h, from which c1game.c moves a value saved under an old dvar name to the new one.

    python tools/drop_codol_prefix.py            apply the table to the tree, re-pin ui_baseline.json
    python tools/drop_codol_prefix.py --check    change nothing; exit 1 when a mapped name is still present or
                                                 a "codol" name in the code is neither mapped nor listed in KEEP
    python tools/drop_codol_prefix.py --report   print every remaining "codol" name in the code with its reason
    python tools/drop_codol_prefix.py --tree DIR [--check]
                                                 apply (or check) GSC_TABLE, the console names the GSC scripts read
                                                 or set, in another checkout: the GSC tree (13-rawfiles)

Step 3 (GSC_TABLE) renames the console names with "codol" that the GSC scripts read or set (scr_codol_botfill ->
scr_botfill, codol_zoneType -> sv_zoneType) on all three sides: here, in the GSC tree (--tree) and in the server
(its rename_prefix_map.json config.gsc_names, which --check compares with GSC_TABLE once server/sh1x carries it).

Deterministic and idempotent: the table maps whole names only (no new name contains an old one), so a second run
changes nothing. The rules:
  * A name is matched as a whole word: not inside a longer identifier, and not when a file extension follows it
    (codol_voice.c, codol_dev_cheats.c, codol_video_mode.cfg are file names of other repositories or installs).
    "-DCODOL_X" (a compiler define) and "^3codol_x" (a colour code in front of a name) count as word starts.
  * Scope: the code (src/, tests/, tools/, scripts/, zonekit/) and the documentation (README.md, every *.md),
    never server/sh1x (its own rename is done in the server repository), zones/, zone_source/, zone_packs/,
    files/ or binary files, and never this file.
  * Kernel33's own C names drop the prefix (CodolPatchZoneRetired -> PatchZoneRetired, CODOL_LANG_MAX ->
    LANG_MAX); the five build-profile macros become K33_* (the K33_* build variables that set them already
    exist, and a bare DIAGNOSTICS is taken by sqlext.h).
  * Dvars and command-line switches Kernel33 itself registers or reads become k33_* (the client's existing
    prefix: k33_voice beside k33_voiceVolume), environment knobs of this repository's own scripts become K33_*.
  * Names the client sends to the match server follow the server repository's rule (its
    tools/rename_prefix_map.json): "codol_" is dropped (codol_devServer -> devServer). codol_dev_cheats becomes
    dev_cheats: the server registers both names for the same handler (CODOL_DevCheats_f), so they merge.
    scripts/gen_sh1x_cmds.py reads the server source through server_name()/server_text() below, so
    src/console/sh1x_cmds.h lists the new names even while the pinned submodule still registers the old ones
    (once it registers the new ones that is a no-op).
  * KEEP lists every "codol" name that stays, with its reason: names that must match bytes outside this
    repository (patch zone names, names compiled into the shipped UI movies, the launcher's and the Python
    lobby's environment variables, external tools, files in installs, the Discord application's asset key).
--check also flags C code that steps over a renamed literal by the old name's length (q += 14 after
strstr(p, "k33_survival")); such a step has to be written sizeof("k33_survival") - 1, as this branch does.
After the rename, a run
  * regenerates src/console/sh1x_cmds.h with scripts/gen_sh1x_cmds.py (the new names sort differently), when
    server/sh1x is initialised;
  * re-pins the source digests the tests keep, but only a pin that matched the file before the run (a pin that
    was already stale stays stale and still fails its test): the ui_baseline.json "files" table, and any
    whitespace-normalised digest in tests/ of a changed file, of one of its profile projections
    (tests/test_r46_native_source_integration.py gate_project) or of a function the r46 contract pins.
"""
import hashlib
import importlib.util
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SELF = pathlib.Path(__file__).resolve()

CODE_DIRS = ("src", "tests", "tools", "scripts", "zonekit")
NEVER_DIRS = ("server/", "zones/", "zone_source/", "zone_packs/", "files/", "build/", "third_party/")
BINARY_SUFFIXES = (".DATA", ".bin", ".ff", ".iwd", ".dll", ".exe", ".z", ".png", ".dds", ".jpg", ".gif",
                   ".zip", ".gz", ".ttf", ".otf", ".swf", ".gfx", ".pyc")

# ---- Kernel33's own C names ------------------------------------------------------------------------------------
C_NAMES = {
    # build profile (scripts/build-profile.sh sets them from the K33_* variables of the same name)
    "CODOL_DIAGNOSTICS": "K33_DIAGNOSTICS",
    "CODOL_LOCAL_TESTING": "K33_LOCAL_TESTING",
    "CODOL_KERNEL_ROOM_CONTEXT": "K33_KERNEL_ROOM_CONTEXT",
    "CODOL_KERNEL_NAME_DISPLAY": "K33_KERNEL_NAME_DISPLAY",
    "CODOL_OFFLINE_LOBBY": "K33_OFFLINE_LOBBY",
    # include guards
    "CODOL_GFX_DATE_TIME_GUARDS_H": "GFX_DATE_TIME_GUARDS_H",
    "CODOL_GFX_DATE_TIME_POLICY_H": "GFX_DATE_TIME_POLICY_H",
    "CODOL_KERNEL_NAME_GUARDS_H": "KERNEL_NAME_GUARDS_H",
    "CODOL_KERNEL_NAME_POLICY_H": "KERNEL_NAME_POLICY_H",
    "CODOL_KERNEL_ROOM_GUARDS_H": "KERNEL_ROOM_GUARDS_H",
    "CODOL_KERNEL_ROOM_POLICY_H": "KERNEL_ROOM_POLICY_H",
    "CODOL_P2P_ACTIVATION_TRANSACTION_H": "P2P_ACTIVATION_TRANSACTION_H",
    "CODOL_P2P_CLIENT_IDENTITY_H": "P2P_CLIENT_IDENTITY_H",
    "CODOL_P2P_EXCEPTION_POLICY_H": "P2P_EXCEPTION_POLICY_H",
    "CODOL_PRODUCTION_PROFILE_H": "PRODUCTION_PROFILE_H",
    # patch_locale_policy.h
    "CODOL_PATCH_EN": "PATCH_EN",
    "CODOL_PATCH_ZH": "PATCH_ZH",
    "CODOL_PATCH_NATIVE": "PATCH_NATIVE",
    "CODOL_LANG_NAME_MAX": "LANG_NAME_MAX",
    "CODOL_LANG_MAX": "LANG_MAX",
    "CODOL_LANG_DEFAULT": "LANG_DEFAULT",
    "CODOL_LANG_ARGV": "LANG_ARGV",
    "CODOL_LANG_ALIAS": "LANG_ALIAS",
    "CODOL_LANG_CONFIG": "LANG_CONFIG",
    "CodolPatchLanguage": "PatchLanguage",
    "CodolPatchLanguageFromArgv": "PatchLanguageFromArgv",
    "CodolPatchLanguageAliasSeen": "PatchLanguageAliasSeen",
    "CodolAsciiWideEqual": "AsciiWideEqual",
    "CodolAsciiPrefix": "AsciiPrefix",
    "CodolAsciiEqual": "AsciiEqual",
    "CodolPatchZoneRetired": "PatchZoneRetired",
    "kCodolPatchZoneMerged": "kPatchZoneMerged",
    "CodolPatchZoneMergedInto": "PatchZoneMergedInto",
    "CodolPatchZoneLocaleAllowed": "PatchZoneLocaleAllowed",
    "CodolPatchZoneLanguage": "PatchZoneLanguage",
    "CodolPatchZoneAllowedFor": "PatchZoneAllowedFor",
    "CodolLanguageCanonical": "LanguageCanonical",
    "CodolLanguageCanonicalWide": "LanguageCanonicalWide",
    "CodolLanguageZoneStem": "LanguageZoneStem",
    "CodolLocaleZoneLanguage": "LocaleZoneLanguage",
    "CodolLanguageSource": "LanguageSource",
    "CodolLanguageFromArgv": "LanguageFromArgv",
    "CodolLanguageFromConfig": "LanguageFromConfig",
    "CodolStartupLanguage": "StartupLanguage",
    "CodolLanguageListAdd": "LanguageListAdd",
    "CodolLanguageListFind": "LanguageListFind",
    "CodolLanguageListJoin": "LanguageListJoin",
    # local_multi_policy.h / local_multi.c
    "CodolLocalMultiArgument": "LocalMultiArgument",
    "CodolLocalMultiRoute": "LocalMultiRoute",
    "CodolLocalPort": "LocalPort",
    "CodolLocalTitle": "LocalTitle",
    "kCodolGameMutex": "kGameMutex",
    # rooms/managed_quick_policy.h
    "CodolManagedQuickOpen": "ManagedQuickOpen",
    "CodolManagedQuickNormalize": "ManagedQuickNormalize",
    # tests/kernel_room_policy_parity.cpp
    "CODOL_NATIVE_ROOM_FIXTURE": "NATIVE_ROOM_FIXTURE",
}

# ---- dvars / command-line switches Kernel33 registers or reads (the client's own k33_ prefix) -------------------
CLIENT_DVARS = {
    "codol_voice": "k33_voice",
    "codol_discord": "k33_discord",
    "codol_soundvoices": "k33_soundvoices",
    "codol_name_utf8": "k33_name_utf8",
    "codol_patchdir": "k33_patchdir",
    "codol_survival": "k33_survival",
    "codol_survivalXxx": "k33_survivalXxx",     # a comment's example of a longer name
    "codol_survivalx": "k33_survivalx",         # the test's longer name that must not match
    "codol_usermaps": "k33_usermaps",
    "codol_local_test_multi": "k33_local_test_multi",
}

# ---- runtime strings and environment knobs of this repository -------------------------------------------------
CLIENT_STRINGS = {
    "CODOL_PARITY_LOG": "K33_PARITY_LOG",      # parity.c startup level (environment)
    "CODOL_SNDDIAG": "K33_SNDDIAG",            # sounddiag.c startup level (environment)
    "CoDOLClassicTest": "K33LocalTest",        # local_multi.c: the per-process mutex name of a local QA client
    "CODOL_GAME": "K33_GAME",                  # scripts/build_zones.py, ui_keys.py, tests: the game folder
    "CODOL_ZZ": "K33_ZZ",                      # scripts/build_zones.py: the codol-classic-zz folder
    "CODOL_MAPS": "K33_MAPS",                  # scripts/build_zones.py: ship | all
    "CODOL_FFDEC": "K33_FFDEC",                # scripts/build_zones.py, ui_keys.py: ffdec-cli.exe
    "CODOL_ASSETEXPLORER_BIN": "K33_ASSETEXPLORER_BIN",   # tools/ui_inspector_assets.py
}

# ---- names the client forwards to (or reads from) the hosted sh1x match server ---------------------------------
# The server's rule: drop "codol_". Every name of src/console/sh1x_cmds.h that starts with codol_, plus the +set
# names p2p.c passes the host. (codol_dev_cheats -> dev_cheats is the server's existing alias of the same command.)
SERVER_CLASH = {"codol_voiceDebug": "sv_voiceDebug"}   # voiceDebug is a C1 dvar name; the server took sv_voiceDebug
# ---- step 3: console names with "codol" that the GSC scripts read or set ------------------------------------------
# (old, new, why). The GSC tree (the 13-rawfiles repository, the server's bin/home/codol_rawfiles and its native
# overlay scripts/codol), the server and this client all spell them; each of the three runs the same table:
#   * this repository: the code, the docs (step 1 rules), and any GSC/menu/config text under zones/;
#   * the GSC tree:    python tools/drop_codol_prefix.py --tree <rawfiles checkout> [--check] (case-insensitive: GSC
#                      and dvar names are), every tracked .gsc/.csc/.csv/.cfg/.txt/.menu/.json file, comments too;
#   * the server:      its tools/rename_prefix_map.json "config.gsc_names" holds the same pairs (--check compares).
# GSC script dvars keep scr_ and lose "codol_"; the others take the IW prefix of what they do. Every new name was
# checked against C1's dvars (src/c1_dvars.h), the server's registered names, the GSC trees and the stock IW3/IW4/
# IW5/T5 scripts: scr_usekillstreak is a stock IW4 _dev.gsc dvar, so the bots' one became scr_botusekillstreak.
GSC_TABLE = [
    # server session state (the server and the lobby set them; the scripts, the server and the client read them)
    ("codol_zoneType", "sv_zoneType", "asset category of the match: mp, coop, zm or survival"),
    ("codol_sessionMap", "sv_sessionMap", "the playlist/session spelling of the map (coop_training_mp_killhouse_sh)"),
    ("codol_playlistId", "sv_playlistId", "the master-selected playlist"),
    ("codol_quickPlay", "sv_quickPlay", "a Quick Play match"),
    ("codol_relayIdentityPath", "sv_relayIdentityPath", "+set by p2p.c for the hosted server"),
    ("codol_maintenance_banner", "sv_maintenanceBanner", "operator text the scripts show as a banner"),
    # AI / actors (the server registers it)
    ("codol_tracePace", "ai_tracePace", "per-frame T5 path pace trace of one actor"),
    # dog AI developer dvars (scripts only)
    ("codol_dogAttackTest", "scr_dogAttackTest", "dog attack test verbs"),
    ("codol_dogAttackLog", "scr_dogAttackLog", "dog attack log"),
    ("codol_dogAttackDiag", "scr_dogAttackDiag", "dog lunge decision trace (named in a server comment)"),
    ("codol_dogsTest", "scr_dogsTest", "MP dogs test verbs"),
    ("codol_dogsNoPathNodes", "scr_dogsNoPathNodes", "MP dogs without path nodes"),
    # GSC script dvars: scr_codol_x -> scr_x
    ("scr_codol_usekillstreak", "scr_botusekillstreak", "scr_usekillstreak is a stock IW4 _dev.gsc dvar"),
] + [("scr_codol_" + n, "scr_" + n, "GSC script dvar") for n in """
    bots botfill botmax botfill_testhumans botkillstreaks bot_objective bot_objective_debug botequipment devbot_target
    weaptest traptest topplayers topplayers_trace topplayers_hold topplayers_dolly topplayers_nowalk scavtest ridetest
    powertest perktest pathtest gibtest fftest factorytest equiptest chtest breathtest boxtest blockertest audiotest
    perklog aar_trace nuke_live helilog factory_soundlog""".split()]
GSC_NAMES = {old: new for old, new, _why in GSC_TABLE}
# the client's language switch alias (a +set on the command line creates a dvar of that name)
CLIENT_SWITCHES = {"codol_patchlang": "loc_patchLang"}
SERVER_NAMES_OLD = """
codol_actorcontact codol_admissionPath codol_admissionRequired codol_animTrace codol_animlistMask
codol_animmodelprobe codol_autoUnstuck codol_autoUnstuckInput codol_autoUnstuckMs codol_bindprofile
codol_boxprobe codol_brushprobe codol_bulletExplodeDebug codol_c1EventParity codol_c1Fixups codol_c1Footsteps
codol_c1Readers codol_checkattachments codol_checkchallenge codol_checkcharacter codol_checkcharvariant
codol_checkcollision codol_checkconstants codol_checkexperience codol_checkmodel codol_checkpmovedef
codol_checkprofile codol_checkstructured codol_checkunlock codol_checkweapon codol_checkweaponregistry
codol_checkxanim codol_clientExe codol_coopClientMapName codol_coopMapGraph codol_corpseprobe codol_debugTrace
codol_destructible codol_devServer codol_dev_cheats codol_dev_clients codol_dev_player codol_dumplocalize
codol_effectaudit codol_enemyColor codol_exitToLobby codol_explodeprobe codol_fileBackedBlocks codol_fireprobe
codol_floorprobe codol_foliageprobe codol_frameProfile codol_freeZoneImages codol_friendlyColor
codol_friendlyCrosshair codol_gibprobe codol_gpRatePercent codol_hitprobe codol_hostParentPid
codol_infectedModelZone codol_limits codol_limitstress codol_loadzone codol_loadzones codol_lookatDebug
codol_matchReportSpool codol_meleeDebug codol_modelslot codol_movementDebug codol_patchZonePath
codol_persistentBoot codol_persistentCatalogue codol_persistentEvents codol_persistentRoot codol_persistentSlot
codol_preferMapGraph codol_profilePath codol_publishExtendedPlayerState codol_pushPublicProfile
codol_pushSecureProfile codol_ragdollLaunchScale codol_recoverDelay codol_rootMotionTrace codol_securityId
codol_securityKeyPath codol_serverSurfaceGeometry codol_sessionStatePath codol_setpos
codol_settleRestingPrediction codol_shadowcompare codol_shieldBlastDebug codol_solidprobe codol_soundTrace
codol_sourceRconRemote codol_sweepprobe codol_theatreAutoRecord codol_theatreMaxSegments codol_theatreSegmentMiB
codol_theatreSegmentSeconds codol_token_consumed codol_token_inventory codol_traceActorEvents
codol_traceRagdollLaunch codol_traceWeaponLock codol_unstuck codol_voiceDebug codol_voiceEcho
codol_voiceMaxPackets codol_warmAdmissionPath codol_warmPreparation codol_worldgunprobe codol_xpRatePercent
codol_zonePath 
codol_oneShotMatch codol_pmemMegs codol_scriptPath codol_scriptOverlay
codol_scriptNativeOverlay
""".split()
SERVER_NAMES = {old: SERVER_CLASH.get(old, old[len("codol_"):]) for old in SERVER_NAMES_OLD if old not in GSC_NAMES}

# ---- step 2: Kernel33's k33 console names -> IW engine prefixes by function ----------------------------------
# (old, new, kind, why). kind: dvar (registered; a saved value of the old name is migrated, src/dvar_renames.h),
# cmd (console command / console built-in), switch (read from the command line only, never registered).
# A name compiled into a shipped UI movie (zones/*.gfxpatch: tags and .as) is NOT here: renaming it needs a new
# verified composition stage of that movie (tools/*-composition, tests/test_optionview_method.py), see KEEP_K33.
# A name that is not in the tree is a no-op (names of branches still being written are listed ahead).
IW_TABLE = [
    # client-game: HUD and drawing
    ("k33_hudFireType", "cg_hudFireType", "dvar", "HUD style item text (menus read it)"),
    ("k33_hudAssists", "cg_hudAssists", "dvar", "HUD style item value (menus read it)"),
    ("k33_hudClip", "cg_hudClip", "dvar", "HUD style item (HUD branch)"),
    ("k33_hudClipRounds", "cg_hudClipRounds", "dvar", "HUD style item (HUD branch)"),
    ("k33_hudSway", "cg_hudSway", "dvar", "HUD sway (HUD branch; refused while a movie row reads it)"),
    ("k33_hudSwayX", "cg_hudSwayX", "dvar", "HUD sway (HUD branch)"),
    ("k33_hudSwayY", "cg_hudSwayY", "dvar", "HUD sway (HUD branch)"),
    ("k33_drawFPS", "cg_drawFramerate", "dvar", "cg_drawFPS is C1 dvar 591 and an IW4 dvar"),
    ("k33_drawViewpos", "cg_drawViewOrigin", "dvar", "cg_drawViewpos is a C1 dvar"),
    ("k33_name_utf8", "cl_utf8Names", "switch", "player names read as UTF-8 (client)"),
    # client: voice chat (IW's cl_voice family; cl_voice itself is a C1 dvar)
    ("k33_voice", "cl_voiceChat", "dvar", "voice chat master switch; cl_voice is a C1 dvar"),
    ("k33_voiceBitrate", "cl_voiceBitrate", "dvar", "voice chat"),
    ("k33_voiceLoopback", "cl_voiceLoopback", "dvar", "voice chat"),
    ("k33_voiceDebug", "cl_voiceDebug", "dvar", "voice chat"),
    ("k33_voiceTone", "cl_voiceTone", "dvar", "voice chat"),
    ("k33_voiceHangMs", "cl_voiceHangMs", "dvar", "voice chat"),
    ("k33_voiceInputDevice", "cl_voiceInputDevice", "dvar", "voice chat"),
    ("k33voice", "cl_voiceStatus", "cmd", "voice chat"),
    ("k33voicemute", "cl_voiceMute", "cmd", "voice chat"),
    ("k33_discord", "cl_discord", "dvar", "Discord presence (client)"),
    ("k33mouse", "cl_fakeMouse", "cmd", "test input (client); gpad_testmouse is another command"),
    # sound
    ("k33_soundvoices", "snd_voiceLimits", "switch", "sound voice allocation fixes"),
    ("k33_sndAliasLimit", "snd_aliasLimit", "dvar", "sound voice allocation"),
    ("k33_sndNearest", "snd_aliasNearest", "dvar", "sound voice allocation"),
    ("k33_snddiag", "snd_diag", "dvar", "sound diagnostics"),
    # gamepad (Kernel33's existing gpad_ family, as IW4's console builds)
    ("k33_glyphinfo", "gpad_glyphInfo", "dvar", "button glyphs"),
    ("k33_glyphSerial", "gpad_glyphSerial", "dvar", "button glyphs"),
    ("k33_reserved", "gpad_reserved", "cmd", "placeholder name of an unused bind slot"),
    # console
    ("k33_consoleKey", "con_toggleKey", "dvar", "console"),
    ("k33_rconPort", "con_rconPort", "dvar", "the console's rcon route"),
    ("k33help", "con_help", "cmd", "console built-in"),
    ("k33dev", "con_status", "cmd", "console built-in: its lists and the server route"),
    ("k33shot", "con_shot", "cmd", "console built-in (diagnostics)"),
    # renderer
    ("k33_screenshot", "r_screenshot", "cmd", "diagnostics screenshot; C1 has screenshot"),
    ("k33_shotFile", "r_shotFile", "switch", "diagnostics screenshot requests"),
    # common / engine
    ("k33_cmdFile", "com_cmdFile", "switch", "headless command-file mode"),
    ("k33_framestats", "com_frameStats", "cmd", "frame timing (diagnostics)"),
    ("k33logflush", "com_logFlush", "cmd", "local QA log flush"),
    ("k33_local_test_multi", "com_localTestMulti", "switch", "several local test clients"),
    # filesystem
    ("k33_patchdir", "fs_patchDir", "switch", "the patch zone folder"),
    ("k33_usermaps", "fs_usermaps", "switch", "usermaps folder"),
    # menus / front end
    ("k33_survival", "ui_survival", "switch", "the Survival room category"),
    ("k33_survivalXxx", "ui_survivalXxx", "switch", "a comment's example of a longer name"),
    ("k33_survivalx", "ui_survivalx", "switch", "the test's longer name that must not match"),
    ("k33ready", "ui_pressReady", "cmd", "test automation: the room's Ready button"),
    # language (test-build commands and switches; the player-facing ones are in the movie, KEEP_K33)
    ("k33_langstat", "loc_tableStats", "cmd", "language switch diagnostics"),
    ("k33_langcycle", "loc_cycle", "cmd", "language switch test"),
    ("k33_langforce", "loc_forceUnload", "cmd", "language switch test"),
    ("k33_langNoReorder", "loc_noReorder", "switch", "language switch test"),
    # network / server hosting
    ("k33_hostGate", "net_hostGate", "switch", "the loopback-only UDP gate"),
    ("k33_p2pHost", "sv_p2pHost", "switch", "host a match at startup"),
]
IW_NAMES = {old: new for old, new, _kind, _why in IW_TABLE}

# ---- step 3: the sh1x server repository's renamed files, folders and functions (its tools/rename_prefix_map.json
# "files" / "identifiers" and build outputs), as this repository's build script, comments and docs name them.
SERVER_REFS = {
    "src/codol/net/codol_network.c": "src/online/net/network.c",
    "src/codol/net/codol_voice.c": "src/online/net/voice.c",
    "src/codol/player/codol_dev_cheats.c": "src/online/player/dev_cheats.c",
    "src/codol/snapshot/codol_player_fields.inc": "src/online/snapshot/player_fields.inc",
    "src/codol/stats/codol_scoreboard_wire.c": "src/online/stats/scoreboard_wire.c",
    "src/codol/weapon/codol_weapon_state.h": "src/online/weapon/weapon_state.h",
    "src/codol/": "src/online/",
    "tests/test_codol_vote_guard.c": "tests/test_vote_guard.c",
    "tools/codol_discord_presence.py": "tools/discord_presence.py",
    "tools/codol_english/build_ff.py": "tools/english/build_ff.py",
    "tools/codol_english/build_font_ff.py": "tools/english/build_font_ff.py",
    "tools/codol_english/gfx_font.py": "tools/english/gfx_font.py",
    "tools/codol_map_handoff.ps1": "tools/ol_map_handoff.ps1",
    "tools/codol_stage_server_pack.py": "tools/stage_server_pack.py",
    "tools/codol_stringtable_dump.py": "tools/stringtable_dump.py",
    "bin/codol_dedrun.exe": "bin/sh1x_dedrun.exe",
    "codol_dedrun.exe": "sh1x_dedrun.exe",
    "codol_dedrun": "sh1x_dedrun",
    "codol_asset_host.h": "asset_host.h",
    "codol_bot_loadout.c": "bot_loadout.c",
    "codol_c1_weapon.c": "c1_weapon.c",
    "codol_dev_cheats.c": "dev_cheats.c",
    "codol_ecdh.c": "ol_ecdh.c",
    "codol_game.c": "game.c",
    "codol_load_blocks.c": "load_blocks.c",
    "codol_match_report.c": "match_report.c",
    "codol_network.c": "network.c",
    "codol_remote_missile.c": "remote_missile.c",
    "codol_voice.c": "voice.c",
    "CODOL_AdmissionMember": "AdmissionMember",
    "CODOL_BeginZoneReplay": "BeginZoneReplay",
    "CODOL_CheatsPermitted": "CheatsPermitted",
    "CODOL_DevClientCommand": "DevClientCommand",
    "CODOL_DevPrint": "DevPrint",
    "CODOL_LocationSelectionAction": "LocationSelectionAction",
    "CODOL_NetworkSend": "NetworkSend",
    "CODOL_ReadXModelBounds": "ReadXModelBounds",
    "CODOL_VoicePacket": "VoicePacket",
    "CODOL_VoteArgIsSafe": "VoteArgIsSafe",
    "CODOL_VoteNameIsSafe": "VoteNameIsSafe",
    "CODOL_NetworkAdmission": "NetworkAdmission",
    "CODOL_RefreshServerPlayerNames": "RefreshServerPlayerNames",
}
IW_DVARS = sorted((old, new) for old, new, kind, _why in IW_TABLE if kind == "dvar")

MAPPING = {}
for table in (C_NAMES, CLIENT_DVARS, CLIENT_STRINGS, SERVER_NAMES, IW_NAMES, SERVER_REFS, GSC_NAMES, CLIENT_SWITCHES):
    for old, new in table.items():
        assert old not in MAPPING, old
        MAPPING[old] = new
# one pass reaches the final name (codol_voice -> k33_voice -> cl_voiceChat)
for old in list(MAPPING):
    seen = {old}
    while MAPPING[old] in MAPPING:
        assert MAPPING[old] not in seen, old
        seen.add(MAPPING[old])
        MAPPING[old] = MAPPING[MAPPING[old]]
assert not set(MAPPING.values()) & set(MAPPING), "a new name is also an old one"

# ---- what stays, and why ------------------------------------------------------------------------------------
# Exact names (case-sensitive). The --check/--report scan looks at every [A-Za-z0-9_]*codol[A-Za-z0-9_]* word
# (any case) in the code; a word followed by a file extension is reported as a file name of another repository
# or of an install. Patch zone names are also taken from zones/, zone_source/ and zone_packs/ (see zone_names()).
REASON_ZONE = "patch zone / fastfile name (a file name in the installs, loaded by name)"
REASON_LEGACY_ZONE = ("an old install's zone name on a line marked LEGACY_ZONE (kPatchZoneMerged, the old-name rules "
                      "and their tests, a zone's origin file): what an existing install still carries")
REASON_ZONE_TEST = "a zone name a test or policy feeds the zone-name rules (codol_<lang>_locale family)"
REASON_MOVIE = "dvar name compiled into the shipped UI movies (codo.gfx / codoingame.gfx tags under zones/)"
REASON_SERVER_REPO = "file, function or folder of the sh1x server repository (renamed there, not here)"
REASON_LAUNCHER = "environment variable / pipe / token of the launcher-managed session protocol (external launcher)"
REASON_PYLOBBY = "environment switch of the external Python lobby (CODOL-EMU), named in ports/tests of it"
REASON_EXTERNAL = "external tool or assembly name (CODOL-AssetExplorer, codol-classic, Python lobby, Discord)"
REASON_LOCALIZE = "localize key inside the shipped zones' localize.str / stringtables"
REASON_INSTALL = "file name inside existing installs"
REASON_PROSE = "product name in prose (CoDOL = the game, Call of Duty Online)"
REASON_HISTORY = "removed online feature, named only in a comment"
REASON_PATH = "folder name on disk (an install or build folder: CODOL Builds, CODOL_2.6.9.7, a local CODOL server tree)"
REASON_PENDING = "server GSC dvar with codol inside the name (not a codol_ prefix); follows the server repo"
KEEP = {
    # names compiled into the UI movies
    "codol_borderless": REASON_MOVIE, "codol_quick_native_room_context": REASON_MOVIE,
    "codol_quick_lineage": REASON_MOVIE, "codol_quick_return": REASON_MOVIE,
    "codol_quick_return_host_false": REASON_MOVIE, "codol_survival_direct_home": REASON_MOVIE,
    "codol_training_room_context": REASON_MOVIE, "codol_theatre_request": REASON_MOVIE,
    "codol_theatre_count": REASON_MOVIE, "codol_theatre_id_i": REASON_MOVIE, "codol_theatre_open": REASON_MOVIE,
    "codol_theatre_playback": REASON_MOVIE, "codol_theatre_": REASON_MOVIE, "codol_favorites_": REASON_MOVIE,
    # zone-name families in code and tests
    "codol_": REASON_ZONE, "codol_zombies_endless": REASON_ZONE,
    "codol_en_locale": REASON_ZONE_TEST, "codol_fr_locale": REASON_ZONE_TEST, "codol_de_locale": REASON_ZONE_TEST,
    "codol_a_b_locale": REASON_ZONE_TEST, "codol__locale": REASON_ZONE_TEST, "codol_native_locale": REASON_ZONE_TEST,
    "xcodol_fr_locale": REASON_ZONE_TEST, "codol_fr_locale_extra": REASON_ZONE_TEST,
    "codol_chinese_extra": REASON_ZONE_TEST, "codol_english_ui_extra2": REASON_ZONE_TEST,
    "codol_loading_mode_names_extra": REASON_ZONE_TEST, "codol_zombies_endless_extra": REASON_ZONE_TEST,
    "codol_englishui": REASON_ZONE_TEST, "codol_chineseui": REASON_ZONE_TEST,
    "codologin": "Scaleform movie name inside the shipped zones (codologin.gfx)",
    # sh1x server repository (files, functions, folders)
    "codol_rawfiles": REASON_SERVER_REPO, "CODOL_DevCheats": REASON_SERVER_REPO,
    "CODOL_C1_VIRTUALIZED_20260928": REASON_SERVER_REPO,
    "CODOL_C1_MOVEMENT_LAYOUT": REASON_SERVER_REPO,
    # launcher-managed session protocol
    "CODOL_VERIFIED_SCRIPTS_REQUIRED": REASON_LAUNCHER, "CODOL_VERIFIED_SCRIPT_PIPE": REASON_LAUNCHER,
    "CODOL_VERIFIED_SCRIPT_PARENT_PID": REASON_LAUNCHER, "CODOL_LOCAL_LOG_IDENTITY_PIPE": REASON_LAUNCHER,
    "CoDOLVerifiedScripts": REASON_LAUNCHER, "CODOL_P2P_PRODUCTION_PIPE": REASON_LAUNCHER,
    "CODOL_P2P_PRODUCTION_PARENT_HANDLE": REASON_LAUNCHER, "CODOL_P2P_PRODUCTION_SESSION_CEILING": REASON_LAUNCHER,
    "CODOL_P2P_STARTUP_IPC_V1": REASON_LAUNCHER, "CODOL_LOCAL_QA_DWAUTH_HANDLE": REASON_LAUNCHER,
    "CODOL_QA_BACKGROUND": REASON_LAUNCHER, "CODOL_QA_BACKGROUND_PROPERTY": REASON_LAUNCHER,
    "CODOL_QA_BG_": REASON_LAUNCHER, "CODOL_QA_BG_0123456789abcdef0123456789abcdef": REASON_LAUNCHER,
    "CODOL_QA_BG_0123456789abcdef0123456789abcdeG": REASON_LAUNCHER,
    # the external Python lobby's switches
    **{name: REASON_PYLOBBY for name in (
        "CODOL_OFFLINE", "CODOL_LOBBY_HOST", "CODOL_STATE_DIR", "CODOL_MARKETPLACE_BALANCE", "CODOL_MAP_POLICY",
        "CODOL_MAP_PREFER", "CODOL_MAP_TYPES", "CODOL_MAP_ROTATE", "CODOL_JOIN_FIX", "CODOL_ADMISSION",
        "CODOL_ADMISSION_PORT", "CODOL_INSTANCE_PORTS", "CODOL_INSTANCE_SPAWN", "CODOL_INSTANCES",
        "CODOL_MATCH_RCON_PORT", "CODOL_MATCH_RCON_HOST", "CODOL_MATCH_RCON_PASSWORD", "CODOL_MM_HOST_PORT",
        "CODOL_ZONE_DIR", "CODOL_SELECTION_STATE", "CODOL_ROTATION_STATE", "CODOL_HANDOFF_FILE", "CODOL_LOBBY_LOG",
        "CODOL_GAME_DIR", "CODOL_USERMAPS_DIR", "CODOL_RANK_WRITEBACK", "CODOL_PER_UID", "CODOL_LOBBY_DB",
        "CODOL_JOINREQ_RELAY_MINGAP", "CODOL_ROOM_FANOUT", "CODOL_SETMAP_JOINRESP", "CODOL_INV_PUSH",
        "CODOL_MP_PURCHASE_RECEIPT", "CODOL_", "CODOL_THEATRE_SESSION_V1")},
    # external tools / products
    "CodolSoundDumper": REASON_EXTERNAL, "Codol": REASON_EXTERNAL, "CoDOLPresence": REASON_EXTERNAL,
    "codolpresence": REASON_EXTERNAL, "CODOL_2697_Launcher": REASON_EXTERNAL, "CODOL_2": REASON_PATH,
    "codol_cfgcrypt": REASON_EXTERNAL, "codol2697": REASON_EXTERNAL, "CoDOL2": REASON_EXTERNAL,
    # localize keys
    **{name: REASON_LOCALIZE for name in (
        "CODOL_MODE_GUN_NAME", "CODOL_MODE_GUN_DESC", "CODOL_MODE_INFECT_NAME", "CODOL_MODE_INFECT_DESC",
        "CODOL_MODE_OIC_NAME", "CODOL_MODE_OIC_DESC", "CODOL_MODE_SHRP_NAME", "CODOL_MODE_SHRP_DESC",
        "CODOL_LOADING_MODE_GUN", "CODOL_LOADING_MODE_INFECT", "CODOL_LOADING_MODE_OIC",
        "CODOL_LOADING_MODE_SHRP", "CODOL_LOADING_MODE_")},
    # files in installs, compatibility, prose, history
    "codol_video_mode": REASON_INSTALL,
    "CoDOL": REASON_PROSE, "CODOL": REASON_PROSE, "codol": REASON_PROSE, "sCODOL": REASON_PATH,
    "CODOL_ZOMBIES_ENDLESS": REASON_ZONE_TEST, "CODOL_LOADING_MODE_NAMES": REASON_ZONE_TEST,
    "CODOL_CHINESE_LOCALE": REASON_ZONE_TEST,
    "codol_name_display_url": REASON_HISTORY, "codol_theatre_url": REASON_HISTORY,
    "drop_codol_prefix": "this rename tool (scripts/gen_sh1x_cmds.py and tests/test_offline_console.py import it)",
}

# ---- k33 names that stay, and why (the step-2 check looks at k33 words inside C string literals) -------------
REASON_K33_MOVIE = ("read or set by name inside a shipped UI movie (zones/*.gfxpatch .as and DoABC tags): a rename "
                    "needs a new verified composition stage of codo.gfx / codoingame.gfx / codologin.gfx")
REASON_K33_FILE = "file or folder name (inside the install or its main folder), not a console name"
REASON_K33_ASSET = "asset or ActionScript package name inside the shipped zones"
REASON_K33_TOKEN = "runtime token, not a console name"
KEEP_K33 = {
    **{name: REASON_K33_MOVIE for name in (
        "k33_hudStyle", "k33_hudStyleList", "k33_english", "k33_language", "k33_languageList", "k33_languageState",
        "k33_languageSerial", "k33_setlanguage", "k33_padinfo", "k33_padbinds", "k33_padcapture", "k33_padclear",
        "k33nav", "k33_snapscan", "k33_snapn", "k33_snaperr", "k33_snapprobe", "k33_snapat", "k33_snap",
        "k33_snaphid", "k33_snapcls", "k33_snap0", "k33_snap1", "k33_snap2", "k33_snap3", "k33_snap4", "k33_snap5",
        "k33_snap6", "k33_snap7", "k33_snapcls0", "k33_snapcls1", "k33_snapcls2", "k33_snapcls3", "k33_snapcls4",
        "k33_snapcls5", "k33_snapcls6", "k33_snapcls7", "k33_snaphid0", "k33_snaphid1", "k33_snaphid2",
        "k33_snaphid3", "k33_voiceVolume", "k33_voiceThreshold", "k33_voiceMode", "k33_voiceMicMute",
        "k33_voiceLevel", "k33_voiceMeter", "k33_voiceInputList", "k33_voiceInputPick", "k33_voiceInputIndex")},
    **{name: REASON_K33_FILE for name in (
        "k33_cmd_", "k33_console", "k33_console_history", "k33_offline", "k33_rcon", "k33_shot", "k33_stats",
        "k33patch", "k33p2p_dedrun", "k33sha256", "k33tick")},
    "k33pad_": REASON_K33_ASSET, "k33": REASON_K33_ASSET,
    "k33rpc": REASON_K33_TOKEN,
}
K33_WORD = re.compile(r'(?<![A-Za-z0-9_])k33_?[a-z][A-Za-z0-9_]*|(?<![A-Za-z0-9_])k33(?![A-Za-z0-9_])')
GENERATED = "src/dvar_renames.h"
MOVIE_MARK = ".gfxpatch/"

LEFT = r'(?:(?<![A-Za-z0-9_])|(?<=-D)|(?<=\^[0-9]))'
RIGHT = r'(?![A-Za-z0-9_])(?!\.[A-Za-z])'
_PATTERN = re.compile(LEFT + '(' + '|'.join(re.escape(k) for k in sorted(MAPPING, key=lambda s: (-len(s), s)))
                      + ')' + RIGHT)
WORD = re.compile(r'[A-Za-z0-9_]*[Cc][Oo][Dd][Oo][Ll][A-Za-z0-9_]*')


def rename(text):
    """text with every mapped name replaced; (new text, number of replacements)."""
    return _PATTERN.subn(lambda m: MAPPING[m.group(1)], text)


SERVER_VOCAB = dict(SERVER_NAMES, **GSC_NAMES)


def server_name(name):
    """A name the server source registers, in the client's (post-rename) vocabulary."""
    return SERVER_VOCAB.get(name, name)


def server_text(text):
    """Server source text with its codol_ command/dvar names spelled as the client spells them."""
    pattern = re.compile(LEFT + '(' + '|'.join(re.escape(k) for k in sorted(SERVER_VOCAB, key=lambda s: (-len(s), s)))
                         + ')' + RIGHT)
    return pattern.sub(lambda m: SERVER_VOCAB[m.group(1)], text)


# ---- step 3 on another checkout: the GSC tree -------------------------------------------------------------------
TREE_SUFFIXES = (".gsc", ".csc", ".gsh", ".csv", ".cfg", ".txt", ".menu", ".inc", ".json", ".md", ".atr", ".script")
_GSC_PATTERN = re.compile(r'(?<![A-Za-z0-9_])(' + '|'.join(re.escape(k) for k in sorted(GSC_NAMES, key=lambda s: (-len(s), s)))
                          + r')(?![A-Za-z0-9_])(?!\.[A-Za-z])', re.I)
_GSC_LOWER = {k.lower(): v for k, v in GSC_NAMES.items()}


def gsc_rename(text):
    """text with every GSC_TABLE name (any case) replaced; (new text, number of replacements)."""
    return _GSC_PATTERN.subn(lambda m: _GSC_LOWER[m.group(1).lower()], text)


def tree(root, check_only):
    """Apply (or, check_only, count) GSC_TABLE in the tracked text files of another checkout (a GSC tree)."""
    root = pathlib.Path(root).resolve()
    out = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], capture_output=True, check=True).stdout
    total, changed = 0, []
    for rel in sorted(r for r in out.decode("utf-8").split("\0") if r):
        path = root / rel
        if not rel.lower().endswith(TREE_SUFFIXES) or not path.is_file():
            continue
        data = path.read_bytes()
        if b"\0" in data:
            continue
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            text = data.decode("latin-1")
        new, n = gsc_rename(text)
        if n:
            total += n
            changed.append((rel, n))
            if not check_only:
                path.write_bytes(encode(new, data))
    for rel, n in changed:
        print("%5d  %s" % (n, rel))
    print("%d %s in %d files of %s" % (total, "name(s) still to replace" if check_only else "replacements", len(changed),
                                       root))
    return 1 if check_only and total else 0


def tracked_files():
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z"], capture_output=True, check=True).stdout
    files = []
    for rel in out.decode("utf-8").split("\0"):
        if not rel or rel.startswith(NEVER_DIRS) or rel.endswith(BINARY_SUFFIXES):
            continue
        path = ROOT / rel
        if path.resolve() == SELF or rel == GENERATED or not path.is_file():
            continue
        if rel.split("/")[0] in CODE_DIRS or rel.endswith(".md"):
            files.append(rel)
    return sorted(files)


def read(rel):
    data = (ROOT / rel).read_bytes()
    if b"\0" in data:
        return None, None
    try:
        return data, data.decode("utf-8")
    except UnicodeDecodeError:
        return data, data.decode("latin-1")


def encode(text, original):
    try:
        original.decode("utf-8")
        return text.encode("utf-8")
    except UnicodeDecodeError:
        return text.encode("latin-1")


def zone_names():
    names = set()
    for d in ("zones", "zone_source", "zone_packs"):
        base = ROOT / d
        if base.is_dir():
            for p in base.iterdir():
                names.add(p.stem if p.is_file() else p.name)
    return names


def remaining(files):
    """{word: {reason: count}} for every codol word left in the code directories."""
    zones = zone_names()
    found = {}
    for rel in files:
        if rel.split("/")[0] not in CODE_DIRS:
            continue
        _, text = read(rel)
        if text is None:
            continue
        for m in WORD.finditer(text):
            word = m.group(0)
            after = text[m.end():m.end() + 2]
            line = text[text.rfind(chr(10), 0, m.start()) + 1:text.find(chr(10), m.end()) % (len(text) + 1)]
            if word in ZONE_WORDS:
                reason = REASON_LEGACY_ZONE if LEGACY_MARK in line else "STILL MAPPED (zone name)"
            elif word in MAPPING:
                reason = "STILL MAPPED (not replaced: inside a longer word or a file name?)"
                if len(after) == 2 and after[0] == "." and after[1].isalpha():
                    reason = "file name (another repository or an install)"
            elif word in KEEP:
                reason = KEEP[word]
            elif word in zones:
                reason = REASON_ZONE
            elif len(after) == 2 and after[0] == "." and after[1].isalpha():
                reason = "file name (another repository or an install)"
            else:
                reason = "UNCLASSIFIED"
            found.setdefault(word, {}).setdefault(reason, 0)
            found[word][reason] += 1
    return found


def baseline_pins():
    path = ROOT / "ui_baseline.json"
    return path, json.loads(path.read_text(encoding="utf-8")).get("files", {})


R46 = "tests/test_r46_native_source_integration.py"
PROFILE_MACROS = ("CODOL_DIAGNOSTICS", "CODOL_LOCAL_TESTING", "CODOL_KERNEL_ROOM_CONTEXT", "CODOL_KERNEL_NAME_DISPLAY",
                  "CODOL_OFFLINE_LOBBY")
HEX64 = re.compile(r'(?<![0-9a-f])[0-9a-f]{64}(?![0-9a-f])')


def normalised_digest(text):
    """The digest the source tests pin: sha256 of the text with every run of white space made one space."""
    return hashlib.sha256(" ".join(text.split()).encode()).hexdigest()


def r46_helpers():
    """function() / gate_project() / CONTRACT of the r46 source test, loaded from the tree as it is now."""
    path = ROOT / R46
    if not path.is_file():
        return None
    try:
        spec = importlib.util.spec_from_file_location("_r46_pin_helpers", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    except Exception as e:  # a branch whose test differs: fall back to whole-file digests only
        print("note: %s not loaded (%s); only whole-file digests are re-pinned" % (R46, e))
        return None


def digest_pairs(old_texts, new_texts, helpers):
    """{old digest: new digest} of every changed file, its profile projections and its r46-pinned functions."""
    pairs = {}

    def add(old, new):
        if old != new:
            pairs.setdefault(old, new)

    contract = getattr(helpers, "CONTRACT", {}).get("function_whitespace_hashes", {}) if helpers else {}
    for rel, old in old_texts.items():
        new = new_texts[rel]
        add(normalised_digest(old), normalised_digest(new))
        if not helpers or not rel.endswith((".c", ".h", ".cpp", ".inc")):
            continue
        # the profile macros under their old names (a first run) and under their new names (a later step)
        for macro in PROFILE_MACROS + tuple(MAPPING[m] for m in PROFILE_MACROS):
            for value in (False, True):
                try:
                    add(normalised_digest(helpers.gate_project(old, macro, value)),
                        normalised_digest(helpers.gate_project(new, MAPPING.get(macro, macro), value)))
                except Exception:
                    pass
        for name in contract.get(rel, {}):
            try:
                add(normalised_digest(helpers.function(old, name)),
                    normalised_digest(helpers.function(new, MAPPING.get(name, name))))
            except Exception:
                pass
    return pairs


def apply():
    conflicts = movie_conflicts()
    if conflicts:
        for old, rel in conflicts:
            print("refused: %s is spelled by the UI movie %s; move it to KEEP_K33 or rebuild the movie" % (old, rel))
        return 1
    files = tracked_files()
    path, pins = baseline_pins()
    before = {rel: hashlib.sha256((ROOT / rel).read_bytes()).hexdigest() for rel in pins if (ROOT / rel).is_file()}
    helpers = r46_helpers()   # loaded before the rename: its helpers do not depend on the names
    changed, total, old_texts, new_texts = [], 0, {}, {}
    for rel in files:
        data, text = read(rel)
        if text is None:
            continue
        new, n = rename(text)
        if n:
            (ROOT / rel).write_bytes(encode(new, data))
            changed.append((rel, n))
            old_texts[rel], new_texts[rel] = text, new
            total += n
    changed += apply_menus()
    zone_texts, moved = apply_zones()
    for rel, (old, new) in zone_texts.items():
        if rel not in new_texts:
            old_texts[rel] = old
        new_texts[rel] = new
        if rel not in [r for r, _ in changed]:
            changed.append((rel, zone_text(old)[1]))
    for rel, new_rel in sorted(moved.items()):
        print("moved  %s -> %s" % (rel, new_rel))
    header = renames_header()
    if not (ROOT / GENERATED).is_file() or (ROOT / GENERATED).read_bytes() != header:
        (ROOT / GENERATED).write_bytes(header)
        changed.append((GENERATED, 0))
    for rel, n in changed:
        print("%5d  %s" % (n, rel))
    print("%d replacements in %d files" % (total + sum(n for r, n in changed if r not in old_texts), len(changed)))
    if not changed and not moved:
        return 0
    # the generated console table: the new names sort differently
    if (ROOT / "server" / "sh1x" / "src" / "g_cmds.c").is_file():
        before_header = (ROOT / "src/console/sh1x_cmds.h").read_bytes()
        subprocess.run([sys.executable, "-B", str(ROOT / "scripts" / "gen_sh1x_cmds.py")], check=True)
        if (ROOT / "src/console/sh1x_cmds.h").read_bytes() != before_header and \
                "src/console/sh1x_cmds.h" not in [r for r, _ in changed]:
            changed.append(("src/console/sh1x_cmds.h", 0))
    else:
        print("note: server/sh1x is not initialised; run python scripts/gen_sh1x_cmds.py once it is")
    # source digests pinned in tests/
    pairs = digest_pairs(old_texts, new_texts, helpers)
    for rel in old_texts:   # whole-file sha256 pins too (a zone source file's digest)
        data = (ROOT / rel).read_bytes()
        old_raw = hashlib.sha256(encode(old_texts[rel], data)).hexdigest()
        if old_raw != hashlib.sha256(data).hexdigest():
            pairs.setdefault(old_raw, hashlib.sha256(data).hexdigest())
    repinned_tests = []
    for rel in files:
        if not rel.startswith("tests/"):
            continue
        data, text = read(rel)
        if text is None:
            continue
        new, n = HEX64.subn(lambda m: pairs.get(m.group(0), m.group(0)), text)
        if new != text:
            (ROOT / rel).write_bytes(encode(new, data))
            repinned_tests.append(rel)
    # ui_baseline.json "files" (its path keys were renamed with the files the zone step moved)
    _, pins = baseline_pins()
    pin_text = path.read_text(encoding="utf-8")
    repinned = []
    origin = {new_rel: rel for rel, new_rel in moved.items()}
    for rel in sorted(set(r for r, _ in changed) | set(repinned_tests)):
        if rel in pins and before.get(origin.get(rel, rel)) == pins[rel]:
            new_hash = hashlib.sha256((ROOT / rel).read_bytes()).hexdigest()
            old_line = '"%s": "%s"' % (rel, pins[rel])
            if new_hash != pins[rel] and pin_text.count(old_line) == 1:
                pin_text = pin_text.replace(old_line, '"%s": "%s"' % (rel, new_hash))
                repinned.append(rel)
    if repinned:
        path.write_bytes(pin_text.encode("utf-8"))
    print("re-pinned digests in: %s" % (", ".join(repinned_tests) or "none"))
    print("re-pinned in ui_baseline.json: %s" % (", ".join(repinned) or "none"))
    return 0


def all_tracked():
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z"], capture_output=True, check=True).stdout
    return [rel for rel in out.decode("utf-8").split("\0") if rel and (ROOT / rel).is_file()]


def menu_files():
    """The native menu sources and their generated listings under zones/ (step-2 names only: zone names stay)."""
    return sorted(rel for rel in all_tracked() if rel.startswith("zones/") and MOVIE_MARK not in rel and
                  (("/menufile/" in rel and rel.endswith((".json", ".txt"))) or
                   ("/sources/" in rel and rel.endswith(".menu"))))


_IW_PATTERN = re.compile(LEFT + '(' + '|'.join(re.escape(k) for k in sorted(IW_NAMES, key=lambda s: (-len(s), s)))
                         + ')' + RIGHT)


def movie_conflicts():
    """[(old name, movie file)] for every step-2 name a shipped UI movie spells: those must not be renamed here."""
    hits = []
    for rel in all_tracked():
        if MOVIE_MARK not in rel:
            continue
        data = (ROOT / rel).read_bytes()
        for old in IW_NAMES:
            if re.search(rb'(?<![A-Za-z0-9_])' + re.escape(old.encode()) + rb'(?![A-Za-z0-9_])', data):
                hits.append((old, rel))
    return hits


def renames_header():
    lines = ["/* Generated by tools/drop_codol_prefix.py - do not edit (the rename tool never rewrites this file).",
             " * Kernel33 dvars renamed from the k33_ prefix to IW engine names. C1 executes config_mp.cfg before",
             " * Kernel33 registers its dvars, so a saved \"seta <old> <value>\" leaves a string dvar of the old name;",
             " * C1MigrateRenamed (c1game.c) copies its value to the new name once and empties the old one. */",
             "#ifndef K33_DVAR_RENAMES_H", "#define K33_DVAR_RENAMES_H", "",
             "static const char *const kDvarRenames[][2] = {"]
    lines += ['    { "%s", "%s" },' % (new, old) for old, new in sorted(IW_DVARS, key=lambda p: p[1].lower())]
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines).encode("ascii")


def k33_remaining():
    """{word: {reason: count}} of k33 console-like words inside C string literals of src/ and tests/."""
    found = {}
    for rel in tracked_files():
        if not rel.startswith(("src/", "tests/")) or not rel.endswith((".c", ".h", ".cpp", ".inc")):
            continue
        _, text = read(rel)
        if text is None:
            continue
        for line in text.splitlines():
            s = line.replace('\\"', '')
            for m in K33_WORD.finditer(s):
                if s[:m.start()].count('"') % 2 != 1:
                    continue
                word = m.group(0)
                after = s[m.end():m.end() + 2]
                if len(after) == 2 and after[0] == "." and after[1].isalpha():
                    reason = "file name (another repository or an install)"
                elif word in MAPPING:
                    reason = "STILL MAPPED"
                elif word in KEEP_K33:
                    reason = KEEP_K33[word]
                else:
                    reason = "UNCLASSIFIED"
                found.setdefault(word, {}).setdefault(reason, 0)
                found[word][reason] += 1
    return found


def apply_menus():
    """Step-2 names in the native menu sources; the stored .bin menulists are re-encoded from their .json."""
    changed = []
    for rel in menu_files():
        data, text = read(rel)
        new, n = _IW_PATTERN.subn(lambda m: IW_NAMES[m.group(1)], text)
        if n:
            (ROOT / rel).write_bytes(encode(new, data))
            changed.append((rel, n))
            if rel.endswith(".json") and (ROOT / (rel[:-5] + ".bin")).is_file():
                sys.path.insert(0, str(ROOT))
                from zonekit import menus
                bin_path = ROOT / (rel[:-5] + ".bin")
                bin_path.write_bytes(menus.encode_menulist(json.loads(new)))
                changed.append((rel[:-5] + ".bin", 0))
    return changed


# ---- step 4: the fastfile (zone) names ---------------------------------------------------------------------------
# (old, new, why). "codol" is implied by the install, so it is dropped. No new name is a stock 1.1.1.13 zone
# (client_shipRetail\chinese: common_mp, common_gametype_*, ui_mp, patch_*, localized_*, mp_<map>, zm_<map> ...), and
# the load-list suffixes kernel33.c PatchZoneStage reads (_ui_mp, _postgfx_mp) and the server's mp_*.ff rule stay; no
# installed name starts with another match category's "<category>_" (the server mounts <category>_*.ff of the patch folder
# in that category's matches: zm_, coop_, survival_). The alphabetical (link) order in each load list is unchanged.
ZONE_TABLE = [
    # installed (client_shipRetail\patch): the consolidated packs and the standalone zones
    ("codol_common", "common_extras", "pack ahead of code_pre_gfx"),
    ("codol_common_ui_mp", "common_ui_mp", "pack of the UI list"),
    ("codol_common_postgfx_mp", "common_postgfx_mp", "pack after code_post_gfx"),
    ("codol_english_locale", "english_locale", "English strings (one language)"),
    ("codol_chinese_locale", "chinese_locale", "Chinese strings (one language)"),
    ("codol_infected_cyborg_postgfx_mp", "infected_cyborg_postgfx_mp", "standalone (C#-written)"),
    ("codol_zm_m1911_arms_diag", "m1911_arms_diag", "standalone; not zm_*: the server mounts zm_*.ff in zombies matches"),
    ("mp_codol_weapons", "mp_weapons", "standalone; the server mounts mp_*.ff"),
    ("mp_codol_weapons_postgfx_mp", "mp_weapons_postgfx_mp", "standalone; the server mounts mp_*.ff"),
    # source zones (zones/, zone_source/): parts of the packs; old installs carry them under the old names
    ("000_codol_party_modes", "000_party_modes", "part of common_extras / english_locale"),
    ("codol_english", "english", "part of english_locale; also the English zones' name prefix"),
    ("codol_english_font", "english_font", "part of common_extras"),
    ("codol_english_ui", "english_ui", "part of english_locale"),
    ("codol_english_ui_extra", "english_ui_extra", "part of english_locale"),
    ("codol_english_usermaps", "english_usermaps", "part of english_locale"),
    ("codol_english_zombies_base", "english_zombies_base", "part of english_locale"),
    ("codol_english_hud_styles", "english_hud_styles", "part of english_locale"),
    ("codol_chinese_ui", "chinese_ui", "part of chinese_locale"),
    ("codol_chinese_ui_extra", "chinese_ui_extra", "part of chinese_locale"),
    ("codol_chinese_hud_styles", "chinese_hud_styles", "part of chinese_locale"),
    ("codol_field_orders_postgfx_mp", "field_orders_postgfx_mp", "part of common_postgfx_mp"),
    ("codol_loading_mode_names", "loading_mode_names", "part of common_extras / english_locale"),
    ("codol_zm_deathmachine_audio", "zm_deathmachine_audio", "part of common_extras"),
]
ZONE_WORDS = {old: new for old, new, _why in ZONE_TABLE}
ZONE_WORDS.update({
    "codol_chinese": "chinese",                  # the Chinese zones' name prefix (codol_chinese* -> chinese*)
    "codol_english_": "english_",                # the same prefix spelled with its underscore
    "gen_mp_codol_weapons": "gen_mp_weapons",    # the mp_weapons zone's table producer, named after it
})
# A line holding this word keeps its old zone names: what an existing install still carries (patch_locale_policy.h
# kPatchZoneMerged, the legacy name rules, their tests, a zone's origin file in another install).
LEGACY_MARK = "LEGACY_ZONE"
# Never rewritten: the sh1x server (its own repository runs its own table), build output, the pinned review records
# of the UI movies (tools/*-composition, tools/*-producer, tools/theatre-retirement, docs/baselines) and the movies'
# own sources (zones/**/*.gfxpatch: a change there is a new composition stage of the movie).
ZONE_NEVER = ("server/", "build/", "third_party/", "docs/baselines/", "tools/theatre-retirement/")
ZONE_NEVER_PARTS = ("-composition/", "-producer/", ".gfxpatch/")
_ZONE_PATTERN = re.compile(r'(?<![A-Za-z0-9_])(' + '|'.join(re.escape(k) for k in sorted(ZONE_WORDS, key=lambda s: (-len(s), s)))
                           + r')(?![A-Za-z0-9_])')


def zone_scope():
    """Tracked text files the zone step rewrites, and tracked paths it may move."""
    files = []
    for rel in all_tracked():
        if rel.startswith(ZONE_NEVER) or any(p in rel for p in ZONE_NEVER_PARTS) or (ROOT / rel).resolve() == SELF:
            continue
        files.append(rel)
    return files


def zone_text(text):
    """text with the zone names replaced on every line that does not carry LEGACY_MARK; (new text, count)."""
    if LEGACY_MARK not in text:
        return _ZONE_PATTERN.subn(lambda m: ZONE_WORDS[m.group(1)], text)
    out, total = [], 0
    for line in text.splitlines(True):
        if LEGACY_MARK in line:
            out.append(line)
            continue
        new, n = _ZONE_PATTERN.subn(lambda m: ZONE_WORDS[m.group(1)], line)
        out.append(new)
        total += n
    return "".join(out), total


def zone_path(rel):
    """rel with every path component (or the stem of the last one) that is an old zone name renamed."""
    parts = rel.split("/")
    for i, p in enumerate(parts):
        stem, dot, ext = p.partition(".")
        if stem in ZONE_WORDS and (i < len(parts) - 1 or dot):
            parts[i] = ZONE_WORDS[stem] + dot + ext
        elif p in ZONE_WORDS:
            parts[i] = ZONE_WORDS[p]
    return "/".join(parts)


def zone_pending():
    """[(rel, new rel, replacements)] the zone step would still change."""
    out = []
    for rel in zone_scope():
        n = 0
        if not rel.endswith(BINARY_SUFFIXES):
            data, text = read(rel)
            if text is not None:
                n = zone_text(text)[1]
        new_rel = zone_path(rel)
        if n or new_rel != rel:
            out.append((rel, new_rel, n))
    return out


def apply_zones():
    """Move the zone sources and rewrite the zone names. {new rel: (old text, new text)} of the rewritten files and
    {old rel: new rel} of the moved ones."""
    texts, moved = {}, {}
    for rel, new_rel, n in zone_pending():
        if new_rel != rel:
            (ROOT / new_rel).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["git", "-C", str(ROOT), "mv", rel, new_rel], check=True)
            moved[rel] = new_rel
        if n:
            data, text = read(new_rel)
            new, _ = zone_text(text)
            (ROOT / new_rel).write_bytes(encode(new, data))
            texts[new_rel] = (text, new)
    for parent in sorted({str(pathlib.PurePosixPath(r).parent) for r in moved}, key=len, reverse=True):
        path = ROOT / parent
        while path != ROOT and path.is_dir() and not any(path.iterdir()):
            path.rmdir()
            path = path.parent
    return texts, moved


def length_hazards(files):
    """C code that steps over a renamed string literal by the OLD name's length (q += 14 after
    strstr(p, "k33_survival")): every renamed name changes length, so such a step must be sizeof("...") - 1."""
    hazards = []
    for rel in files:
        if not rel.endswith((".c", ".h", ".cpp", ".inc")):
            continue
        _, text = read(rel)
        if text is None:
            continue
        for old, new in MAPPING.items():
            if len(old) == len(new):
                continue
            for m in re.finditer(r'"%s"' % re.escape(new), text):
                window = text[m.end():m.end() + 120]
                if re.search(r"(?:\+=?|[<>]=?)\s*%d(?![0-9])" % len(old), window):
                    line = text.count(chr(10), 0, m.start()) + 1
                    hazards.append("%s:%d: %s" % (rel, line, new))
    return hazards


def check(report):
    files = tracked_files()
    pending = 0
    for rel in files:
        _, text = read(rel)
        if text is not None:
            pending += rename(text)[1]
    hazards = length_hazards(files)
    for h in hazards:
        print("old-length step after a renamed literal: %s" % h)
    for rel in menu_files():
        _, text = read(rel)
        pending += len(_IW_PATTERN.findall(text))
    for rel, new_rel, n in zone_pending():
        print("zone name(s) still to rename: %s%s%s" % (rel, " -> " + new_rel if new_rel != rel else "",
                                                      " (%d in the text)" % n if n else ""))
        pending += n + (new_rel != rel)
    conflicts = movie_conflicts()
    for old, rel in conflicts:
        print("step-2 name spelled by a UI movie (move it to KEEP_K33): %s in %s" % (old, rel))
    stale_header = not (ROOT / GENERATED).is_file() or (ROOT / GENERATED).read_bytes() != renames_header()
    if stale_header:
        print("%s is missing or stale: run the tool" % GENERATED)
    found = remaining(files)
    for word, reasons in k33_remaining().items():
        for reason, n in reasons.items():
            found.setdefault(word, {}).setdefault(reason, 0)
            found[word][reason] += n
    bad = {w: r for w, r in found.items() if any(k.startswith(("UNCLASSIFIED", "STILL")) for k in r)}
    if report:
        by_reason = {}
        for word, reasons in found.items():
            for reason, n in reasons.items():
                by_reason.setdefault(reason, []).append("%s (%d)" % (word, n))
        for reason in sorted(by_reason):
            print("%s:\n    %s" % (reason, ", ".join(sorted(by_reason[reason], key=str.lower))))
    gsc_mismatch = server_gsc_mismatch()
    print("%d mapped name(s) still to replace; %d unclassified codol/k33 word(s) in the code" % (pending, len(bad)))
    for word, reasons in sorted(bad.items()):
        print("    %s %s" % (word, reasons))
    return 1 if pending or bad or hazards or conflicts or stale_header or gsc_mismatch else 0


def server_gsc_mismatch():
    """True when server/sh1x's rename table (config.gsc_names) differs from GSC_TABLE (absent: a note only)."""
    path = ROOT / "server" / "sh1x" / "tools" / "rename_prefix_map.json"
    if not path.is_file():
        print("note: server/sh1x is not initialised; its config.gsc_names was not compared")
        return False
    names = json.loads(path.read_text(encoding="utf-8")).get("config", {}).get("gsc_names")
    if names is None:
        print("note: the pinned server/sh1x has no config.gsc_names yet (bump it to the server's GSC rename)")
        return False
    if names != GSC_NAMES:
        print("server/sh1x config.gsc_names differs from GSC_TABLE: %s" %
              sorted(set(names.items()) ^ set(GSC_NAMES.items())))
        return True
    return False


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--tree" in args:
        sys.exit(tree(args[args.index("--tree") + 1], "--check" in args))
    if "--check" in args or "--report" in args:
        sys.exit(check("--report" in args))
    sys.exit(apply())
