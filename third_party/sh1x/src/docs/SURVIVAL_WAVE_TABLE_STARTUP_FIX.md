# Survival startup: absent per-map wave tables

The first Survival lookup constructed mp/survival_waves_<map>.csv and called
TableLookup to detect whether it existed. A missing StringTable never returns
an empty cell: StringTable_GetAsset attempts the default table and aborts map
loading. Qadeem failed before the intended tier fallback could run.

The installed common_gametype_survival fastfile and the rawfiles provide twelve
map-specific wave tables, the shared survival_waves_tier_1 table, and the
separate survival_waves AI-type table. Qadeem and Strike have authored level
placements but no specific wave table. The script now selects the real shared
tier directly for those maps. The other twelve keep their actual level table.
It does not use the differently shaped AI-type table as a wave-table fallback,
add invented CSVs, relax missing-asset handling, or mask an absent default.

Validation:
- survival_wave_audit.py executes the actual GSC selector and precache
  prefix under Node, rejecting any tableLookup existence probe, and compares
  every selected name with the actual installed fastfile stringtable list.
  All fourteen advertised levels select a present table with twenty authored
  waves and a real first wave.
- Static GSC resolution passes for Qadeem with the modified script.
- Real native startup probes used only UDP17761, dedicated isolated homes and
  active rawfiles, without changing the user's existing client/host/lobby.
  Qadeem reached survival running=1 in16.02seconds; Strike in18.06seconds.
  Both loaded28AItypes and20waves from survival_waves_tier_1, with4repeat waves
  and15AItypes actually used. Qadeem loaded29squad/68heli/4paths; Strike loaded
  57squad/91heli/4paths. Both owned probe processes were shut down afterward.
- These are native server startup checks. C1 joining/playing Survival is not
  claimed by this validation.

Source: active rawfiles maps/mp/gametypes/_survival.gsc. Parent reviewed,
committed and deployed it as rawfiles91cc7a5; changes take effect on the next
Survival map start. Audit/probe code is retained under tools/; regression tests
and the recorded startup evidence are under tests/.

Run tests:
python tests/test_survival_wave_tables.py
python tools/survival_wave_audit.py --game <game> --asset-tool <CodolSoundDumper.exe>
python tools/survival_startup_probe.py --game <game> --port 17761
The last command refuses an occupied probe port and stops only its own child
processes. It never writes the lobby rotation, active server config or deployed
rawfiles. Runtime logs and detailed results remain in obj/survival_wave_probes/.
