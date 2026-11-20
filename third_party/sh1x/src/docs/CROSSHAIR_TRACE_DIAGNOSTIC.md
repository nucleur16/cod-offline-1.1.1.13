# Crosshair trace observation, 2026-10-29

`lookatDebug` defaults off. Setting it to 1 enables read-only native observations from `UpdateLookAtEntity`, limited to one line per human per second. It does not modify target classification, masks, traces, client code, or gameplay.

Each `lookat:` record identifies the rejection stage or final classification and includes session/pm state, health, viewer team, output weapon flags, eye/end coordinates, trace fraction/hit type/entity/startsolid/allsolid/content flags, hit entity type/health/contents, native actor team, and raw owner handle (0 unowned, otherwise entity number plus 1). Bots are excluded. Use the actual target direction while comparing this output to passive C1 `tools/crosshair_probe.py` readings.

Native production-path unit check: `make run-test_lookat` passed. Existing friend/foe/dead/neutral/hidden/corpse/player cases still pass; new assertions verify the default-off diagnostic, one-second throttle and bot exclusion. No live process was started, stopped or manipulated by this worker; no real in-game targeting fix is claimed.

Read-only source findings to distinguish during the next owned probe:

- Native `G_TraceCapsule` calls `SV_Trace` with locational=0. `SV_SetupIgnoreEntParams` ignores the viewer's children/siblings. `SV_PointTraceToEntity` checks actor contents, ownership and linked broadphase contents; without a locational DObj it traces bounds. Live actors initialize `CONTENTS_BODY`.
- OpenBLOPS `Game/Client/cgame/cg_draw_names.cpp::CG_ScanForCrosshairEntityInternal` clears predicted bits 8/16 before its scan. Its locational trace mask is 0x280B001; children/siblings are not ignored. Enemy coloring additionally requires the held weapon's enemyCrosshairRange and FX visibility, then writes crosshair settle timestamps. These are reference leads, not verified C1 field offsets.
- Passive C1 readings with weapon color-change enabled, but both predicted/snapshot flags and settle times zero, do not prove a team-code defect. Trace rejection, early scan gates or an invalid enemy range can produce those results. Previous telemetry checked the color-change boolean but did not measure the range.
- C1's scanner is virtualized in .tvm0. The readable consumers and .tvm0 references do not justify replacing its logic or copying T5 offsets. No speculative hooks or behavior changes were added.
