# Retired duplicate map table upgrade

Existing launcher installations can retain a removed fastfile after an update.
Kernel33 now ignores the exact retired `codol_zombies_endless` archive in every
locale, preventing it from overriding the complete shared `usermaps_patch` map
table. Prefix/suffix lookalikes remain eligible. The source regression covers
English, Chinese and native language selection. No launcher redownload or
manual file removal is required by this correction.

The change follows merged upstream PR20. Its input commit and current main have
identical trees. The shared frontend, PadSnap, map filtering, locale fallback
and pinned launcher bridge remain inherited from that merged baseline.

`build-proof.json` binds current source inputs, both client profiles, the existing
launcher host and all21 generated shipping files. Production retains its
single-client guard; only the isolated testing profile permits local multiples.
The rebuilt local DLL ran four actual EN/ZH clients with the retired archive
deliberately retained. Each process logged that it ignored the file, then all
four entered Zombies, displayed natural results and returned to the same room.
This is narrow local acceptance; the combined release remains held.

The19-zone build's unfiltered verifier passed18 comparisons. Its usermaps UI
input in the QA installation was already shipping-filtered, so that comparison
is not claimed as a pass; the final filtered file is byte-identical to the
preceding accepted shared build. No new FF source was introduced here.
