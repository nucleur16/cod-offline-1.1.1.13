/*
 * dw_api.cpp - the C boundary of the offline lobby: owns the listener thread and adapts the
 * embedder's C callbacks to dw::Host. See dw_api.h.
 */
#include <string.h>
#include <windows.h>
#include "dw.h"
#include "dw_api.h"
#include "dw_net.h"

namespace {

struct ApiHost : dw::Host {
    DwOfflineConfig cb;
    virtual void Log(const std::string &line) { if (cb.log) cb.log(line.c_str()); }
    virtual int SurvivalSupported(const std::string &level) { return cb.survivalSupported ? cb.survivalSupported(level.c_str()) : -1; }
    virtual double Mono() { return GetTickCount64() / 1000.0; }
    virtual bool GetLocalRoute(uint64_t uid, dw::LocalRoute &out)
    {
        if (!cb.localRoute) return false;
        DwOfflineRoute r;
        memset(&r, 0, sizeof(r));
        if (!cb.localRoute(uid, &r)) return false;
        r.map[sizeof(r.map) - 1] = r.sessionMap[sizeof(r.sessionMap) - 1] = 0;
        r.gametype[sizeof(r.gametype) - 1] = r.category[sizeof(r.category) - 1] = 0;
        r.state[sizeof(r.state) - 1] = r.error[sizeof(r.error) - 1] = 0;
        out.ownerUid = r.ownerUid;
        out.enabled = r.enabled != 0;
        out.armed = r.armed != 0;
        out.launched = r.launched != 0;
        out.ready = r.ready != 0;
        out.failed = r.failed != 0;
        out.state = r.state;
        out.error = r.error;
        out.port = r.port;
        out.map = r.map;
        out.sessionMap = r.sessionMap;
        out.gametype = r.gametype;
        out.category = r.category;
        return true;
    }
    virtual bool LocalPlayStart(uint64_t ownerUid, const std::string &sessionMap, const std::string &gametype, const std::string &type,
                                int difficulty, bool launch, int &port, std::string &error)
    {
        char why[256] = "";
        int p = 0;
        if (!cb.start) { error = "no embedded match host"; return false; }
        int ok = cb.start(ownerUid, sessionMap.c_str(), gametype.c_str(), type.c_str(), difficulty, launch ? 1 : 0, &p, why, (int)sizeof(why));
        why[sizeof(why) - 1] = 0;
        port = p;
        error = why;
        if (!ok && error.empty()) error = "the embedded match host refused the selection";
        return ok != 0;
    }
    virtual std::string DisplayName(const std::string &profileName)
    {
        char name[64];
        if (!cb.displayName || profileName.empty()) return "";
        name[0] = 0;
        if (!cb.displayName(profileName.c_str(), name, (int)sizeof(name))) return "";
        name[sizeof(name) - 1] = 0;
        return name;
    }
    virtual bool StartNames(const std::string &map, const std::string &gametype)
    {
        return cb.startNames && cb.startNames(map.c_str(), gametype.c_str()) != 0;
    }
    virtual void LocalPlayLeave(uint64_t ownerUid)
    {
        if (cb.leave) cb.leave(ownerUid);
    }
    virtual void OnStartGame(const dw::StartInfo &info)
    {
        if (!cb.startGame) return;
        DwOfflineStartInfo o;
        memset(&o, 0, sizeof(o));
        o.uid = info.uid;
        strncpy(o.playerName, info.playerName.c_str(), sizeof(o.playerName) - 1);
        strncpy(o.map, info.map.c_str(), sizeof(o.map) - 1);
        strncpy(o.sessionMap, info.sessionMap.c_str(), sizeof(o.sessionMap) - 1);
        strncpy(o.gametype, info.gametype.c_str(), sizeof(o.gametype) - 1);
        strncpy(o.zoneType, info.zoneType.c_str(), sizeof(o.zoneType) - 1);
        o.hostPort = info.hostPort;
        o.local = info.local ? 1 : 0;
        o.difficulty = info.difficulty;
        cb.startGame(&o);
    }
};

struct Runtime {
    ApiHost host;
    dw::Config cfg;
    dw::NetConfig net;
    HANDLE thread;
    volatile long stop, port, altPort, failed;
};
Runtime *g_rt;

DWORD WINAPI ListenerThread(LPVOID)
{
    Runtime *rt = g_rt;
    dw::Server *server = new dw::Server(rt->cfg, &rt->host);
    int rc = dw::NetRun(*server, rt->net, &rt->stop, &rt->port, &rt->altPort);
    if (rc) InterlockedExchange(&rt->failed, 1);
    delete server;
    return (DWORD)rc;
}

std::string Str(const char *s) { return s ? s : ""; }

} // namespace

