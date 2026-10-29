# Capacity queue ticker

The existing server-message ticker displays capacity queue position and rough ETA.
Queue and server-preparation/joining status share one upper-right view below
Settings and Exit. A status update replaces the previous animation immediately,
including the transition from queue placement to joining. Ordinary lobby notices
retain the original bottom-left position. These are not two simultaneous tickers.
Queue and joining text use x880/y80 (a viewport at approximately x820/y60
at1280x720), below the upper-right buttons. Other announcements retain x32/y905.
A changed queue status replaces the current animation immediately. Identical
heartbeats refresh the existing message expiry without restarting its slide;
the full text can cross the viewport even when its animation exceeds ten seconds.
Entering
a match removes capacity messages and retains ordinary messages; returning to the
front end restores its native geometry.

Only bind_showServerMessage, onSetIsInGameHandle and tryStartAnimation change.
An independent ABC reader verifies all class/method/script metadata, existing pool
entries and other method bodies stay exact. Only one movie tag and one rawfile
asset change. Source assembly retains every modified asset byte and increases
native block4 allocation by77 bytes over the previous upper-right candidate.
The source-generated voice_ui_mp.ff hash is
1c4c3c86a5a029a7fb1c2bc98664a61f5e7088fb363fd86c737ae1d6372154c1.

The shared source baseline also records the existing kernel33.c hash from the
already compiled and real-client tested queue/background r12 build. No C source
changes accompany this ticker update. The source guard had still recorded its
71786b6 version; it now agrees with the compiled8723e5c source.

Compilation and structural/source checks pass. Real-client ticker readability,
capacity expiry, automatic handoff and removal still require their own acceptance.
This is part of the complete paired resident release; production is unchanged.
