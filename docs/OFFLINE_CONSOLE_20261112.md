# Offline console and the embedded match server (2026-11-12, branch `feat/offline-console`)

The request: "codoffline needs to have the command console from the dll, and have the commands sync up with the
server commands as well". This document describes what is enabled, how a typed line is routed, which commands
are synced and how the sync is tested, and what was measured as opposed to inferred.

## What gated the console, and what is enabled now

`src/console/console.c` (the overlay console: input line, completion, log, Present-hook drawing) was compiled only
when `K33_DIAGNOSTICS` was 1 (`#if K33_DIAGNOSTICS && !defined(K33_NO_CONSOLE)` in `src/kernel33.c`, around the
include and around `InstallConsole()`). `K33_DIAGNOSTICS` follows `K33_LOCAL_TESTING` (`src/production_profile.h`,
`scripts/build-profile.sh`), and shipping builds reject it. So the default `sh scripts/build.sh` had no console, and
only a local-testing build had one.

Now (`src/kernel33.c`, two small blocks):

| part | shipping (`build.sh`) | local testing (`K33_LOCAL_TESTING=1`) |
|---|---|---|
| console.c: overlay, completion, history, `cmdlist`/`dvarlist`/`find`/`condump`/`con_help`/`con_status`, HUD dvars `cg_drawFramerate`/`cg_drawViewOrigin`, `con_toggleKey` | yes | yes |
| console_server.c: route to the hosted server (`sv`/`rcon`, bare server names, server completion, cheat echo) | yes | yes |
| k33shot.c: `r_screenshot`, `com_frameStats`, `k33_shot.req`/`k33_stats.req`, draw-call hooks | no | yes |
| `con_shot` built-in (console BMP) | no | yes |
| `rcon` to a server other than the hosted one (`con_rconPort`, `main\k33_rcon.txt`) | no | yes, only for an explicit `rcon` while nothing is hosted |

The console still opens with the key left of 1 (scan code 0x29) or F1 (`con_toggleKey`, 0 = none), and
`+set k33_console 0` still leaves it out. `-DK33_NO_CONSOLE` still builds and links.

What the shipping DLL now contains because of this (all byte-checked as before, logged once by `InstallConsole`
in builds that have a log): the `Direct3DCreate9` import-slot wrap (0xB42598) and the device/swap-chain `Present`
vtable hooks, the window-class WndProc swap (0x62FB85 -> `ConsoleWndProc`), and the four pre-hooks on
`Com_LogPrintMessage` 0x594010 (0x5941A4, 0x5942CE, 0x5944A2, 0x59B96E). Nothing test-only came with them: the
command-file transport, `cl_fakeMouse`/`ui_pressReady`, the parity logger and the snapshot diagnostics keep their own
`K33_LOCAL_TESTING`/`K33_DIAGNOSTICS` gates.

## Routing rules (ConExecute, console.c)

A line is split on `;` outside quotes, and each command goes to exactly one place:

1. Console built-ins (`clear cmdlist dvarlist find condump con_help con_status`) run in the console.
2. `sv <line>` (alias `rcon <line>`) goes to the hosted server.
3. A name the client has (a C1 command, or a C1 dvar by its numeric or readable name, also after
   `set/seta/reset/toggle...`) goes to C1's `Cbuf_AddText` 0x58BA60. If the server also has that name, the console
   adds one grey line: `<name>: the client's own; the match server has one too: sv <name>`.
   **Where a name exists on both sides, the client wins; `sv <name>` addresses the server's.** Measured in the
   pinned server table: 219 dvar names collide, e.g. `timescale`, `sv_maxclients`, `sv_cheats` and many `bg_*`.
4. A cheat command from the server's set (below) goes to `Cbuf` as well. C1 has no such command, so
   `CL_ForwardCommandToServer` 0x494CE0 sends it as this player's reliable client command: that is what a cheat
   needs, because the server applies it to the client that sent it. While not connected the console prints
   `no match running: <cmd> is a command of the match server`.
5. A server command or dvar the client does not have goes to the hosted server without a prefix (`dev_cheats`,
   `dev_clients`, `g_gravity`, `set g_speed 300`, ...).
6. Anything else goes to `Cbuf`, as before.

Whenever rule 2 or 5 applies and this client hosts nothing, the console prints
`no match running: "<line>" is for the match server, which this client runs only during a match`.

### The route to the server (console_server.c)

The route is the server's own loopback rcon, the same channel `p2p.c` polls `sh1x_status` on. The console sends
one datagram `rcon "<password>" <line>` to 127.0.0.1:`<host port>`, and the reply is what the command printed
(`SVC_RemoteCommand` redirects `Com_Printf` into the reply and runs the line with invoker power 100). Replies are
read only from that address and port, on a short worker thread, because the server runs on this same game thread.

