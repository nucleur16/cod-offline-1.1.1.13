#!/usr/bin/env python3
"""Drops the "codol" branding from Kernel33's own names (identifiers, macros, dvars, console commands, the
names the client forwards to the hosted sh1x match server) with an explicit table, so the same rename can be
applied again to another branch (for example after a rebase onto the branch that first ran it).

    python tools/drop_codol_prefix.py            apply the table to the tree, re-pin ui_baseline.json
    python tools/drop_codol_prefix.py --check    change nothing; exit 1 when a mapped name is still present or
                                                 a "codol" name in the code is neither mapped nor listed in KEEP
    python tools/drop_codol_prefix.py --report   print every remaining "codol" name in the code with its reason

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
SERVER_CLASH = {}
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
codol_playlistId codol_preferMapGraph codol_profilePath codol_publishExtendedPlayerState codol_pushPublicProfile
codol_pushSecureProfile codol_ragdollLaunchScale codol_recoverDelay codol_rootMotionTrace codol_securityId
codol_securityKeyPath codol_serverSurfaceGeometry codol_sessionMap codol_sessionStatePath codol_setpos
codol_settleRestingPrediction codol_shadowcompare codol_shieldBlastDebug codol_solidprobe codol_soundTrace
codol_sourceRconRemote codol_sweepprobe codol_theatreAutoRecord codol_theatreMaxSegments codol_theatreSegmentMiB
codol_theatreSegmentSeconds codol_token_consumed codol_token_inventory codol_traceActorEvents
codol_traceRagdollLaunch codol_traceWeaponLock codol_unstuck codol_voiceDebug codol_voiceEcho
codol_voiceMaxPackets codol_warmAdmissionPath codol_warmPreparation codol_worldgunprobe codol_xpRatePercent
codol_zonePath codol_zoneType
codol_oneShotMatch codol_pmemMegs codol_relayIdentityPath codol_scriptPath codol_scriptOverlay
codol_scriptNativeOverlay
""".split()
SERVER_NAMES = {old: SERVER_CLASH.get(old, old[len("codol_"):]) for old in SERVER_NAMES_OLD}

MAPPING = {}
for table in (C_NAMES, CLIENT_DVARS, CLIENT_STRINGS, SERVER_NAMES):
    for old, new in table.items():
        assert old not in MAPPING, old
        MAPPING[old] = new

# ---- what stays, and why ------------------------------------------------------------------------------------
# Exact names (case-sensitive). The --check/--report scan looks at every [A-Za-z0-9_]*codol[A-Za-z0-9_]* word
# (any case) in the code; a word followed by a file extension is reported as a file name of another repository
# or of an install. Patch zone names are also taken from zones/, zone_source/ and zone_packs/ (see zone_names()).
REASON_ZONE = "patch zone / fastfile name (a file name in the installs, loaded by name)"
REASON_ZONE_TEST = "a zone name a test or policy feeds the zone-name rules (codol_<lang>_locale family)"
REASON_MOVIE = "dvar name compiled into the shipped UI movies (codo.gfx / codoingame.gfx tags under zones/)"
REASON_SERVER_REPO = "file, function or folder of the sh1x server repository (renamed there, not here)"
REASON_LAUNCHER = "environment variable / pipe / token of the launcher-managed session protocol (external launcher)"
REASON_PYLOBBY = "environment switch of the external Python lobby (CODOL-EMU), named in ports/tests of it"
REASON_EXTERNAL = "external tool or assembly name (CODOL-AssetExplorer, codol-classic, Python lobby, Discord)"
REASON_LOCALIZE = "localize key inside the shipped zones' localize.str / stringtables"
REASON_INSTALL = "file name inside existing installs"
REASON_COMPAT = "command-line alias kept for existing launchers (patch_locale_policy.h)"
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
    "codol_": REASON_ZONE, "codol_english": REASON_ZONE, "codol_chinese": REASON_ZONE,
    "codol_english_": REASON_ZONE, "codol_zombies_endless": REASON_ZONE,
    "codol_en_locale": REASON_ZONE_TEST, "codol_fr_locale": REASON_ZONE_TEST, "codol_de_locale": REASON_ZONE_TEST,
    "codol_a_b_locale": REASON_ZONE_TEST, "codol__locale": REASON_ZONE_TEST, "codol_native_locale": REASON_ZONE_TEST,
    "xcodol_fr_locale": REASON_ZONE_TEST, "codol_fr_locale_extra": REASON_ZONE_TEST,
    "codol_chinese_extra": REASON_ZONE_TEST, "codol_english_ui_extra2": REASON_ZONE_TEST,
    "codol_loading_mode_names_extra": REASON_ZONE_TEST, "codol_zombies_endless_extra": REASON_ZONE_TEST,
    "codol_englishui": REASON_ZONE_TEST, "codol_chineseui": REASON_ZONE_TEST,
    "mp_codol_weapons": REASON_ZONE, "mp_codol_weapons_postgfx_mp": REASON_ZONE,
    "gen_mp_codol_weapons": "producer of the mp_codol_weapons zone, named after it",
    "codologin": "Scaleform movie name inside the shipped zones (codologin.gfx)",
    # sh1x server repository (files, functions, folders)
    "codol_rawfiles": REASON_SERVER_REPO, "codol_dedrun": REASON_SERVER_REPO, "CODOL_DevPrint": REASON_SERVER_REPO,
    "CODOL_DevClientCommand": REASON_SERVER_REPO, "CODOL_LocationSelectionAction": REASON_SERVER_REPO,
    "CODOL_BeginZoneReplay": REASON_SERVER_REPO, "CODOL_ReadXModelBounds": REASON_SERVER_REPO,
    "codol_map_handoff": REASON_SERVER_REPO, "codol_stage_server_pack": REASON_SERVER_REPO,
    "codol_stringtable_dump": REASON_SERVER_REPO, "codol_discord_presence": REASON_SERVER_REPO,
    "codol_english_font": REASON_ZONE, "CODOL_C1_VIRTUALIZED_20260928": REASON_SERVER_REPO,
    "CODOL_C1_MOVEMENT_LAYOUT": REASON_SERVER_REPO,
    "scr_codol_bots": REASON_PENDING,
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
    "codol_patchlang": REASON_COMPAT,
    "CoDOL": REASON_PROSE, "CODOL": REASON_PROSE, "codol": REASON_PROSE, "sCODOL": REASON_PATH,
    "CODOL_ZOMBIES_ENDLESS": REASON_ZONE_TEST, "CODOL_LOADING_MODE_NAMES": REASON_ZONE_TEST,
    "CODOL_CHINESE_LOCALE": REASON_ZONE_TEST,
    "codol_name_display_url": REASON_HISTORY, "codol_theatre_url": REASON_HISTORY,
    "drop_codol_prefix": "this rename tool (scripts/gen_sh1x_cmds.py and tests/test_offline_console.py import it)",
}