extern "C" int DwOfflineStartServer(const DwOfflineConfig *config)
{
    if (g_rt || !config || !config->profile || !config->state) return 0;
    /* 0 = a free port (dw_net.cpp); an explicit one must be an unprivileged port */
    if (config->port && (config->port < 1024 || config->port > 65535)) return 0;
    if (config->altPort && (config->altPort < 1024 || config->altPort > 65535)) return 0;
    Runtime *rt = new Runtime();
    rt->host.cb = *config;
    rt->cfg.resources = Str(config->resources);
    rt->cfg.profile = Str(config->profile);
    rt->cfg.localName = Str(config->playerName);
    rt->cfg.legacyStore = Str(config->legacyStore);
    rt->cfg.state = Str(config->state);
    rt->cfg.matchProfile = Str(config->matchProfile);
    rt->cfg.zoneDir = Str(config->zoneDir);
    rt->cfg.gameDir = Str(config->gameDir);
    rt->cfg.mmHostIp = "127.0.0.1";
    rt->cfg.mmHostPort = config->hostPort;
    /* stunPort / stunAltPort are set by the listener to the ports it obtained. stunMappedPort (the
     * "your address" port of a STUN reply) stays the Python lobby's constant 3074 whatever port the
     * lobby itself listens on: it describes the client, not the lobby. */
    rt->cfg.stunPort = config->port;
    rt->cfg.stunAltPort = config->altPort;
    rt->net.port = config->port;
    rt->net.altPort = config->altPort;
    rt->net.stun = true;
    rt->stop = rt->port = rt->altPort = rt->failed = 0;
    g_rt = rt;
    rt->thread = CreateThread(NULL, 0, ListenerThread, NULL, 0, NULL);
    if (!rt->thread) { g_rt = NULL; delete rt; return 0; }
    for (int i = 0; i < 300; i++) {
        long port = InterlockedCompareExchange(&rt->port, 0, 0);
        if (port) return (int)port;
        if (InterlockedCompareExchange(&rt->failed, 0, 0) || WaitForSingleObject(rt->thread, 10) == WAIT_OBJECT_0) break;
    }
    /* Not listening: stop it; the caller refuses offline mode. The Runtime is leaked on purpose if
     * the thread does not end in time (it still owns it). */
    InterlockedExchange(&rt->stop, 1);
    if (WaitForSingleObject(rt->thread, 3000) == WAIT_OBJECT_0) { CloseHandle(rt->thread); g_rt = NULL; delete rt; }
    return 0;
}

extern "C" void DwOfflineStopServer(void)
{
    Runtime *rt = g_rt;
    if (!rt) return;
    InterlockedExchange(&rt->stop, 1);
    if (WaitForSingleObject(rt->thread, 5000) == WAIT_OBJECT_0) {
        CloseHandle(rt->thread);
        g_rt = NULL;
        delete rt;
    }
}

extern "C" int DwOfflineAltPort(void)
{
    Runtime *rt = g_rt;
    return rt ? (int)InterlockedCompareExchange(&rt->altPort, 0, 0) : 0;
}

extern "C" int DwOfflineRunning(void)
{
    Runtime *rt = g_rt;
    return rt && InterlockedCompareExchange(&rt->port, 0, 0) && WaitForSingleObject(rt->thread, 0) == WAIT_TIMEOUT;
}
