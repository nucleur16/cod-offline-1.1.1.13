# Full room result

C1's channel-task completion6FD840 calls the supplied callback with only
the channel record and a success boolean. Main-room callback711350 writes
reason3 and leaves on every failure, discarding the remote task's error.
The existing UI selector5C7E40 maps reason1 to SF_UI_ERROR_ROOM_FULL and
reason3 to SF_UI_ERROR_ROOM_NONEXISTENT.

The guarded hook inspects the failing task before that information is lost.
Only task19 / callback711350 / the current main-room channel / explicit
BD_CHANNEL_FULL1104 takes the ordinary LeaveRoom path with reason1. It
does not manufacture a successful subscription or change room authority.
Native0x7230E0 establishes the error constant on this exact build. The entry
and accessor guards exclude mismatched code. Other outcomes use the original
completion, including1000, missing-channel1101, unknown errors and success.

Requires the matching backend capacity classification. An older backend's
generic1000 remains generic; never reinterpret all missing-room errors as full.
Offline native execution and a compiled callback fixture cover the distinction;
the rendered result still requires actual paired-client acceptance.
