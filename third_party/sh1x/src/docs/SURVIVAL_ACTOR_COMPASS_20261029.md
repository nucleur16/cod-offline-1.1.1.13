# Survival actor radar: original C1 consumer evidence (2026-10-29)

The permanent Survival UAV producer is paired with an existing real C1 actor
consumer. No additional client mode hook or synthetic map marker was added.

`tests/test_c1_actor_compass.py` executes the unchanged C1 image (SHA256
`D990610732258B1D00DD6FE63D6691F294312A2E0562955A01CE16216115390D`)
in Unicorn. It maps original PE sections and supplies snapshot, actorInfo, encoded
origin, ordinary radar inputs, map geometry, dvar values and prior compass history.
The only code hook stops execution at the outer return address. No engine helper,
math, team, visibility, origin or radar function is substituted.

The actual compass driver at `0x00433A70` reads an eType18 snapshot entity,
resolves actor slot0 through `cg+0x168514` (valid), `+0x168520` (entity number),
`+0x168528` (team), and updates compass record19 through `0x00433830`.
The compass array is `0x016C7A00`, stride0xB8; actor slots occupy records19–50.
A viewer on team2 and actor on team1 produce hostile flag1. The original position
codec decodes current actor origin `(200,100,10)`. The original sweep test sees
movement from `(200,-100,10)` across its sweep and calls `0x00433120`, publishing
ping time1000 and the native latched position `(200,-100,10)` into record+0x30/+0x10.
This is native swept UAV behavior, rather than continuous satellite revelation.

The six tests pass: actor update/ping in Survival, Coop and war; radar disabled;
friendly actor; invalid actorInfo; non-actor/dead entity types0/20; and stale
compass history. Negative cases suppress the expected update or ping. Categories
use the same real actor consumer, so the crosshair-specific category gate does
not imply a corresponding compass gate.

The existing renderer also consumes these same records: `0x00434AF3` reads the
ping timestamp at record+0x30, `0x00434B2F` compares it to client time, and
`0x00434C2C`–`0x00434C78` submits the ordinary enemy ping material from
`0x016D9B84`. Its record loop ends at51 (`0x00434EF3`), covering the actor records
as well as players. The regression uses ordinary radarEnabled1 with spyplane0
and satellite0, matching the permanent UAV rather than requiring a purchased
advanced support effect. This renderer path is source/disassembly evidence; the
regression executes the upstream actor update/sweep/ping production, not GPU
rendering.

Run:

    python tests/test_c1_actor_compass.py --image "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13/C1.exe" -v

Result: 6 tests PASS. This supplements the owned native server probe documenting
`teamHasRadar=[0,0,1,0]` and playerState radarEnabled1 in
`SURVIVAL_PURSUIT_RADAR_CLASS_20261029.md`. Live C1 actor-dot appearance
remains user/parent visual acceptance; no claim of a captured rendered frame.
Passive read attempts found the first newly launched process already exited and
the replacement in an uninitialized/menu state. No process input, write, focus,
RCON command, restart or deployment was performed by this investigation.
