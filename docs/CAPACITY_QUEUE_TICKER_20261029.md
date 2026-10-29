# Capacity queue ticker

The existing server-message ticker displays capacity queue position and rough ETA.
Queue and server-preparation/joining status share one upper-right view below
Settings and Exit. A status update replaces the previous animation immediately,
including the transition from queue placement to joining. Ordinary lobby notices
retain the original bottom-left position. These are not two simultaneous tickers.
Queue text in the front end uses y110, above the language selector that covered
the original y905 line at1280x720. Other announcements keep their original position.
A new queue heartbeat replaces the current queue animation immediately. Entering
a match removes capacity messages and retains ordinary messages; returning to the
front end restores its native geometry.

Only bind_showServerMessage, onSetIsInGameHandle and tryStartAnimation change.
An independent ABC reader verifies all class/method/script metadata, existing pool
entries and other method bodies stay exact. Only one movie tag and one rawfile
asset change. Source assembly retains every modified asset byte and increases
native block4 allocation by153 bytes. The source-generated voice_ui_mp.ff hash is
04ecb752948859242c711b43b6975572c1287f48861e0109e1329c52ce85cf3c.

The shared source baseline also records the existing kernel33.c hash from the
already compiled and real-client tested queue/background r12 build. No C source
changes accompany this ticker update. The source guard had still recorded its
71786b6 version; it now agrees with the compiled8723e5c source.

Compilation and structural/source checks pass. Real-client ticker readability,
capacity expiry, automatic handoff and removal still require their own acceptance.
This is part of the complete paired resident release; production is unchanged.
