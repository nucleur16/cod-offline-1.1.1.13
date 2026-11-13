/*
 * dw_net.cpp - the loopback listener of the offline Demonware lobby.
 *
 * One thread, one select() loop: the TCP listener (auth and lobby connections), and the four UDP
 * sockets the client's bdIPDiscovery / bdNATTypeDiscovery probes need (dw-capture.py main()):
 *
 *   127.0.0.1:<port>      primary (TCP and UDP)
 *   127.0.0.2:<altPort>   test 2, "changed IP and port"
 *   127.0.0.2:<port>      the advertised changed-address (the client aims follow-up probes there)
 *   127.0.0.1:<altPort>   test 3, "changed port"
 *
 * Neither port is fixed: with NetConfig::port / altPort 0 a free one is taken (the operating system's
 * choice first, see RandomPort), so the lobby runs next to another lobby or a second client on the
 * same PC. The embedder points the client at whatever was obtained (offline_lobby.c).
 *
 * Every socket is bound to a loopback address; nothing here can listen on another interface.
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "dw.h"
#include "dw_net.h"

namespace dw {

struct NetConn { SOCKET sock; Conn *conn; };

/* `err`: why the socket could not be had (WSAGetLastError is lost to the closesocket that follows). */
static SOCKET BindUdp(const char *ip, int port, int &err)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { err = WSAGetLastError(); return s; }
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = inet_addr(ip);
    a.sin_port = htons((u_short)port);
    /* SO_EXCLUSIVEADDRUSE: nobody else may share (or steal) the loopback port. */
    BOOL on = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof(on));
    if (bind(s, (sockaddr *)&a, sizeof(a)) != 0) { err = WSAGetLastError(); closesocket(s); return INVALID_SOCKET; }
    return s;
}

static bool SendAll(SOCKET s, const Bytes &data)
{
    size_t off = 0;
    while (off < data.size()) {
        int n = send(s, data.data() + off, (int)(data.size() - off), 0);
        if (n > 0) { off += (size_t)n; continue; }
        if (WSAGetLastError() != WSAEWOULDBLOCK) return false;
        fd_set wr;
        FD_ZERO(&wr);
        FD_SET(s, &wr);
        timeval tv = { 5, 0 };
        if (select(0, NULL, &wr, NULL, &tv) <= 0) return false;
    }
    return true;
}

static int BoundPort(SOCKET s)
{
    sockaddr_in a;
    int alen = sizeof(a);
    memset(&a, 0, sizeof(a));
    return getsockname(s, (sockaddr *)&a, &alen) == 0 ? ntohs(a.sin_port) : 0;
}

static SOCKET ListenTcp(int port, int &err)
{
    SOCKET tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp == INVALID_SOCKET) { err = WSAGetLastError(); return tcp; }
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((u_short)port);
    BOOL on = TRUE;
    setsockopt(tcp, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof(on));
    if (bind(tcp, (sockaddr *)&a, sizeof(a)) != 0 || listen(tcp, SOMAXCONN) != 0) {
        err = WSAGetLastError();
        closesocket(tcp);
        return INVALID_SOCKET;
    }
    return tcp;
}

/* The lobby's port has to be free for TCP and for UDP, and Windows gives ports out per protocol. A
 * port the system picks for TCP can be one it will not give to UDP: Hyper-V / WSL reserve blocks of
 * 100 ports per protocol (netsh int ipv4 show excludedportrange), and bind() into such a block fails
 * with WSAEACCES. MEASURED on the development PC: tcp 127.0.0.1:61400 binds, udp 127.0.0.1:61400 is
 * refused, and since the system hands TCP ports out one after another, sixteen tries in a row landed
 * in one block and a client start failed. So only the first try takes the system's choice; every
 * further try is a port drawn at random from the dynamic range, which leaves a block at once. */
enum { kPortAttempts = 64, kDynamicFirst = 49152, kDynamicCount = 16384 };

static int RandomPort(unsigned &seed)
{
    seed = seed * 1103515245u + 12345u;
    return kDynamicFirst + (int)((seed >> 8) % kDynamicCount);
}

