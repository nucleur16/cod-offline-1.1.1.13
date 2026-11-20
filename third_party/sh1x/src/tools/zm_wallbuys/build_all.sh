#!/usr/bin/env bash
# Rebuilds every zm wall-buy outline output from the installed zones (Git Bash).
#   tool_build/   AssetExplorer src copy + PortContext.InPlace (pap_patch_staging/tool_src/PortContext.cs.diff) + WallbuyCommands.cs
#   wallbuys.json the spec (tools/gen_wallbuys.py in the worker scratchpad; copied here)
# Inputs are the INSTALLED map zones: to stack another change (e.g. the Kino worker's theater createfx edit), install or
# point --input at that zone first, or pass --rawfile maps/createfx/zm_theater_sh_fx.gsc=<edited text>. Re-runs are
# idempotent (wallbuy_ fxids are replaced, not duplicated).
# The terminal: python terminals.py wallbuys.json before the zone builds, gen_wall_table.py for the GSC table (README).
set -euo pipefail
ST="D:/CoD/CODOL Builds/zm_wallbuy_staging"
G="${CODOL_GAME:-D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13}"
T5="D:/SteamLibrary/steamapps/common/Call of Duty Black Ops/main"
S="$ST/tool_src/wallbuys.json"
EXE="$ST/tool_build/src/Codol.SoundDumper/bin/Release/net9.0/CodolSoundDumper.exe"
O="$ST/out"
mkdir -p "$O/client_shipRetail/chinese" "$O/main" "$O/tmp"
for m in zm_theater_sh zm_factory_sh zm_sumpf_sh; do
  "$EXE" zm-wallbuy --game "$G" --t5 "$T5" --spec "$S" --input "$G/client_shipRetail/chinese/$m.ff"     --out "$O/client_shipRetail/chinese/$m.ff" --iwd "$O/tmp/iw_model_21.iwd"     --iwd-extra chalk_weapon_explosives_c --iwd-extra grenade_bag_chalk_c
done
# zm_asylum_sh / zm_prototype_sh: GSC walls only. Their stock zones already draw CoDOL's outlines, and a Kernel33
# patch-folder zone for them (zm_wallbuys.ff) crashed C1 at startup (2026-10-26), so none is built.
mv -f "$O/tmp/iw_model_21.iwd" "$O/main/iw_model_21.iwd"
rmdir "$O/tmp"
md5sum "$O"/client_shipRetail/chinese/*.ff "$O"/main/*.iwd
