/*
 * dw_stubs.cpp - placeholders for modules that are not part of a build.
 *
 * build-offline.inc.sh defines DW_HAVE_<FILE> for every offline/dw_<file>.cpp it compiles (and not
 * for a file listed in OFFLINE_SKIP), so a partial build - one service under test, or the port in
 * progress - still links. A task answered from here is the Python lobby's empty-success
 * fall-through. A complete build compiles every module and none of these.
 */
#include <set>
#include "dw.h"
#include "dw_state.h"

namespace dw {

#ifndef DW_HAVE_STORE
void StoreInit(Server &) {}
void StoreFree(Server &) {}
#endif
#ifndef DW_HAVE_STORAGE
Reply StorageTask(Server &, Conn &, int, const Vals &) { return Reply(); }
void OnUidLatched(Server &, Conn &) {}
#endif
#ifndef DW_HAVE_PROFILES
void ProfilesInit(Server &) {}
void ProfilesFree(Server &) {}
std::string EnsureBlob(Server &, uint64_t) { return ""; }
int PlayerLevel(Server &, uint64_t) { return 1; }
std::string ExportMatchBlob(Server &, uint64_t) { return ""; }
void SetMirrorOwner(Server &, uint64_t) {}
bool ProfileLoad(Server &, uint64_t, Json &, bool) { return false; } /* dw_profiles.h; no profile */
std::string ProfilesImportLegacy(Server &) { return ""; }
#endif
#ifndef DW_HAVE_PLAYERDATA
std::set<int> PdEquippedItemIds(Server &, const Bytes &) { return std::set<int>(); } /* dw_playerdata.h; nothing equipped */
#endif
#ifndef DW_HAVE_ROOMS
Reply RoomTask(Server &, Conn &, int, const Vals &) { return Reply(); }
void RoomsInit(Server &) {}
void RoomsFree(Server &) {}
void RoomLeaveAll(Server &, Conn &) {}
#endif
#ifndef DW_HAVE_MATCHMAKING
Reply MatchmakingTask(Server &, Conn &, int, const Vals &) { return Reply(); }
#endif
#ifndef DW_HAVE_SESSION
void OnLobbyHandshake(Server &, Conn &) {}
void MatchRelease(Server &, Conn &) {}
std::string LivePlayerName(Server &, Conn *) { return ""; }
#endif
#ifndef DW_HAVE_DEVPANEL
Reply DevPanelInstantMessage(Server &, Conn &, int, const Vals &) { return Reply(); }
#endif
#ifndef DW_HAVE_MARKETPLACE
Reply MarketplaceTask(Server &, Conn &, int, const Vals &) { return Reply(); }
void MarketInit(Server &) {}
void MarketFree(Server &) {}
#endif
#ifndef DW_HAVE_PUSHES
void AfterTask(Server &, Conn &, int, int) {}
#endif
#ifndef DW_HAVE_STATS
Reply StatsTask(Server &, Conn &, int, const Vals &) { return Reply(); }
#endif
#ifndef DW_HAVE_CAREER
void CareerInit(Server &) {}
void CareerFree(Server &) {}
void ServiceTick(Server &) {}
std::string RankWritebackMaybeApply(Server &, uint64_t, bool) { return ""; }
std::string ProgressionCredit(Server &, Json &, const Json &) { return ""; }
void ProgressionMastery(Server &, const Json &, std::map<int, double> &) {}
#endif
#ifndef DW_HAVE_EMBEDDED
const EmbeddedFile kEmbedded[] = { { "", 0, 0, 0, 0, 0 } };
const int kEmbeddedCount = 0;
#endif

} // namespace dw
