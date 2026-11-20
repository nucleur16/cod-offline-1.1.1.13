# Keep endpoint associations across private match homes

C1 can retain its encrypted association with an address/port across matches.
Community workers intentionally create a fresh private home for every native
process generation. Keeping `codol_sessions.txt` only in that home discards
transport state which a reused client may still expect at the same endpoint.

The reviewed local candidate adds `sessionStatePath`. Its default empty
value retains the existing home-file behavior. A trusted launcher can instead
select an exclusive per-port path. Saves use the existing atomic-file helper;
loads are bounded by the peer-file limit and retain the original peer parser,
expiry, key checks and future send-counter reservations. A path configuration
change saves/resets the previous peer set before loading the new location.

The coupled application change selects the private worker path
`state/instances/<port>/codol_sessions.txt`; profiles, admissions and match homes
remain separate for each lease. It requires this native implementation. Do not
publish a worker-only release and claim the transport correction: an older
binary does not implement the new setting. The ordinary official worker already
uses a stable per-port home; this change is not a measured reduction of its cold
startup time or proof about the original public friend-join incident.

Offline validation on the combined source:

- Actual native persistence fixture reproduces home loss and checks restoration
  from the shared endpoint path, isolation of another port, expiry, oversized
  files and preserved send/receive counters. Clock/filesystem are explicit
  boundaries; it opens no socket and runs no game server.
- Existing send-reservation, DATA fast-path, atomic replacement and idle peer
  retention fixtures pass.
- Two real worker launch-builder cases with a fake child check shared-path
  reuse and simultaneous-port isolation. Existing community worker tests pass
  (23 cases, one skipped), as do 28 Windows launch/security construction cases.
- Normal/Zombies source pins are refreshed. The unchanged Zombies delta
  materializes with all 1727 compiler-input hashes matching. Old binary
  acceptance remains invalid; exact profile packages and live C1 acceptance are
  pending. A default-tree Linux link alone does not qualify either profile.

The combined default normal source tree links successfully as Linux ELF32,
SHA-256 `fbd8277ba9d93ae71012ca4d8f8a30f2f6fde780233f44112921e9b93c80b1bd`.
This build includes the separate entity-reference ABI correction. Windows and
exact-profile rebuilds of the combined source remain pending; this hash is not
a release selection or a replacement for the native profile manifests.

The earlier chat reported an actual same-client/same-port reconnect using its
own candidate build. That is useful prior evidence, not acceptance of this
combined build. No game client or dedicated server has been launched here.
