/*
 * dw_stubs.cpp - placeholders for services that are not ported yet. Each definition moves to its own
 * file as the port lands; a task answered from here is the Python lobby's empty-success fall-through.
 */
#include "dw.h"
#include "dw_state.h"

namespace dw {

#ifndef DW_HAVE_STORAGE
Reply StorageTask(Server &, Conn &, int, const Vals &) { return Reply(); }
void OnUidLatched(Server &, Conn &) {}
#endif
#ifndef DW_HAVE_ROOMS
Reply RoomTask(Server &, Conn &, int, const Vals &) { return Reply(); }
Reply MatchmakingTask(Server &, Conn &, int, const Vals &) { return Reply(); }
Reply DevPanelInstantMessage(Server &, Conn &, int, const Vals &) { return Reply(); }
void OnConnClosed(Server &, Conn &) {}
void OnLobbyHandshake(Server &, Conn &) {}
#endif
#ifndef DW_HAVE_MARKETPLACE
Reply MarketplaceTask(Server &, Conn &, int, const Vals &) { return Reply(); }
#endif
#ifndef DW_HAVE_STATS
Reply StatsTask(Server &, Conn &, int, const Vals &) { return Reply(); }
#endif
#ifndef DW_HAVE_PUSHES
void AfterTask(Server &, Conn &, int, int) {}
#endif
#ifndef DW_HAVE_CAREER
void ServiceTick(Server &) {}
#endif
#ifndef DW_HAVE_STATE
void StateInit(Server &s) { s.st = new ServerState(); }
void StateFree(Server &s) { delete s.st; s.st = NULL; }
#endif
#ifndef DW_HAVE_EMBEDDED
const EmbeddedFile kEmbedded[] = { { "", 0, 0, 0, 0, 0 } };
const int kEmbeddedCount = 0;
#endif

} // namespace dw
