# The producers of mp_codol_weapons / mp_codol_weapons_postgfx_mp

`build_zones.py` builds both zones from `zone_source/` + `zones/` like every other zone. These files are what made
those sources, kept so they can be made again (for example for the next MP weapon).

- `gen_mp_codol_weapons.py` -- the stringtables of mp_codol_weapons. Input: `CodolSoundDumper zone-stringtables
  --input <zone>.ff --out <dir>/<zone>` dumps of patch_code_post_gfx_mp, patch_common_gametype_mp and
  common_gametype_mp. Each table is written whole: the effective stock table (patch zone first) plus the m14ebr_mp
  rows, because C1 links the first stringtable of a name and this zone loads ahead of every stock zone.
- `MpWeaponBuild.cs`, `MpWeaponInspect.cs`, `MpWeaponCommands.cs` -- `CodolSoundDumper mp-weapons-zone --game <1.1.1.13>
  --out <ff>` (and `mp-zone-inspect`), added to a CODOL-AssetExplorer tree that has PortContext.InPlace (the
  zm_wallbuy / pap staging tree); `Program.cs.diff` registers the commands. The output was decompiled into `zones/mp_codol_weapons_postgfx_mp/` (zonekit.decompile, origin new).
- `render_xmodel_export.py` -- flat-shaded side/top views of `export-models --merge --format xmodel_export` output
  (surfaces drawn with the hidden material left out), used to check the composed M14 against the zombies model.
- `simulate_surface_draw.py` -- draws a dumped surface set (`mp-zone-inspect --dump-surfs`) through its base offsets.

## What the M14 EBR (m14ebr_mp) is

C1 ships m14ebr_mp as an unfinished MP assault rifle (marketplace item 1057, product 1365): attachment table rows,
the M14 part tables (m14ebrncm iron sight, mag, grip, muzzle; m14ebr stocks) and its attachment animations, but no
base gun (no mp/coweapmodels/m14ebr_mp.csv: the client composed attachments with no body), no class-editor slot rows,
and column 6 of its marketplace row (released) 0. The zombies M14 (m14ebr_zm, the wall buy) draws
codo_weapon_m14ebr_viewmodel / _world: that same gun with its default parts merged into one model, on an M14 receiver
that carries every MP attachment tag (tag_ironsight included; the M21 EBR receiver has none).

mp_codol_weapons_postgfx_mp holds wea_m14ebr_m14ebr_<camo>[_vmgun], camo rows blk, 01 .. 21: copies of the zombies
models on the same streamed surfaces (models/codo_weapon_m14ebr_* in the stock iwds -- the zombies M14's own draw path).
The receiver surfaces (material mc/mtl_wea_m21ebr_m21ebr_blk) draw a renamed copy of the M21 EBR's camo material of
that row (the receiver uses the M21 EBR's texture); the merged default parts draw mc/mtl_wea_m14ebr_hidden, a copy of
stock mc/shadowcaster (techset mc_shadowcaster, no lit or depth technique), so only C1's own parts show at those tags.
Each model carries its own bone bounds inline: the sh1x server's XModel reader requires it (stock camo models do the
same). The first build (2026-10-27, md5 e02a014f) cut the receiver surfaces out into inline geometry; C1 drew that
wrong in first person (front of the gun missing, the muzzle floating), and its base offsets were stale.

mp_codol_weapons adds the base gun rows, the released assault-rifle attachments m14ebr_mp lacked (red_reflex,
for_gripafg, for_griphandstop, bip_ar, sra_riflelaser, sto_ar15fix/prs/ubr; their part rows are the M16's), class-editor
slot rows (the M21 EBR's) and sets the released column.
