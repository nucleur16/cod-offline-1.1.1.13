/* dw_net.h - the loopback listener (dw_net.cpp). */
#pragma once
#include "dw.h"

namespace dw {

struct NetConfig {
    int port;     /* TCP and primary UDP on 127.0.0.1; 0 = a free port the operating system picks */
    int altPort;  /* the alternate STUN port; 0 = a free port the operating system picks */
    bool stun;    /* bind the three alternate STUN sockets */
    bool pickOwn; /* tests: skip the operating system's own choice of a free port, draw one at random at once */
    NetConfig() : port(0), altPort(3078), stun(true), pickOwn(false) {}
};

/* Runs until *stop becomes non-zero. *boundAlt receives the alternate STUN port and then *boundPort
 * the TCP port, once listening. With port 0 the listener takes a free port on which the TCP socket
 * AND the UDP sockets of both loopback addresses could be bound; with altPort 0 likewise for the
 * alternate pair. The ports obtained replace Config::stunPort / stunAltPort.
 * -> 0, or non-zero when the listener could not be bound. */
int NetRun(Server &server, const NetConfig &cfg, volatile long *stop, volatile long *boundPort, volatile long *boundAlt = NULL);

} // namespace dw
