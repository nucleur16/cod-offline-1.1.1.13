# Paired Bot Training preview publication candidate

The local fresh-room failure showed Wet Work while the allocator selected Favela.
Discovery arrived without any host room-info preview and the backend fell through
to its independent rotation. Earlier offline cases supplied a preview and missed
that producer gap. The previous local backend-only candidate failed gameplay.

The opt-in `CODOL_TRAINING_PREVIEW_PUBLISH=1` module publishes the actual native
room-info record before forwarding genuine host Ready=true at C1 70EB70. It
uses the same type15 message layout and room send method 70D8C0 as native 70EA70.
It does not choose a map or change Ready, room, session, roster or map fields.
Only the exact main room, connected current SDK owner, playlist wire ordinal2,
training flags, valid nine-pool entry and out-of-game state may publish. Other
Ready calls retain their original behavior. Exact native byte guards precede
installation; the environment flag defaults off.

The paired backend requires the current owner's preview for Bot Training
discovery when its preview flag is enabled. Unknown preview cannot become a
different server-selected map. Other playlist and room flows remain separate.

Qualification: actual compiled adapter passes26 cases, including all nine entries,
publication-before-Ready ordering, unchanged room bytes and17 rejected scopes.
Actual C1 writers70A850 and708AB0 execute offline for all nine records; output
matches the retained codec and the actual booking consumer selects each map.
Bit/type and string primitives, transport and room authority remain explicit
fixture boundaries. The focused backend matrix passes36 suites/509 cases with
one conditional skip. Shipping and QA DLLs build without warnings.

These proofs do not establish real C1 queue ordering, discovery delivery or
gameplay. No candidate is production qualified. Repeated fresh and reused room
starts must confirm published preview arrival before allocation, matching cards
on both clients and the actual loaded map. Keep paired components together.
Evidence: `E:/codol-task-evidence/training-preview-publish-20261026-r1/`.
