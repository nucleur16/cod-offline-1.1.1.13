# Resident capacity queue client candidate

The resident backend can keep a manual room request queued while all compatible
servers are occupied. The existing ticker shows its room/party position and a
rough estimate. A validated capacity update renews only that room's current
native booking callback, allowing a wait beyond the SDK's ordinary timeout.
It does not change Ready, admission, roster, player spawning, or game results.

The adapter requires the exact supported client code bytes, native callback
kind and pending status, manual booking context, current room, and a bounded
accepted capacity message. Wrong-context or malformed messages do not renew the
callback. Service failure retains finite native failure handling. There is no
global server-busy suppression or unbounded callback hold.

`candidate.json` pins every compiled C/header/definition input and the separately
built shipping and QA DLLs. Only QA enables the authorized local test interface;
shipping keeps the inherited quiet production profile. The fixture exercises the
actual adapter decision code, including wrong contexts and repeated renewals.
Build dependencies and the embedded client code image remain private external
inputs; neither binaries nor game assets are committed here.

The source map matches the already tested client-r8 candidate exactly. Its old
build receipt's commit label was stale; this record explicitly distinguishes that
label from the complete verified compilation-input map and actual branch base.
The new canonical native normal/Zombies builds still need paired real-client
acceptance, owner/guest, crisis behavior, and the remaining release gates. Do not
install or deploy this source checkpoint as an accepted complete release.
