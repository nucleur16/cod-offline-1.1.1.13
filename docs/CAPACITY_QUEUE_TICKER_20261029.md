# Capacity queue ticker

The existing server-message ticker displays capacity queue position and rough ETA.
Queue and server-preparation/joining status share one upper-right view below
Settings and Exit. A status update replaces the previous animation immediately,
including the transition from queue placement to joining. Ordinary lobby notices
retain the original bottom-left position. These are not two simultaneous tickers.
Queue and joining text use x880/y80 (a viewport at approximately x820/y60
at1280x720), below the upper-right buttons. Other announcements retain x32/y905.
A changed queue status replaces the current animation immediately. Identical
heartbeats refresh the existing message expiry without restarting its slide;
the full text can cross the viewport even when its animation exceeds ten seconds.
Entering
a match removes capacity messages and retains ordinary messages; returning to the
front end restores its native geometry.

Only bind_showServerMessage, onSetIsInGameHandle and tryStartAnimation change.
An independent ABC reader verifies all class/method/script metadata, existing pool
entries and other method bodies stay exact. Only one movie tag and one rawfile
asset change. Source assembly retains every modified asset byte and increases
native block4 allocation by77 bytes over the previous upper-right candidate.
The source-generated voice_ui_mp.ff hash is
1c4c3c86a5a029a7fb1c2bc98664a61f5e7088fb363fd86c737ae1d6372154c1.

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
`output_remote_ui_merge_candidate/client_shipRetail/patch/common_ui_mp.ff`,
2,076,133 bytes, SHA256
`d748c5ae4b3d5834520249b1c1ee641ddd7d7f9af396218f2314fc8c0785c22c`.
Direct baseline validation, all five baseline tests, all seven invitation tests,
and the actual-body localhost controller tests passed. The shared-zone build
completed successfully. Runtime UI acceptance remains pending; no deployment or
real-client qualification is implied by this merge/build check.

## Heartbeat refresh merge candidate (`9aed480`)

This merge takes exactly incoming `9aed480945f9bd9bd6b8b2119857634727a3fe95`,
without fetching or merging later tips. Identical queue/preparation heartbeats
refresh their current ticker expiry without restarting its slide. The complete
current 36-class/89-file inventory, localhost controls/readiness and current
kernel33.c/InviteItemRenderer pins are retained; only ServerMessageView source,
compiled tag and corresponding source/artifact pins advance. The patch's movie
hash and the baseline's patch.txt hash were recomputed from the assembled merged
movie and actual manifest bytes.

Merged codo.gfx: 4,580,056 bytes, SHA256
`691250c42f916e1a04d17f4f89e6a3e8a461a9ad2f754f6563113040bfa4af03`.
patch.txt SHA256:
`3577fea1e9e324a9e9f60f20c9bc80c48d5532f85cbd80cabce4c851713a136f`.
`output_remote_ui_merge_candidate/client_shipRetail/patch/common_ui_mp.ff`:
2,076,230 bytes, SHA256
`ddb06f0584533849598cd105e0c6e767666c54fe899fa44d78acca5e9cefcd88`.

Direct baseline validation, five baseline tests, seven invitation tests and the
actual AS Campaign/localhost/Ready controller tests pass. Geometry tests pass
MP/Survival/Zombies stock variants and eight viewport sizes. The shared-zone
candidate build succeeds. No commit, deployment or real-client acceptance is
claimed; existing earlier candidate hashes above remain historical receipts.
