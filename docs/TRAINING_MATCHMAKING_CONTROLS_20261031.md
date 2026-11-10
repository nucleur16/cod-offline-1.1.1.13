# Training matchmaking controls

Boot Camp and Bot Training retain their matchmaking identity after native
playlist commit changes the room into its generic form. The exact room index,
native room name and validated training playlist are retained in an unarchived
process dvar so a newly loaded movie can restore the restriction after gameplay.
Map and mode controls
stay disabled while Ready, the native coordinator, dedicated discovery and
booking retain their existing behavior. Leaving the room clears the UI marker;
an ordinary custom room does not inherit it.

Automatic training room titles use Boot Camp or Bot Training. Other internal
launcher account aliases use the uniquely identified local owner's public name
when available, with Matchmaking as a safe readable fallback. This affects
rendering only; identifiers, owner data and session names remain unchanged.

The companion lobby fix retains authenticated training origin across generic
room updates, including Boot Camp's observed descriptor, and uses public display
names on dedicated search results. Deploy the paired client and lobby together.
This is a client/backend change, not a GSC-only release.

Validation so far: FFDec compilation and shared zone build; actual ActionScript
handler execution tests for both training playlists, commit/return, repeated
Ready, room replacement and Leave/custom-room isolation; room-name rendering
fixtures and existing Ready gates. These are offline checks, separate from the
actual local C1 observations below. Production acceptance remains pending.
After the user closed the client and dedicated-server Firewall prompts, real
local C1 testing exposed a missing lifecycle boundary: unloading/reloading the
movie after a match erased the original controller-local marker. The updated
handler test now constructs a new controller against retained process dvars;
room replacement, changed native name, and Leave discard the context.

The rebuilt UI (SHA256
`5c0ea2927887c888c7474b0c15a9917d3084863257e42c364cf006ebbccb1a90`)
completed an actual Boot Camp Killhouse match, returned with the Boot Camp title
and map/mode controls locked, refused clicks at the former controls, and entered
a second Killhouse match. Native status reported one human, eleven bots and
twelve seats. Local match clocks were shortened through ordinary RCON dvars to
exercise the real timed completion path. No native package or game script was
changed for those clock adjustments.

The old retained QA DLL lacked the selected background-window and queue-wait
integration. Both shipping and QA profiles were rebuilt from the selected
source; QA SHA256
`5088e591ed929a1898b8d3188bcc2bdc5a9ddb33f4e63da10c67ad6ebdb63434`
then repeated Boot Camp fresh/return/second Killhouse entry successfully.
Bot Training also entered Favela, completed a timed match, returned with its
validated next Crossfire preview and locked controls, and entered Crossfire.
The following native preview was Bog. Both actual servers reported one human,
eleven bots and twelve native slots, with the expected one/three human room
caps. These local tests used the retained signed dedicated package, not the
final combined production cohort. The shipping DLL was built, not gameplay
qualified.

The companion backend's scoped pre-offer cancellation completion now lets C1
run its actual empty-search reset callback. Actual Cancel/retry recovered in
both modes and reached dedicated Killhouse/Crossfire respectively; the stock
no-server notice on cancellation must be acknowledged. The custom-room host
could change war to dom and Overgrown to Rust to Storm, and a second real client
joined with the matching roster/map/mode. The actual guest frame subsequently
showed room318's title as the public host name ReliabilityQA1, with guest
ReliabilityQA2 and the Killhouse/Team Deathmatch card. No internal alias was
visible. The exact R2 UI and QA5088 native profile were unchanged. Zombies
menu/room/Ready also reached actual Nacht entry with one human and four seats;
broader Zombies gameplay and final shipping/r40/production acceptance remain
pending.

The user explicitly clarified that Bot Training should rotate normally within
its configured playlist, while Boot Camp has only Killhouse. This change denies
manual map/mode control without removing legitimate playlist rotation. The user
requested one combined update; no independent training cutover or maintenance
is selected by this task.