The port and the password file come from `P2PConsoleHost` (a self-contained block at the end of
`src/p2p/p2p_inproc.c`). It reads `g_p2p` under `g_p2pLock` and returns them only while a host session is starting
or ready on 127.0.0.1. The console asks again for every command. It keeps no handle and no pointer into
`sh1x_host.dll`, so a host that is torn down and started again (re-hosting on branch k33-offline-3) needs nothing
from this side. `src/p2p/p2p.c` is unchanged.

Why not `Sh1xHost_Command`: that export is `Cbuf_AddText` into the server. The line runs in a later server frame,
and nothing returns its output.

### Cheats and the server's permission model

The server enforces this, unchanged (`dev_cheats.c`): `CheatsPermitted() = sv_cheats &&
!lobby_match_server()`. `lobby_match_server` is true when `admissionRequired` is 1 (hosting always sets it)
and `devServer` is not 1. Once `sv_cheats` has been on, the server process records no progression or stats.

The console's part: when the host types `dev_cheats on` (a server console command, so only the host's rcon
can run it), `ConSvCheatsOn` first sends `devServer 1`, as its own rcon datagram, then the command.
`devServer` is registered with flags 0, so the operator may set it at run time. It is the server's own
developer-server switch, and in this offline client the server exists for this one player on loopback. Nothing
else sets it, and nothing on the client sets `sv_cheats`.

Every new match starts with cheats off (`+map`, not `devmap`). The first cheat command typed in a match where the
console has not seen `sv_cheats` on prints a one-line hint: `cheats are off in a new match: type dev_cheats on
first ...`.

The server answers a cheat command with a game message (`DevPrint`, server command `e`). Measured: C1 shows
this neither in `console_mp.log` nor on the HUD (`viewpos`, `give ammo` and a refusal left nothing). So half a
second after each cheat the console asks the server for `dev_cheats` and prints the result: whether cheats are
permitted, then per player the health, pm_type, the god/demigod/notarget/noclip/ufo flags, the position and the
weapon. After `devset <dvar> <value>` it prints that dvar instead. `devhelp` prints the console's `con_status` list.

## Synced commands and how they are kept in sync

`scripts/gen_sh1x_cmds.py` reads the pinned server source (`server/sh1x`, commit
`1e945cab00bc6fc29f33e621ba866d8cb134290f`) and writes `src/console/sh1x_cmds.h`:

- `kSh1xCmds` (145): every `Cmd_Add{,P,H}Command{,Generic}("name", ...)` literal.
- `kSh1xDvars` (517): every `Cvar_Register<Type>("name", ...)` literal with its description, plus the `devset`
  dvars that GSC creates.
- `kSh1xCheats` (14), in `cmd_devhelp`'s order: `god demigod notarget noclip ufo kill give take setviewpos viewpos
  devset addbot kickbot devhelp`. Each one is checked to be dispatched by `ClientCommand` (g_cmds.c) or
  `DevClientCommand`, and a verb that `DevClientCommand` handles but devhelp does not list fails the
  generator. Usage strings are taken from the server's own: `give <weapon [model]|health [n]|ammo|all>`,
  `take <weapon|weapons|health [n]|ammo|all>`, `setviewpos x y z [yaw [pitch]]`, `devset <dvar> <value>`.
- `kSh1xDevset` (10): `jump_height g_speed g_gravity timescale player_sprintSpeedScale bg_fallDamageMinHeight
  bg_fallDamageMaxHeight sv_botStop scr_bots sv_botDifficulty`. This is now the console's `devset` argument
  list (the old hand-written `kArgDevset` and `kArgRcon` lists are gone).

Server console commands relevant to development (all reachable bare or with `sv`): `dev_cheats`, `dev_cheats
<on|off>`, `dev_clients`, `dev_player <slot> <noclip|ufo|god|demigod|notarget> <on|off|toggle>`,
`dev_clientcmd <clientnum|bots|all> <command ...>`, `sh1x_status`, `status`, `map_restart`, `kick`, plus every
other name in the table.

Completion: while a match runs, the console fetches the server's real `cmdlist` and `cvarlist` once per host
session over the same route, and again after a command went to the server (for fresh values). The live lists
replace the table. Before a match, or if the server does not answer, the table is the list, without values.

`tests/test_offline_console.py` (in `tests/run_tests.sh`) fails when:

- `gen_sh1x_cmds.py --check` differs (the submodule moved, or a server command, dvar, cheat or usage changed);
- the console's `kDevCmds` cheat rows and `kSh1xCheats` differ in names, order or usage;
- the cheat set read straight from the server source differs from the table;
- the console is gated again, a diagnostics-only part appears outside `#if K33_DIAGNOSTICS`, or the route gets a
  port, password or target from anywhere but `P2PConsoleHost`.

The table versus the running server (measured, `cmdlist`/`cvarlist` of a hosted zm match):

- the server answered 141 commands and 550 dvars against 145 and 517 in the table;
- in the table only: `assets crash error extract freeze getss showconfigstring` (under build conditions), and 47
  dvars of disabled subsystems such as `bot_*`;
