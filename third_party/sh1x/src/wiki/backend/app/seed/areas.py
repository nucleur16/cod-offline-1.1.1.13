"""Human descriptions for the source areas the indexer discovers.

The counts come from the index; this file only supplies the prose and the order
they should appear in. An area found in the tree but missing here still shows
up, just without a description.
"""
from __future__ import annotations

AREAS: list[dict] = [
    {
        "name": "net",
        "title": "Network transport",
        "path": "src/codol/net/",
        "position": 10,
        "summary": (
            "Everything between a C1 client's UDP socket and a decoded message: the "
            "DTLS-shaped handshake and ECDH key agreement, the association step that "
            "ties a lobby-issued identity to a server slot, connect and netchan "
            "framing, the bit-level wire codec every other area serialises through, "
            "the usercmd receive queue, and the pure-file check."
        ),
    },
    {
        "name": "snapshot",
        "title": "Snapshot and frame publication",
        "path": "src/codol/snapshot/",
        "position": 20,
        "summary": (
            "Everything the server publishes to a client each frame. The frame "
            "builder gathers player state, entities, clients, actors and HUD into a "
            "snapshot state; the delta codec writes it against an acknowledged "
            "baseline; the field tables (`*_fields.inc`) are measured C1 wire layouts, "
            "not offsets into engine structs. Config strings, events, radar and the "
            "snapshot history live here too."
        ),
    },
    {
        "name": "player",
        "title": "Player input and movement",
        "path": "src/codol/player/",
        "position": 30,
        "summary": (
            "Input decode through to movement result: usercmd parsing, the C1 pmove "
            "cutover (ground, mantle, dive, sprint), recoil and stance, the animation "
            "tables shared with the retained `_bg_animation_mp.asm`, perks and "
            "abilities, spectate defaults, use hints and team effects."
        ),
    },
    {
        "name": "weapon",
        "title": "Weapons",
        "path": "src/codol/weapon/",
        "position": 40,
        "summary": (
            "CoDOL weapons are composed from a base definition plus attachments "
            "rather than shipped whole, so this area owns composition, the weapon "
            "registry and loader, the fire/reload state machine, weapon locks and "
            "missiles, turret heat, and the server-side weapon config strings."
        ),
    },
    {
        "name": "vehicle",
        "title": "Vehicles",
        "path": "src/codol/vehicle/",
        "position": 50,
        "summary": (
            "Vehicle asset definitions, the native simulation step, and the spawner "
            "that scripts reach through `spawnvehicle`."
        ),
    },
    {
        "name": "script",
        "title": "GSC script surface",
        "path": "src/codol/script/",
        "position": 60,
        "summary": (
            "The builtin functions and methods CoDOL scripts call, grouped by topic "
            "as `codol_script_<topic>.c`, plus the syntax-lowering pass that accepts "
            "C1-era script constructs, precaching, profile and loadout access, menus "
            "and the startup shims that let a map script reach gameplay."
        ),
    },
    {
        "name": "assets",
        "title": "Assets and streaming",
        "path": "src/codol/assets/",
        "position": 70,
        "summary": (
            "Reading the game's own data: fastfiles and IWDs (including their "
            "encryption), the asset registry and streaming, native parsers for "
            "xmodel, xsurface, xanim, material, sound and effect, character "
            "composition from playerdata parts, string tables and map custom data."
        ),
    },
    {
        "name": "collision",
        "title": "Collision and physics",
        "path": "src/codol/collision/",
        "position": 80,
        "summary": (
            "The BSP side of the world: brushes and the brush tree, leaves, terrain, "
            "static models, materials and world planes, plus dynamic entities, "
            "triggers, breakable glass groups and the physics step."
        ),
    },
    {
        "name": "stats",
        "title": "Profiles, progression and match data",
        "path": "src/codol/stats/",
        "position": 90,
        "summary": (
            "The read-only secure profile, experience, challenges and unlock rules, "
            "skill rating, the scoreboard, and the structured match-data schema and "
            "its wire format."
        ),
    },
    {
        "name": "engine",
        "title": "Retained CoD4x engine",
        "path": "src/",
        "position": 100,
        "summary": (
            "The CoD4x server this project forked: the frame loop, console and cvars, "
            "filesystem, netchan, the GSC virtual machine and compiler, game logic, "
            "and the plugin host. CoDOL changes to it are guarded by `CODOL_SERVER`."
        ),
    },
    {
        "name": "asmsource",
        "title": "Retained x86 assembly",
        "path": "src/asmsource/",
        "position": 110,
        "summary": (
            "Engine functions that were never rewritten in C. They are the original "
            "game binary's x86, disassembled and reassembled under their own names, "
            "so their behaviour is exactly the shipped behaviour. CoDOL hooks into "
            "them appear as `call CODOL_*` inside the listing."
        ),
    },
    {
        "name": "platform",
        "title": "Win32 platform layer",
        "path": "src/win32/",
        "position": 120,
        "summary": "Windows entry points, the console window, and the resource script.",
    },
    {
        "name": "vendor",
        "title": "Vendored libraries",
        "path": "src/mbedtls/, src/zlib/",
        "position": 130,
        "summary": (
            "Third-party code compiled into the server: mbedTLS for the crypto the "
            "handshake and IWD decryption need, and zlib for fastfile inflation. "
            "Indexed for cross-referencing, not documented here."
        ),
    },
    {
        "name": "tools",
        "title": "Offline tools",
        "path": "tools/",
        "position": 140,
        "summary": (
            "Helpers that never run inside the server: the archive reader, the event "
            "map generator, probes and catalog scripts. `tools/cod4_dasm/` holds the "
            "full CoD4 disassembly that `src/asmsource/` was drawn from; it is "
            "excluded from the index because it is roughly 1.8 million lines."
        ),
    },
    {
        "name": "tests",
        "title": "Unit tests",
        "path": "tests/",
        "position": 150,
        "summary": (
            "Standalone C and Python tests run by `make test-codol`. Each names its "
            "sources explicitly rather than linking the whole server."
        ),
    },
]

BY_NAME = {area["name"]: area for area in AREAS}
