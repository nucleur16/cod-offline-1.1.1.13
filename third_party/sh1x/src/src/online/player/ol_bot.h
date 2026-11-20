#ifndef BOT_H
#define BOT_H
#include "player.h"
struct client_s;
void RegisterBotMethods(void);
/* 0 legacy bot, 1 command staged, 2 receiver disappeared during notify. */
int BotBuildCommand(struct client_s *,usercmd_t *);
int ClientIsBotClient(unsigned client);
/* Called only from the three native player_die early-return branches. */
void ReportDeathGateRefusal(struct gentity_s *victim, int gate);
#endif
