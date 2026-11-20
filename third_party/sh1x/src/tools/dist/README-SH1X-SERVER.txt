sh1x server - Call of Duty Online Closed Alpha Server
=====================================================

Match server only. The lobby (Demonware emulator) is run separately, on the
same host.

  sh1x_dedrun.exe / sh1x_dedrun      the server (Windows / Linux 32-bit)
  START-SH1X-SERVER.cmd                start on Windows
  start-sh1x-server.sh                 start on Linux
  c1_keys.bin                          archive keys (C1.exe is used instead if present)
  client_shipRetail\chinese\*.ff       shipped fastfiles
  main\*.iwd                           shipped archives
  main\sh1x_svr_scripts.iwd            the server's scripts, string tables,
                                       bot waypoints and bot classes
  main\server.cfg                      settings - edit this
  main\qconsole.log                    the log

The layout matches a Call of Duty Online 1.1.1.13 install, so the folder can be
merged onto one. Keep it private: it holds Activision's game files and their
decryption key.


SETUP
-----
1. Start the lobby with its state directory where main\server.cfg expects it
   (default: lobby_state\ in this folder), or edit the three codol_*Path lines.
   - native lobby:  set CODOL_STATE_DIR=<this folder>\lobby_state before
                    1-START-LOBBY.cmd; profiles live in its resources\ folder,
                    so point profilePath at that.
   - Docker lobby:  its ./data is the state directory, ./data/resources the profiles.
2. Give the lobby the rcon_password from main\server.cfg
   (CODOL_MATCH_RCON_PASSWORD, or native_server_rcon.txt in its state dir),
   and the address players reach: CODOL_MM_HOST_IP=<public IP>,
   CODOL_MM_HOST_PORT=<SH1X_PORT>.
3. Run START-SH1X-SERVER.cmd (Windows) or ./start-sh1x-server.sh (Linux:
   apt install libc6-i386 lib32stdc++6 lib32gcc-s1).


PORTS
-----
  17733/udp        match server (SH1X_PORT)
  3074/udp+tcp     lobby
  3075/tcp         lobby launcher admission


ADMISSION
---------
The lobby admits players (website launch tokens when it has the website's
public key) and writes each admitted player's session key to
game_admissions.txt in its state directory. With admissionRequired 1 the
server lets in only those players, as their lobby account and under their
website username.


BOTS
----
Bots fill around the real players: none while the server is empty, otherwise
scr_botfill (12) minus the number of real players, never more than
scr_botmax (11). Set both in main\server.cfg.


MORE THAN ONE MATCH
-------------------
The lobby asks every match server what it is running (rcon sh1x_status). When a
player presses Ready it:
  - joins a server that already has other real players (the player is sent
    that server's map and mode);
  - otherwise switches a server nobody else is on to the map and mode picked;
  - otherwise, when every server is full of real players, starts another one
    with START-SH1X-SERVER.cmd on the next free port (lobby settings
    CODOL_INSTANCE_SPAWN and CODOL_INSTANCE_SPAWN_PORTS, default 17734-17741).
Extra servers keep their logs in instances\<port>\main and quit after five
minutes without a real player. Open those UDP ports too.


UPDATING THE SERVER WITHOUT ENDING A MATCH (Linux/Docker)
---------------------------------------------------------
Copy the new build beside the old one as sh1x_dedrun.new, then:
    nohup ./sh1x-rollout.sh > rollout.log 2>&1 &
It starts a second match server on the new build, has the lobby send new players
there (match_rollout.txt in the lobby state directory), restarts the old server
once its match is over and its players are back in the lobby, then removes the
temporary one when it empties. Nobody is kicked. Progress is in rollout.log.
A new script pack goes beside the live one as main/sh1x_svr_scripts.iwd.new;
the rollout gives it to the new server and installs it when the old one
restarts. Do not replace main/sh1x_svr_scripts.iwd by hand while a server runs:
the old build compiles it at its next map load and stops on any new builtin.


EDITING SCRIPTS
---------------
A loose file under main\scripts\ with the same relative path as a file in
sh1x_svr_scripts.iwd (for example main\scripts\maps\mp\gametypes\dm.gsc)
overrides the packed one. Loose main\waypoints\<map>_wp.csv and
main\mp\botclasstable.csv override theirs too.
