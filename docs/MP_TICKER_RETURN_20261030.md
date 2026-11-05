# Queue ticker after a match

The shared ServerMessageView can remain hidden after gameplay: entering a match
removes queue messages and hides an empty view; returning to the front end
restores its coordinates without restoring visibility. The old `show` method
only made the view visible while `_isInGame` was true.

Restore visibility when a valid queue or joining message reaches the front end,
before duplicate-message handling. Multiplayer and Zombies use the same view.
Ordinary notices retain their existing visibility behavior. The queue's
geometry, expiry refresh, animation and removal on match entry are unchanged.

Only the compiled `show` method (body 15), movie tag 6002 and the corresponding
rawfile asset change. Independent metadata, constant-pool, method-body and zone
allocation checks pass. The complete source-built fastfile SHA256 is
`200f1c6514bb8fbd23490d522e2a9b9f7c6679939f8fb72da6ae943d1251d716`.

A bounded interpreter executes the actual original and transplanted ABC prefix
through its duplicate-message delegate. Twelve hidden/visible, queue/join and
ordinary-message cases reproduce the old hidden status and verify the repaired
visibility write. Message construction is a fixture; this is not a rendered C1
acceptance receipt or proof of successful match allocation.

Evidence: `mp-ticker-return-r1/source-reproduction.json` in the private resident
task evidence root. No native DLL, lobby or game-server source change is needed
for this UI correction. Publication and real-client acceptance are tracked
separately from these build checks.
