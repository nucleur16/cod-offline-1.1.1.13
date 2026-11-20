# Qualified departure and prepared-server source cohort

The normal/Zombies manifests now describe the exact sources used by the retained
departure builds. They include the four reviewed headless/durable-write source
leaves and the two departure/text fixes. The Zombies delta is regenerated only
for affected leaves; all other profile-specific differences remain intact.

Both profiles materialize to the same complete code-file hashes as the retained
build inputs. No unfinished frame-profiler, actor-lifecycle or VM performance
work is included. Linux libraries and compiler variables retain their pinned
values. Headless support is already in these profiles: a Windows preparation
tool must verify these leaves and must not apply the old patch a second time.

`qualified-departure-cohort.json` binds the retained Windows/Linux binaries and
runtime evidence digests. Real native control/lifecycle checks cover the five
core cases on both platforms and fifteen additional Linux mode/difficulty cases.
Linux also checks held game clocks and clean resume. Account cohorts are fixtures;
original-client gameplay, departure display timing and private-room acceptance
remain separate requirements.

Each profile also pins its own script revision and IWD digest. The default rawfile
submodule pin is the qualified multiplayer revision; Zombies retains its recorded
profile-specific revision. `verify_scripts.py --raw-repo RAW --profile normal|zombies
--pack IWD` reads committed Git blobs rather than mutable working files and checks
every script, waypoint and bot-class entry. Historical collation bookkeeping is
excluded consistently with the packer. The normal profile now explicitly pins the independently qualified opening-balance
guard at rawfiles `045da5f9`; its IWD digest is `76e157416c3eab10e8b371d42d826274476dfdfe44a7b2814370502016425461`.
Six exact-native startup/VM checks cover Killhouse/war and Bog/combat, quiet held
prematch, unchanged fifteen-second live balancing and natural 6v6 bot openings.
The normal manifest retains its prior script cohort separately: earlier warm
lifecycle receipts prove those earlier bytes, while the script supplement binds
the new VM/startup evidence. No new compiled native input or Zombies script is
introduced, and no original-client gameplay acceptance is inferred.
Twenty-five existing waypoint/bot assets use Windows checkout line endings in
the tested packs. Their LF Git-blob hashes and CRLF conversion are recorded
explicitly; script bytes are never normalized. Other content or inventory drift
still fails verification, even if someone changes the expected ZIP digest.

The existing build revision strings and the legacy departure comment in the
compiled source are preserved for exact input provenance. The actual condition
publishes after every authenticated human departure. A newly built binary still
requires qualification; source identity does not establish byte-identical output
across different build directories/toolchains. The full native/backend/client
release remains held. The corresponding guard was separately qualified on the
unchanged production native and published GSC-only for future multiplayer; that
publication does not authorize the full release.
