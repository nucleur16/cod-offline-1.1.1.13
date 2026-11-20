# Exact ee5e script release recipe

This directory preserves the exact reviewed script archive's ordered source inventory. It contains hashes and build/proof metadata only; the archive, game assets, credentials and live state are excluded.

| Component | Exact SHA-256 |
| --- | --- |
| Script IWD, 5,410,990 bytes / 1,718 members | `ee5e4102ffa70d5be1566116aacd5121ea435c9890b940ea0e13286dfe642dc3` |
| Required normal native | `92eb66e3730ced32cf921e2856df44b04a4caabd0e402020a24257819c0e0fda` |
| Required Zombies native | `4a226c41e6b734a2dce24210bcb51749f152adc41499a66db01c7f68d6bc4d50` |
| Canonical/member/policy manifest | `524bf6c0df3e13392c994ac7e459a59ac23077437e8ed0e9cbd2f629753dd4c3` |
| Unchanged authoritative packager | `8e1efb3d692b36d40ad95d7f96a62452d4b00d80e4e1e3ef408ea4898becc65a` |

English and Chinese use the same script archive. Native routing remains MP/co-op through normal and Zombies through the Zombies profile; language does not select a script revision.

## Source and archive correspondence

`member-order.json` records every one of the **1,680 raw and 38 support** members in exact archive order. Each raw entry separately pins its **canonical committed Git blob OID, SHA-256 and size**, its **historical archive-member SHA-256 and size**, and its explicit source-to-archive line-ending policy. It also pins the actual packager, imported helper and both native code manifests. The archive retains the reviewed 99cad preservation candidate, with only `maps/_zombiemode.gsc` updated for authored spawn reservation.

The published canonical raw merge is **c7da25a75f517969463479cbf621450480dd1ce0**, with parents c67753856df1011fb7caada14101bacfdcfacc60 and 3f91e86c49b3a892cf7657351ee72cc7490518b2. The recipe requires that exact raw HEAD and verifies its actual committed blob bytes. The native raw submodule is now **prepared clean at detached c7da**, with `core.autocrlf=false` / `core.eol=lf`. Native remains ab21a3699496eb2af6df622c85cada6174b4460d + 8458d68306f3d2c3b768f6d50836b55625c2a6fd; its index still records e37 until the parent stages and commits the prepared gitlink. Native/application commit and pin closure remain pending.

This is a reviewed source rebase rather than a fast-forward from e37: common base cfb0c56cf006d754dc69bc8f1a6d8929f0f6323c, with three old-branch-only and 54 new-branch-only commits. The old queued postgame class-reply guard and podium combat-spawn guard are present in current c7da source. Exact archive reproduction binds the full accepted current source; no new e37 ancestor claim is made.

**Correction to the earlier blanket Git-byte equality claim:** 1,562 canonical LF raw blobs differ from the reviewed archive bytes solely by line endings inherited from the earlier CRLF working checkout. The other 118 raw members and all 38 support members are identity mappings. The v1 working-byte reproductions remain historical evidence; they did not prove canonical Git-byte equality. The v2 manifest replaces that claim with exact dual byte pins and per-file policies, bound to the parent's postcommit audit hash `01eb3ec45516dcec24f73b9b17c5508137aa05df7cdad5e06203d6b3b63d90ca`.

For only the 1,562 explicitly listed files, the recipe validates canonical LF bytes and the recorded LF/CRLF counts, reconstructs the recorded CRLF bytes in fresh external staging, then requires the exact historical member hash. Identity files undergo no conversion. Inputs must equal either their exact canonical Git bytes or exact reviewed archive bytes; another line-ending mixture or any content change is rejected. The original checkout and committed source are never normalized or rewritten. No 1,562-file source churn commit is needed.

The ordinary packager's Git enumeration order produces a different archive hash despite identical member contents. This recipe preserves the reviewed donor order explicitly. It invokes the **real unchanged `tools/codol_package.py::build_pack`**, using only a verified ordered inventory in its isolated imported module. It does not modify the packager. Source copies are private output inputs; there are no hardlinks or runtime changes.

## Reproduce after Git closure

From the native repository root, with its raw submodule initialized at the published accepted commit, run:

```text
python -B docs/releases/current-scripts-ee5e-20261023/reproduce.py --output E:/CoDOL-Task-Clients/ee5e-clean-source-rebuild
```

The output must be a **new absolute directory outside the native and raw roots**. The default rejects uninitialized, dirty or merging raw source and requires canonical commit c7da25a. It verifies the entire raw/support inventory, actual canonical blobs, recipe inputs, explicit policies and all archive-member hashes before writing. It retains `verified-source/`, the exact IWD and `reproduction.json` inside that new directory. Existing outputs are never overwritten. Any changed source, inventory, policy, recipe or archive hash fails closed; changes require a new qualified artifact/manifest.

The v2 recipe first passed against an independent clean LF checkout and the clean Git-recognized reviewed CRLF checkout. After preparing the actual native submodule, the **default command above passed with no raw override and no escape flag**, again reproducing exact ee5e from all 1,680 canonical inputs. All three receipts have `raw_clean: true` and `reviewed_working_tree_explicitly_allowed: false`. The native index was unchanged by preparation and reproduction; the gitlink is deliberately unstaged. Historical v1 precommit exception receipts remain separate.

`qualification.json` records these exact reproductions, submodule preparation/ancestry and **eight rejection cases**, including an actual source mutation in an isolated E clone, dirty-source rejection, changed canonical/member pins, unsupported or mismatched line-ending policies, and existing-output protection. The native index and the original raw/independent LF inputs remained unchanged; the authorized submodule checkout changed only its own prepared state. The reproducer neither builds nor qualifies a native binary and does not operate C1. Read-only upstream checks still report native main `8458d683` and raw main `3f91e86c`.

## Local C1 proof boundary

`proof-boundary.json` binds two parent-observed C1 receipts: `qa12-zombies-four-C1-return-proof.json` hash `74636250ca1b0c94ea684ca5dc3a5a697383936528317fb8d30670eed397fc74`, and `qa12-zombies-four-C1-repeat-proof.json` hash `17f5a1e08b1a355cd952bc0a007a7eff1139409bfe80b44a7a5edb17ea7153f7`.

On **4a226c41 + ee5e4102**, four actual clients (two English, two Chinese) passed one local `zm_prototype_sh` manual-room run: guest-only Ready and cancellation, real cold-worker wait without the old busy modal, all four Round 1 views with distinct authored positions and peer HUD, natural results on all four, and return to the same retained room (parent-reported backend 69421 / UI 490). No client or lobby restart occurred during that room/match lifecycle. These observations are distinct from source hashing and offline body tests.

The second receipt confirms the same four C1 processes reached natural defeat results and returned to backend 69421 / UI 490 again. The fourth Ready started that match immediately with about 13 seconds left on the native countdown. Both matches used native `4a226c41` + archive `ee5e4102`, had no client/lobby restart, and ended with ordinary worker cleanup. These are parent-observed local C1 results, not tests executed by this recipe. Normal92 C1, MP/Quick on the combined candidate, Asylum/Training spawn contracts and full release/production qualification remain pending. The earlier 4a + 76d3 narrow receipt remains separate historical evidence. This directory authorizes no publication, maintenance or production restart.
