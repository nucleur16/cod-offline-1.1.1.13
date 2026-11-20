# Current native parity and animation safety qualification

`qualification.json` describes the current normal **92eb66e3** and Zombies
**4a226c41** builds. The earlier `native-upstream-8458-20261023/` records remain
historical proof for **4d4dd320/e43bcfa4**. No earlier receipt is overwritten.

The only native compiler leaf added to the normal 4d4 closure is
`src/codol/script/codol_script_anim.c`. Zombies additionally drops the obsolete
whole-body unsigned fastfile override: both profiles now use the same correct
first-read-chunk loader. Other Quick, Theatre, signed admission, profile and
session source paths remain unchanged. See `source-preservation.json`.

The archive control runs the real native crypto, inflate, zone-directory,
script-string, asset and rawfile readers against the same 7,265,557-byte QA
archive and same C1 key input. The old loader fails; the corrected loader reads
the complete 11,521,174-byte zone. Only metadata and digests are emitted.
No key material or user-owned archive contents are committed.

The animation control compiles the actual query/receiver/helper and actual
server tree-count implementation, with seven verbatim retained assembly
functions. A valid donor query reads the actual native phase. Two donor child
processes reproduce SIGSEGV. Each corrected profile implementation passes 24
cases. Entity/DObj ownership, script-model eligibility, VM exception transfer,
result emission and the asset-default diagnostic query are explicit doubles.
This is function/ABI proof; it does not run a game VM or prove C1 gameplay.
`PROOF.md` documents the producer and the crash path.

The separately reviewed **76d3f4dd** Zombies script package removes the stale,
unused `getAnimTime(%walk_and_run_loops)` caller while preserving the five pose
functions. The native safety fix alone is not gameplay acceptance. That external
package is not asserted to equal the preserved `e37a77d` rawfiles gitlink.
Normal QA uses its separately pinned script package. The parent observed a
narrow local four-human EN/ZH Zombies start, rendered Round1 HUD, natural
defeat/results and persistent room return on exact 4a22+76d3; see
`zombies-narrow-C1.json`. Normal current-binary C1, repeat-match stability and
full production/release qualification remain pending.

## Reproduce the offline regression

Use a fresh external output directory and the exact current materialized source
with its four pinned native libraries. Supply the preserved **c8a953ad** source
as the animation donor and **e43bcfa4** source as the unsigned-loader donor:

```text
python -B docs/releases/native-parity-safety-20261023/fixtures/prepare.py --profile zombies --source <current-zombies-source> --animation-donor <preserved-c8-source> --unsigned-donor <preserved-e43-source> --output <new-recipe-directory>
```

The preparer rejects mismatched leaf/library/body hashes and existing outputs.
For normal, use `--profile normal` and the current normal materialization.

Use the existing pinned builder from `qualification.json`, with network none,
no published ports, no capabilities, a non-root user, read-only root, writable
`/tmp` and a new writable `/out`. Mount these inputs read-only:

| Container path | Input |
| --- | --- |
| `/src` | Selected current source and `lib-native` |
| `/recipe` | Fresh preparer output |
| `/animation-donor` | Preserved c8 source |
| `/unsigned-donor` | Preserved e43 source |
| `/qa/archive.ff` | Archive matching the qualification digest |
| `/key/C1.exe` | C1 key input matching the qualification digest |

Run `bash /recipe/run.sh`. This launches standalone compiler/test executables,
including two expected-crash donor children; it never launches a server/client.
The portable wrapper is newly packaged; the receipts bind the separately
executed original archive and animation runners. No new wrapper execution is
claimed by this documentation change.
