# sh1x server (Call of Duty Online Closed Alpha Server) -- native match server.
#
# The server is a 32-bit Linux executable built from this repository
# (tools/build_linux.sh). It needs only the 32-bit C and C++ runtimes.
# Ubuntu 24.04 matches the toolchain it is built with, so libstdc++ symbol
# versions always line up.
#
# Game files are NOT baked in: /game is a read-only mount holding c1_keys.bin,
# client_shipRetail/chinese/*.ff and main/*.iwd. The lobby is not in here
# either: it runs independently on the same host, and /data is its state
# directory (securitykey.bin, game_admissions.txt, resources/).

FROM ubuntu:24.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends libc6-i386 lib32stdc++6 lib32gcc-s1 ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --create-home --home-dir /home/codol --shell /usr/sbin/nologin codol

WORKDIR /opt/codol
COPY sh1x_dedrun /opt/codol/sh1x_dedrun
# Scripts, rawfiles, bot waypoints and bot classes, packed; "_svr_" keeps it out
# of every archive list sent to clients.
COPY sh1x_svr_scripts.iwd /opt/codol/sh1x_svr_scripts.iwd
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
COPY map_handoff.awk /usr/local/bin/map_handoff.awk
COPY rcon_password.sh /usr/local/bin/rcon_password.sh
RUN chmod 0755 /opt/codol/sh1x_dedrun /usr/local/bin/entrypoint.sh /usr/local/bin/rcon_password.sh \
 && mkdir -p /game /data \
 && chown codol:codol /data /home/codol

USER codol

# Defaults; docker-compose.yml passes the real values from .env.
ENV CODOL_GAME_PORT=17733 \
    CODOL_MAP=mp_rust \
    CODOL_GAMETYPE=dm \
    CODOL_ZONE_TYPE=mp \
    CODOL_DEVELOPER=0 \
    CODOL_BOTS=0 \
    CODOL_ADMISSION_REQUIRED=1 \
    CODOL_EXTRA_ARGS=""

EXPOSE 17733/udp

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
