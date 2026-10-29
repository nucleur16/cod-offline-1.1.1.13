# M14 EBR after branch consolidation

The M14 tables, producer tools, zone manifests and all44 model sources in main
match `dev`'s corrected `b06cb8d` asset subtree byte for byte. The installed
weapon zones also match that release: table MD522e724382b185632b699b54cd583a04d,
art MD5c0111bc74c91dfaa0a79b5970307cf7b. Neither `dev` nor its files were changed.

The missing behavior was in native Survival archive loading. Its category-only
patch scan omitted the two permanent `mp_codol_weapons` zones, despite borrowing
MP weapons/gameplay. The old server reported50 legal M14 attachment rows, no
complete added receiver models, and rejected both tested composed recipes for
missing base art. The normal profile code then used its stock weapon path,
whose original model names are `viewmodel_m14_ebr` / `weapon_m14ebr_tan`.

The server now loads those exact two shared weapon zones in Survival, in the
same sorted first-definition order used by the client. It does not import
unrelated MP map/UI patches. MP/Coop/Zombies category selection is preserved.

The isolated after-test mounted both zones, found58 legal attachment rows and
all44 complete receiver models, and composed the plain and camo4 recipes to
`wea_m14ebr_m14ebr_blk` / `_04` and their `_vmgun` variants. This proves the
reported Survival fallback path is fixed. Create-a-Class slot/release tables
were already present; their source is unchanged. Native receipts and exact
binary hashes are tracked in the server repository. A rendered in-game model
check remains distinct from the source/native composition tests.
