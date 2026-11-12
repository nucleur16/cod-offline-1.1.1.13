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
 * Every socket is bound to a loopback address; nothing here can listen on another interface.
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "dw.h"
#include "dw_net.h"

namespace dw {

struct NetConn { SOCKET sock; Conn *conn; };

static SOCKET BindUdp(const char *ip, int port)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return s;
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = inet_addr(ip);
    a.sin_port = htons((u_short)port);
    /* SO_EXCLUSIVEADDRUSE: nobody else may share (or steal) the loopback port. */
    BOOL on = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof(on));
    if (bind(s, (sockaddr *)&a, sizeof(a)) != 0) { closesocket(s); return INVALID_SOCKET; }
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

int NetRun(Server &server, const NetConfig &cfg, volatile long *stop, volatile long *boundPort)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 1;
    SOCKET tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((u_short)cfg.port);
    BOOL on = TRUE;
    setsockopt(tcp, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof(on));
    if (tcp == INVALID_SOCKET || bind(tcp, (sockaddr *)&a, sizeof(a)) != 0 || listen(tcp, SOMAXCONN) != 0) {
        server.Log(Fmt("net: cannot listen on tcp 127.0.0.1:%d (%d)", cfg.port, WSAGetLastError()));
        if (tcp != INVALID_SOCKET) closesocket(tcp);
        WSACleanup();
        return 2;
    }
    int alen = sizeof(a);
    getsockname(tcp, (sockaddr *)&a, &alen);
    int port = ntohs(a.sin_port);
    SOCKET udp[4];
    udp[0] = BindUdp("127.0.0.1", port);
    udp[1] = cfg.stun ? BindUdp("127.0.0.2", cfg.altPort) : INVALID_SOCKET; /* via 3 */
    udp[2] = cfg.stun ? BindUdp("127.0.0.2", port) : INVALID_SOCKET;        /* advertised changed-address */
    udp[3] = cfg.stun ? BindUdp("127.0.0.1", cfg.altPort) : INVALID_SOCKET; /* via 2 */
    server.Log(Fmt("net: listening on 127.0.0.1:%d tcp%s; stun alt %s:%d%s", port, udp[0] != INVALID_SOCKET ? "+udp" : " (udp bind FAILED)",
                   "127.0.0.2", cfg.altPort, (udp[1] == INVALID_SOCKET || udp[2] == INVALID_SOCKET || udp[3] == INVALID_SOCKET) && cfg.stun ? " (an alternate socket FAILED)" : ""));
    if (boundPort) InterlockedExchange(boundPort, port);

    std::vector<NetConn> conns;
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
