# The producers of mp_codol_weapons / mp_codol_weapons_postgfx_mp

`build_zones.py` builds both zones from `zone_source/` + `zones/` like every other zone. These files are what made
those sources, kept so they can be made again (for example for the next MP weapon).

- `gen_mp_codol_weapons.py` -- the stringtables of mp_codol_weapons. Input: `CodolSoundDumper zone-stringtables
  --input <zone>.ff --out <dir>/<zone>` dumps of patch_code_post_gfx_mp, patch_common_gametype_mp and
  common_gametype_mp. Each table is written whole: the effective stock table (patch zone first) plus the m14ebr_mp
  rows, because C1 links the first stringtable of a name and this zone loads ahead of every stock zone.
- `MpWeaponBuild.cs`, `MpWeaponInspect.cs`, `MpWeaponCommands.cs` -- `CodolSoundDumper mp-weapons-zone --game <1.1.1.13>
  --out <ff>` (and `mp-zone-inspect`), added to a CODOL-AssetExplorer tree that has PortContext.InPlace (the
  zm_wallbuy / pap staging tree); `Program.cs.diff` registers the commands, `StreamedSurfaceInliner.cs.diff` adds a
  name filter. The output was decompiled into `zones/mp_codol_weapons_postgfx_mp/` (zonekit.decompile, origin new).
- `render_xmodel_export.py` -- flat-shaded side/top views of `export-models --merge --format xmodel_export` output,
  used to check the composed M14 against the zombies model it is cut from.

## What the M14 EBR (m14ebr_mp) is

C1 ships m14ebr_mp as an unfinished MP assault rifle (marketplace item 1057, product 1365): attachment table rows,
the M14 part tables (m14ebrncm iron sight, mag, grip, muzzle; m14ebr stocks) and its attachment animations, but no
base gun (no mp/coweapmodels/m14ebr_mp.csv: the client composed attachments with no body), no class-editor slot rows,
and column 6 of its marketplace row (released) 0. The zombies M14 (m14ebr_zm, the wall buy) draws
codo_weapon_m14ebr_viewmodel / _world: that same gun with its default parts merged into one model, on an M14 receiver
that carries every MP attachment tag (tag_ironsight included; the M21 EBR receiver has none).

mp_codol_weapons_postgfx_mp cuts the receiver surfaces (material mc/mtl_wea_m21ebr_m21ebr_blk) out of the zombies
models into wea_m14ebr_m14ebr_<camo>[_vmgun], camo rows blk, 01 .. 21, each drawn with a renamed copy of the M21 EBR's
camo material of that row (the receiver uses the M21 EBR's texture). Geometry is inline (new XModelSurfs
wea_m14ebr_m14ebr_vmgun10 / _lod010 / _lod110 / _lod210), and everything the models lead to is in the zone, so it is
complete on its own while it stays loaded from startup. Each model carries its own bone bounds inline: the sh1x
server's XModel reader requires it (stock camo models do the same).

mp_codol_weapons adds the base gun rows, the released assault-rifle attachments m14ebr_mp lacked (red_reflex,
for_gripafg, for_griphandstop, bip_ar, sra_riflelaser, sto_ar15fix/prs/ubr; their part rows are the M16's), class-editor
slot rows (the M21 EBR's) and sets the released column.
