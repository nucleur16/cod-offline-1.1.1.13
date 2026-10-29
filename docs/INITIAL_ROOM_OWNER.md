# Initial room ownership after a successful manual join

The exact C1 SDK calls the room subscription callback before storing the
incoming channel owner. Publishing deferred owner metadata restored the host
star but the native callback interpreted zero-to-host ownership as a migration,
started the room timer and left after ten seconds. That backend-only candidate
was withdrawn after its real-client failure.

The paired replacement retains the SDK's owner write and native roster role
calculation. It bypasses the migration callback only for the local player's
initial self-subscription metadata in an already accepted, connected, non-host
room: matching channel, zero previous owner, no pending join, authenticated
local identity and a unique accepted roster containing both local player and
incoming owner. The adapter changes no room/cache fields, Ready values,
authority, timeout value or phase. Other callbacks use the original function.

Exact-build guards cover callback entry713F90, SDK owner store6FD18D and both
identity accessor branches722773/722799. The ESI/ret12 bridge preserves flags
and registers. A compiled fixture covers the positive case and nineteen
fallbacks. A second replay runs the compiled adapter inside the actual C1 SDK
6FD100, native callback713F90, role calculation70ECE0 and timeout pump7105D7:
the withdrawn behavior leaves; the replacement assigns one host with no
migration timer; pending joins and real owner changes retain their timer.

The backend initial_channel_owner feature is disabled by default and requires
this paired client. It must not be enabled for an unqualified mixed-client
production population. Offline replays do not establish rendered UI, live
membership, gameplay or WAN acceptance. Local live results are separate
receipts; production remains subject to client qualification and maintenance.

The first local paired test kept the guest connected for over five minutes
and restored its host flag, but the creator's own roster still contained zero
host flags. The first legacy subscription notice had cleared the creator's
existing SDK owner before it serialized that roster. The subsequent backend
correction preserves owner metadata for the confirmed, admitted creator only
on that initial notice. The pending guest receives no early owner metadata or
invented acceptance. Eight handler tests and native legacy-versus-preserved
creator replays pass; live acceptance of that correction is separate.
