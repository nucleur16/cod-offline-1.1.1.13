# Survival wave StringTable overrides

The existing native Survival patch now carries eleven wave StringTable
overrides beside the two kiosk menus and eleven localization assets. The
original thirteen assets are retained byte for byte; no menu offsets or codec
rules changed. No shared UI or client DLL rebuild is required.

This patch must mount ahead of base `common_gametype_survival`: the server's
mounted tables take precedence over loose CSV fallbacks. These packaged inputs
mirror the authoritative rawfiles repo's `mp/survival_waves_<map>.csv` files.
Eleven non-gas tables replace existing special groups on waves 7/14/18 with
chemical/hazmat/hazmat while retaining counts and bosses. Hardhat and Terminal
also regain the missing repeat metadata for waves 17–20. Firing Range and Storm
already have their authored gas schedules and require no override.

To reproduce after updating server scripts/data:

    python tools/sync_survival_wave_tables.py --rawfiles <native-tree>/bin/home/codol_rawfiles
    python tests/test_survival_wave_overrides.py
    python tests/test_survival_kiosk_menus.py
    python scripts/build_zones.py patch_common_gametype_survival --candidate --out output_survival_progression

The table test checks all twenty waves, gas and repeat values and C1 StringTable
codec round trips. The actual built candidate was decrypted and parsed with
`zonekit.container`/`zonekit.zoneimage`: 24 assets, all 13 prior kiosk assets
unchanged and eleven decoded wave tables. Native actor/GSC behavior and live
gameplay acceptance are tracked separately in the server's
`docs/CODOL_SURVIVAL_PROGRESSION.md`.
