/* dw_net.h - the loopback listener (dw_net.cpp). */
#pragma once
#include "dw.h"

namespace dw {

struct NetConfig {
    int port;     /* TCP and primary UDP on 127.0.0.1; 0 = ephemeral (then STUN cannot be reached) */
    int altPort;  /* the alternate STUN port */
    bool stun;    /* bind the three alternate STUN sockets */
    NetConfig() : port(0), altPort(3078), stun(true) {}
};

/* Runs until *stop becomes non-zero. *boundPort receives the TCP port once listening.
 * -> 0, or non-zero when the listener could not be bound. */
int NetRun(Server &server, const NetConfig &cfg, volatile long *stop, volatile long *boundPort);

} // namespace dw
