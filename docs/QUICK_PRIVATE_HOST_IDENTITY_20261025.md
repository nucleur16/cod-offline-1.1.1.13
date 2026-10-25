# Quick host selection for private launch identities

The native room title can contain `p69c7b...` while the native roster supplies the
public display name. The previous unique bare-name selector then fails on the
local host. Native type15 is ignored by a local host, so rewriting the relayed
title alone cannot correct that local selection.

When this client is the native room host, select exactly one verified current
member whose UID string equals native `GetPlayerID` and whose native `IS_ME`
agrees. For a guest, retain the unique current bare `realName` selector. The
coupled backend must normalize the genuine canonical host's type15 and successful
targeted type5 room title to that host's signed public display name. It retains
all other native fields and original acceptance/Ready/sender rules.

The native `GetPlayerID` handler `5AA8C0` formats the 64-bit ID from `722760` with
`%llu` (`5AA8ED`), matching callback110's string `id` (`695909`). Evidence is from
the exact retained C1 disk image SHA-256
`a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976`.
Use string comparison throughout; do not round account IDs through Number.

Six actual AS selectors are covered by the native-shaped identity/Ready fixtures.
Each local UID contour is required by a causal mutation. Private local titles,
all-false member host flags, long IDs, guest signed names, stale identity/Ready
epochs and role disagreement are covered. Twelve UI unittest cases pass in total,
including the existing RoomInfo display and baseline guards. Native callbacks
and elapsed time remain explicit fixture inputs.

The existing FFDec22 importScript/method-transplant path compiles exactly
`cppSetMemberReadyState`, `bind_quickMatchReady` and
`onRankedMatchPassedTimeTimerHandle`, retaining original signatures and all other
method bodies. Only their two class tags change from the preceding RoomInfo
candidate; 6863 other tags remain byte-identical. Login and zone marker are
unchanged. The retained offline native archive reader accepts all three assets.
An independent ABC reader additionally verifies all metadata, existing pool
entries and 238 other bodies across both controllers remain exact. The prior
invitation contract retains protection for every existing body; only the two
reviewed Quick body hashes are updated, with their original hashes recorded.

Current movie SHA-256:
`c0ac41b419cfe9f273fcb4d27bc74e0e246fa121896d5d0239de74cd57ff600e`.
Current candidate `voice_ui_mp.ff` SHA-256:
`30061852c0d7a6484a00999ef5f88924356cee7fb7b068bbd1ad0166304740c8`.

This must ship with the corresponding backend change in
https://github.com/zzVertigo/codol-classic/pull/134. No client/game process,
selected-file publication or production deployment was performed. The original
legacy-account repeated-match stall and current-build graphical/gameplay
acceptance remain open.
