# Capacity queue ticker

The existing server-message ticker displays capacity queue position and rough ETA.
Queue and server-preparation/joining status share one upper-right view below
Settings and Exit. A status update replaces the previous animation immediately,
including the transition from queue placement to joining. Ordinary lobby notices
retain the original bottom-left position. These are not two simultaneous tickers.
Queue text in the front end uses y110, above the language selector that covered
the original y905 line at1280x720. Other announcements keep their original position.
A new queue heartbeat replaces the current queue animation immediately. Entering
a match removes capacity messages and retains ordinary messages; returning to the
front end restores its native geometry.

Only bind_showServerMessage, onSetIsInGameHandle and tryStartAnimation change.
An independent ABC reader verifies all class/method/script metadata, existing pool
entries and other method bodies stay exact. Only one movie tag and one rawfile
asset change. Source assembly retains every modified asset byte and increases
native block4 allocation by153 bytes. The source-generated voice_ui_mp.ff hash is
04ecb752948859242c711b43b6975572c1287f48861e0109e1329c52ce85cf3c.

The shared source baseline also records the existing kernel33.c hash from the
already compiled and real-client tested queue/background r12 build. No C source
changes accompany this ticker update. The source guard had still recorded its
71786b6 version; it now agrees with the compiled8723e5c source.

Compilation and structural/source checks pass. Real-client ticker readability,
capacity expiry, automatic handoff and removal still require their own acceptance.
This is part of the complete paired resident release; production is unchanged.


## Combined integration candidate

The subsequent merge into `Kernel33-integrate-all-20261029` retains the current
localhost controls, lobby readiness guards, map controller and independently
reviewed InviteItemRenderer. The new ServerMessageView source/tag and registry
entry are included alongside those classes. The protected baseline is the union
of both inventories; the current kernel33.c and InviteItemRenderer hashes take
precedence over the older remote values.

The assembled merged codo.gfx is 4,579,910 bytes with SHA256
`47e1240b47601997ada67dc5bd128e3991ae4a83c510f61e7d63401904339d71`.
The shared UI candidate is
`output_remote_ui_merge_candidate/client_shipRetail/patch/codol_common_ui_mp.ff`,
2,076,133 bytes, SHA256
`d748c5ae4b3d5834520249b1c1ee641ddd7d7f9af396218f2314fc8c0785c22c`.
Direct baseline validation, all five baseline tests, all seven invitation tests,
and the actual-body localhost controller tests passed. The shared-zone build
completed successfully. Runtime UI acceptance remains pending; no deployment or
real-client qualification is implied by this merge/build check.