LEFT = r'(?:(?<![A-Za-z0-9_])|(?<=-D)|(?<=\^[0-9]))'
RIGHT = r'(?![A-Za-z0-9_])(?!\.[A-Za-z])'
_PATTERN = re.compile(LEFT + '(' + '|'.join(re.escape(k) for k in sorted(MAPPING, key=lambda s: (-len(s), s)))
                      + ')' + RIGHT)
WORD = re.compile(r'[A-Za-z0-9_]*[Cc][Oo][Dd][Oo][Ll][A-Za-z0-9_]*')


def rename(text):
    """text with every mapped name replaced; (new text, number of replacements)."""
    return _PATTERN.subn(lambda m: MAPPING[m.group(1)], text)


def server_name(name):
    """A name the server source registers, in the client's (post-rename) vocabulary."""
    return SERVER_NAMES.get(name, name)


def server_text(text):
    """Server source text with its codol_ command/dvar names spelled as the client spells them."""
    pattern = re.compile(LEFT + '(' + '|'.join(re.escape(k) for k in sorted(SERVER_NAMES, key=lambda s: (-len(s), s)))
                         + ')' + RIGHT)
    return pattern.sub(lambda m: SERVER_NAMES[m.group(1)], text)


def tracked_files():
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z"], capture_output=True, check=True).stdout
    files = []
    for rel in out.decode("utf-8").split("\0"):
        if not rel or rel.startswith(NEVER_DIRS) or rel.endswith(BINARY_SUFFIXES):
            continue
        path = ROOT / rel
        if path.resolve() == SELF or not path.is_file():
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
            if word in MAPPING:
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
        for macro in PROFILE_MACROS:
            for value in (False, True):
                try:
                    add(normalised_digest(helpers.gate_project(old, macro, value)),
                        normalised_digest(helpers.gate_project(new, MAPPING[macro], value)))
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
    for rel, n in changed:
        print("%5d  %s" % (n, rel))
    print("%d replacements in %d files" % (total, len(changed)))
    if not changed:
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
    # ui_baseline.json "files"
    pin_text = path.read_text(encoding="utf-8")
    repinned = []
    for rel in sorted(set(r for r, _ in changed) | set(repinned_tests)):
        if rel in pins and before.get(rel) == pins[rel]:
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
                if re.search(r"\+=?\s*%d(?![0-9])" % len(old), window):
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
    found = remaining(files)
    bad = {w: r for w, r in found.items() if any(k.startswith(("UNCLASSIFIED", "STILL")) for k in r)}
    if report:
        by_reason = {}
        for word, reasons in found.items():
            for reason, n in reasons.items():
                by_reason.setdefault(reason, []).append("%s (%d)" % (word, n))
        for reason in sorted(by_reason):
            print("%s:\n    %s" % (reason, ", ".join(sorted(by_reason[reason], key=str.lower))))
    print("%d mapped name(s) still to replace; %d unclassified codol word(s) in the code" % (pending, len(bad)))
    for word, reasons in sorted(bad.items()):
        print("    %s %s" % (word, reasons))
    return 1 if pending or bad or hazards else 0


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--check" in args or "--report" in args:
        sys.exit(check("--report" in args))
    sys.exit(apply())
