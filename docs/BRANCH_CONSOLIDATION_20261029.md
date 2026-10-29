# Client branch consolidation checkpoint

Integration branch: `integration/all-branches-20261029`, based on Survival fix
`27c0b61`. Before fetching, all 70 local/tracking refs were copied under
`refs/preserve/consolidation-20261028-230102/`, with an additional ref for the
detached usermap worktree at `78aa881`. Fetch used `--no-prune`; existing dirty
and conflicted worktrees were not changed. No original branch was deleted.

All 71 preserved heads are ancestors of this checkpoint. Main `cd7149d` also
brings the former room-production, Quick-intent and gated-P2P heads. The other
maximal targets were `7e837c4`, `3fe348c`, `ecf2e9d`, `78aa881`, `7180c4c`,
`24ddd36`, `8b7faed`, `fff3760`, `1ce2d12`, `f71f694`, `717a726`, `a5bcb53`,
`3c9a5de`, and `57a373d`. No merge used the `ours` merge strategy.

## Reviewed resolutions

- Preserve current built-in localhost hosting, its room toggle/Ready/context
  API, and native gitlink `a239d365` including startup and crate markers. Main's older optional P2P implementation
  must not replace these or duplicate their includes and registrations.
- Include main's independent room roles, training previews, Quick policy,
  production diagnostics and opt-in QA startup-window safeguards.
- Merge MatchController source and recompile its tag; preserve current lobby
  classes and the new InviteItemRenderer in the same shared movie. The ABI
  review records the exact three intended production-main source changes,
  all 51 repooled protected bodies, and the immutable preceding review chain.
- Retain the later M14 models with default parts hidden; the low-memory trial
  contains older competing binary layouts, not additional model identities.
- Retain current complete map catalogues, Survival rows, Zombies availability,
  and existing authored arena ordering. Older union snapshots contain subsets
  or reorder these same rows. No map was removed to resolve a conflict.
- Preserve evolved shared sound diagnostics and voice-limit integration,
  including quiet shipping defaults. Both old diagnostic heads share the same
  original sounddiag blob, predating these improvements.
- Preserve historical Survival staging zones and their menu serializer beside
  the current shipping kiosk implementation. Staging zones remain staging.
- Trial merge resolutions only combine already-present registrations or an
  older DLL. Keep one registration per feature and rebuild current binaries.
- Mark the assembly-read `g_piNext` variable as used: production diagnostics
  removing its only C read otherwise made the optimized shipping DLL fail to
  link. This does not change the hook's target or runtime control flow.

## Validation

- Shipping, developer/vision, and no-console DLL profiles built successfully.
- MatchController and InviteItemRenderer compiled through FFDec; shared UI
  baseline tests passed with both baseline inventories retained.
- Invitation hold/review tests: 7 passed, including 168 original behavior checks,
  99 historical resolved-method checks and 22 production integration checks.
- Local-host toggle, Campaign/Ready routing, and measured room layout Node
  fixtures passed. Survival kiosk unittest and six zone-pack tests passed.
- Current shared UI and Survival kiosk candidate zones built: 2/2 successful.
- Windows managed-Quick fixture passed 293 checks without a live client.
- Linux ASAN/UBSAN executions of the unchanged P2P status and intent-heartbeat
  fixtures passed.
- The complete Windows runner stops when Windows explicitly blocks
  `p2p_status_test.exe` as virus/potentially unwanted software. This is recorded
  as a limitation, not a passing full suite. No protection settings were
  changed and no renamed version of the blocked binary was executed.
- Private-image x86 and live GFx/gameplay acceptance are separate checks.

This checkpoint does not include the subsequent Survival icon/native changes
being developed in parallel. Integrate their committed source and advance the
native gitlink before final release acceptance. No deployment or push is part
of this checkpoint.