- on the live server only: `XAssetUsage meminfo sh1x_mem`, 80 dvars registered by retained assembly or GSC (`ai_*`,
  `bg_shock_volume_*`, numeric names).

That gap is why the live lists win while a match runs.

## Host DLL

The installed `D:\CoD\CODOL Builds\k33-uistyle-offline-test\p2p\sh1x_host.dll` (PE time stamp 2026-11-12 14:07:26,
SHA-256 `fe0b7d3d...f3fe`) suffices; no rebuild is needed. The route uses only rcon, `cmdlist`, `cvarlist`,
`devServer`, `dev_cheats` and the cheat client commands, and its strings show all of these
(the dev-cheat-commands work is in it). No new export is required.

## Runtime check (measured)

The test client lived in its own folder: a real `main` with hard links to the .iwd files, junctions only for
`main\sh1x_rawfiles`, `main\video`, `miles` and `usermaps`, and copies of everything small or writable. It was
driven by posted window messages and the command file, without desktop input. Evidence is under the agent scratch
folder `offline_console\evidence`.

- **Shipping build** (no command file, no log). The console opened at the menu with the scan-code-0x29 key.
  Typing `codol_dev` listed the server commands from the table (`02_shipping_console_completion_strip.png`).
  `sv status` printed the `no match running` line (`03_shipping_condump.txt`).
- **Local-testing build, zm match hosted in-process** (`run2_*`, `run3_*`, `run3_condump.txt`):
  - Before the match: `sv sv_bot` completed from the table; `god` printed `no match running: god is a command of
    the match server`; `dev_cheats on` printed `no match running`.
  - In the match, `Kernel33.log`: `console: server lists from the running host (session ...): 141 commands, 550
    dvars`.
  - `viewpos` with cheats off printed the hint and the server's `cheats: cheats are off ...` line.
  - `dev_cheats on` printed `dev_cheats enabled=1`; `god` then showed `... god ...`; `noclip` showed
    `pm_type 2 god noclip`; a second `noclip` showed `pm_type 0`.
  - `devset g_speed 380` printed `"g_speed" is: "380" default: "190"`.
  - `sv timescale` printed the server's dvar; `find botStop` printed `sv  sv_botStop "0"` from the running server.
  - Completion of `sv_bot` showed the server dvars with live values; `codol_dev` showed the four server names.
  - The god flag was still on after 60 s in the match (`dev_cheats` then: `hp 100 ... god`).
  - The server log (`p2p\home\main\qconsole.log`) shows `CoDOL cheats: client 0 give/devset/viewpos` and the
    progression-off warnings.
- **Name collision** (`run4_condump.txt`): `timescale` ran C1's (`-> 1867`), with the line `the client's own; the
  match server has one too: sv timescale`.
- **Run 2 used an earlier build** in which the server still got `+set devServer 1` on its command line.
  Runs 3 and 4 were made with the per-command echo.
- **Final build** (the committed sources; `final_*`, `final_condump.txt`, the server's `qconsole.log`), zm_theater_sh
  hosted in-process:
  - `god` before cheats printed the hint and `cheats are off ...; devServer 0`, so the server's start-up is
    unchanged.
  - `dev_cheats on` printed `dev_cheats enabled=1`. The server log shows
    `Rcon from 127.0.0.1: devServer 1`, then `Rcon from 127.0.0.1: dev_cheats on`.
  - `god` then showed `god`; `give ammo` showed `CoDOL cheats: client 0 give` in the server log; `ufo` showed
    `pm_type 3 god ufo`, and the second `ufo` showed `pm_type 0` again.
  - `sv status` printed the server's status table, with ConTest in slot 0.
- **Harness note**: two final-build attempts did not reach a match. On this machine, with other clients running,
  C1's own startup zone load took 134 s, and the room command arrived before the menus were up. Driving the same
  client once it had loaded worked. No console code runs on that path.

## Unverified / inferred

- `kill`, `take`, `setviewpos`, `addbot`, `kickbot`, `demigod` and `notarget` were not exercised at runtime. They
  share the dispatch path of the measured ones (`god`, `noclip`, `ufo`, `give`, `devset`, `viewpos`).
- That `devServer` may be set at run time is read from its registration (flags 0). It is also measured: the
  final run's `dev_cheats` showed it change from 0 to 1.
- Whether a damage hit is actually absorbed under `god` was not provoked. The run shows the flag on and the player
  alive at 100 hp after 60 s in a zm round; that is inferred evidence only.
- Shipping profile: the console, completion from the table and `no match running` were seen at the menu. A hosted
  match was driven only with the local-testing build, because a shipping build has no command file and no
  back-buffer screenshot. The console and route sources are the same in both profiles.
- The server lists are fetched again after every command that went to the server. The final run measured 8
  fetches, each 2 + 20 datagrams on loopback: cheap here, but not tuned.
