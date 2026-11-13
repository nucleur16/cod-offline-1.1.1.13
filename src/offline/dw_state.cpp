/* dw_state.cpp - server state lifecycle and the connection-close hook (dw-capture.py serve() finally). */
#include "dw.h"
#include "dw_state.h"

namespace dw {

void StateInit(Server &s)
{
    s.st = new ServerState();
    StoreInit(s);
    ProfilesInit(s);
    CareerInit(s);
    MarketInit(s);
    RoomsInit(s);
    /* Config::legacyStore: the careers of the first offline builds, once (dw_profiles.cpp). */
    std::string imported = ProfilesImportLegacy(s);
    if (!imported.empty()) s.Log(imported);
}

void StateFree(Server &s)
{
    if (!s.st) return;
    RoomsFree(s);
    MarketFree(s);
    CareerFree(s);
    ProfilesFree(s);
    StoreFree(s);
    delete s.st;
    s.st = NULL;
}

void OnConnClosed(Server &s, Conn &c)
{
    RoomLeaveAll(s, c);
    MatchRelease(s, c); /* MC-3: this connection no longer holds the match server */
}

} // namespace dw
