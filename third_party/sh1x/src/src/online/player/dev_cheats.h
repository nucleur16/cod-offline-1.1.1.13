/* Local developer cheat commands (noclip, god, give, ...) and the gate that
 * keeps them away from lobby/ranked matches.
 *
 * Gate: a cheat command, a cheat-protected dvar change or a bot-control dvar
 * takes effect only while CheatsPermitted() is true:
 *   - sv_cheats is 1 (devmap sets it, map clears it; the lobby's match
 *     dispatcher always sends map), and
 *   - the server is not a lobby match server: admissionRequired is 0
 *     (an isolated direct-connect/offline server) or the operator declared a
 *     developer server on the command line with +set devServer 1.
 *
 * Progression: once sv_cheats has been seen on, this server process records
 * no progression or stats at all (match report, matchdata record, the
 * server-side progress store and the in-match secure-profile push) until it
 * is restarted. CheatsTainted() is that latch.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef DEV_CHEATS_H
#define DEV_CHEATS_H
#ifdef OL_SERVER

struct gentity_s;

/* sv_cheats on and not a lobby match server. */
int CheatsPermitted(void);
/* Why cheats are refused right now (NULL when they are permitted). */
const char *CheatsRefusal(void);
/* 1 once sv_cheats has been on in this process: progression sinks refuse. */
int CheatsTainted(void);
/* Refuse a persistence sink while tainted; says once per sink why. */
int CheatsBlockProgression(const char *sink);

/* devmap on a lobby match server is refused (returns 0 after printing why). */
int DevMapAllowed(void);
/* map (not devmap): put back the server dvars devset changed. */
void DevCheatsRestoreDvars(void);

/* Client command verbs owned by this module (give, take, viewpos, devset,
 * addbot, kickbot, devhelp). 1 when the verb was one of them. */
int DevClientCommand(struct gentity_s *ent, const char *verb);
/* Plain-text game message to one client ("e", not localized). */
void DevPrint(struct gentity_s *ent, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* sv_botStop while cheats are permitted: 0 normal, 1 bots stand still and
 * hold fire, 2 bots move but never acquire or shoot. */
int DevBotStop(void);

/* Server console commands and dvars (dev_clientcmd, sv_botStop,
 * devServer). */
void RegisterDevCheatCommands(void);

#endif
#endif