int NetRun(Server &server, const NetConfig &cfg, volatile long *stop, volatile long *boundPort, volatile long *boundAlt)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 1;
    SOCKET tcp = INVALID_SOCKET, udp[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };
    int port = 0, alt = cfg.altPort;
    /* The primary port: TCP + UDP on 127.0.0.1 and the advertised changed-address 127.0.0.2:<port>.
     * An explicit port is taken as it is (a UDP twin that cannot be bound is logged, as before); a
     * free port must give all of them, so the client's NAT discovery always has its answers. */
    const char *step = "", *refused = "";
    int attempts = 0, refusedPort = 0, refusedError = 0, err = 0;
    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);
    unsigned seed = (unsigned)tick.LowPart ^ (GetCurrentProcessId() * 2654435761u) ^ GetTickCount();
    for (int attempt = 0; attempt < (cfg.port ? 1 : (int)kPortAttempts); attempt++) {
        attempts++;
        err = 0;
        step = "tcp listen";
        port = cfg.port ? cfg.port : attempt || cfg.pickOwn ? RandomPort(seed) : 0;
        tcp = ListenTcp(port, err);
        if (tcp == INVALID_SOCKET) { refused = step; refusedPort = port; refusedError = err; continue; }
        port = BoundPort(tcp);
        step = "udp 127.0.0.1";
        udp[0] = BindUdp("127.0.0.1", port, err);
        if (udp[0] != INVALID_SOCKET) step = "udp 127.0.0.2";
        udp[2] = cfg.stun && (cfg.port || udp[0] != INVALID_SOCKET) ? BindUdp("127.0.0.2", port, err) : INVALID_SOCKET;
        if (cfg.port || (udp[0] != INVALID_SOCKET && (!cfg.stun || udp[2] != INVALID_SOCKET))) break;
        refused = step; refusedPort = port; refusedError = err;
        if (udp[0] != INVALID_SOCKET) closesocket(udp[0]);
        if (udp[2] != INVALID_SOCKET) closesocket(udp[2]);
        closesocket(tcp);
        tcp = udp[0] = udp[2] = INVALID_SOCKET;
    }
    if (tcp == INVALID_SOCKET) {
        server.Log(cfg.port ? Fmt("net: cannot listen on tcp 127.0.0.1:%d (%d)", cfg.port, err)
                            : Fmt("net: no free loopback port for the lobby after %d tries (last: %s of port %d refused with error %d)", attempts,
                                  refused, refusedPort, refusedError));
        WSACleanup();
        return 2;
    }
    if (attempts > 1)
        server.Log(Fmt("net: the free port was found at try %d (before it: %s of port %d refused with error %d)", attempts, refused, refusedPort,
                       refusedError));
    /* The alternate pair: 127.0.0.2:<alt> (test 2, "changed IP and port") and 127.0.0.1:<alt> (test 3). */
    if (cfg.stun && cfg.altPort) {
        udp[1] = BindUdp("127.0.0.2", cfg.altPort, err);
        udp[3] = BindUdp("127.0.0.1", cfg.altPort, err);
    } else if (cfg.stun) {
        for (int attempt = 0; attempt < (int)kPortAttempts; attempt++) {
            udp[1] = BindUdp("127.0.0.2", attempt || cfg.pickOwn ? RandomPort(seed) : 0, err);
            if (udp[1] == INVALID_SOCKET) continue;
            alt = BoundPort(udp[1]);
            udp[3] = alt != port ? BindUdp("127.0.0.1", alt, err) : INVALID_SOCKET;
            if (udp[3] != INVALID_SOCKET) break;
            closesocket(udp[1]);
            udp[1] = INVALID_SOCKET;
        }
    }
    /* What the changed-address STUN replies advertise is the pair that was actually obtained. */
    server.cfg.stunPort = port;
    if (cfg.stun) server.cfg.stunAltPort = alt;
    server.Log(Fmt("net: listening on 127.0.0.1:%d tcp%s%s; stun alt %s:%d%s", port, udp[0] != INVALID_SOCKET ? "+udp" : " (udp bind FAILED)",
                   cfg.port ? "" : " (free port)", "127.0.0.2", alt,
                   (udp[1] == INVALID_SOCKET || udp[2] == INVALID_SOCKET || udp[3] == INVALID_SOCKET) && cfg.stun ? " (an alternate socket FAILED)" : ""));
    if (boundAlt) InterlockedExchange(boundAlt, alt);
    if (boundPort) InterlockedExchange(boundPort, port);

    std::vector<NetConn> conns;
    int stunLogged = 0;
    DWORD lastTick = GetTickCount();
    while (!stop || !InterlockedCompareExchange(stop, 0, 0)) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(tcp, &rd);
        for (int i = 0; i < 4; i++) if (udp[i] != INVALID_SOCKET) FD_SET(udp[i], &rd);
        for (size_t i = 0; i < conns.size(); i++) FD_SET(conns[i].sock, &rd);
        timeval tv = { 0, 200000 };
        int ready = select(0, &rd, NULL, NULL, &tv);
        if (ready == SOCKET_ERROR) { Sleep(50); continue; }
        if (ready > 0 && FD_ISSET(tcp, &rd)) {
            sockaddr_in from;
            int flen = sizeof(from);
            SOCKET s = accept(tcp, (sockaddr *)&from, &flen);
            if (s != INVALID_SOCKET) {
                if ((ntohl(from.sin_addr.s_addr) >> 24) != 127 || conns.size() >= 60) {
                    closesocket(s); /* loopback only; FD_SETSIZE bounds the rest */
                } else {
                    NetConn nc;
                    nc.sock = s;
                    nc.conn = server.Open();
                    conns.push_back(nc);
                }
            }
        }
        for (int i = 0; ready > 0 && i < 4; i++) {
            if (udp[i] == INVALID_SOCKET || !FD_ISSET(udp[i], &rd)) continue;
            char data[2048];
            sockaddr_in from;
            int flen = sizeof(from);
            int n = recvfrom(udp[i], data, sizeof(data), 0, (sockaddr *)&from, &flen);
            if (n <= 0 || (ntohl(from.sin_addr.s_addr) >> 24) != 127) continue;
            Bytes out;
            int via = 0;
            std::string what;
            if (!StunReply(server.cfg, Bytes(data, (size_t)n), out, via, what)) continue;
            SOCKET reply = udp[i];
            if (via == 3 && udp[1] != INVALID_SOCKET) reply = udp[1];
            else if (via == 2 && udp[3] != INVALID_SOCKET) reply = udp[3];
            /* The first few only: they show that the client's NAT discovery found this listener. */
            if (stunLogged < 12) {
                static const char *const sockName[4] = { "127.0.0.1:port", "127.0.0.2:alt", "127.0.0.2:port", "127.0.0.1:alt" };
                stunLogged++;
                server.Log(Fmt("stun: %s on %s from 127.x:%d, answered from %s", what.c_str(), sockName[i], ntohs(from.sin_port),
                               reply == udp[1] ? sockName[1] : reply == udp[3] ? sockName[3] : sockName[i]));
            }
            sendto(reply, out.data(), (int)out.size(), 0, (sockaddr *)&from, flen);
        }
        for (size_t i = 0; ready > 0 && i < conns.size(); i++) {
            if (!FD_ISSET(conns[i].sock, &rd)) continue;
            char data[65536];
            int n = recv(conns[i].sock, data, sizeof(data), 0);
            if (n <= 0) conns[i].conn->closing = true;
            else server.Feed(conns[i].conn, data, (size_t)n);
        }
        DWORD now = GetTickCount();
        if (now - lastTick >= 1000) { lastTick = now; server.Tick(); }
        /* A handler may have queued output on any connection (room fan-out), so drain them all. */
        for (size_t i = 0; i < conns.size();) {
            Conn *c = conns[i].conn;
            if (!c->outbuf.empty()) {
                if (!SendAll(conns[i].sock, c->outbuf)) c->closing = true;
                c->outbuf.clear();
                c->lastTx = server.host->Mono();
            }
            if (c->closing) {
                closesocket(conns[i].sock);
                server.Close(c);
                conns.erase(conns.begin() + (long)i);
            } else {
                i++;
            }
        }
    }
    for (size_t i = 0; i < conns.size(); i++) { closesocket(conns[i].sock); server.Close(conns[i].conn); }
    closesocket(tcp);
    for (int i = 0; i < 4; i++) if (udp[i] != INVALID_SOCKET) closesocket(udp[i]);
    WSACleanup();
    return 0;
}

} // namespace dw
