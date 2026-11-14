#ifndef K33_P2P_MATCH_OPTIONS_H
#define K33_P2P_MATCH_OPTIONS_H
#include <string.h>
typedef struct {
    char map[64], session[64], gametype[32], category[16];
    int difficultyPresent,difficulty;
} P2PRoomContext;
static int P2PMatchIdent(const char *s, size_t maximum)
{
    size_t n = s ? strlen(s) : 0, i;
    if (!n || n > maximum || *s < 'a' || *s > 'z') return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_')) return 0;
    return 1;
}
/* Private movie-to-engine context, independent of the enable/disable command. */
static const char *P2PParseRoomContext(int argc, const char *const *argv, P2PRoomContext *out)
{
    P2PRoomContext next = {0}; const char *base;
    next.difficulty=-1;
    if (argc != 3) return "Choose a match lobby first.";
    if (!P2PMatchIdent(argv[0],63) || !P2PMatchIdent(argv[1],31)) return "Invalid selected map or mode.";
    if (strcmp(argv[2],"mp") && strcmp(argv[2],"zm") && strcmp(argv[2],"coop") && strcmp(argv[2],"survival"))
        return "Invalid match category.";
    strcpy(next.session,argv[0]);strcpy(next.gametype,argv[1]);strcpy(next.category,argv[2]);
    base=argv[0];
    if (!strncmp(base,"coop_",5)) {
        const char *level=strstr(base,"_mp_");
        if (!level) return "This mission has no MP base level.";
        base=level+1;
    }
    if (!strcmp(next.category,"zm")) {
        if (strncmp(base,"zm_",3)) return "Zombies needs a Zombies map.";
    } else if (strncmp(base,"mp_",3)) return "This mode needs an MP base map.";
    if (!strcmp(next.category,"mp") && strcmp(base,next.session)) return "Multiplayer needs an MP map.";
    if (!strcmp(next.category,"survival") && strcmp(base,next.session) && strncmp(next.session,"coop_survival_",14))
        return "Survival needs a Survival map.";
    if (!strcmp(next.category,"coop") && !strncmp(next.session,"coop_survival_",14)) return "This map belongs to Survival.";
    strcpy(next.map,base);
    if (!strcmp(next.gametype,"tdm")) strcpy(next.gametype,"war");
    else if (!strcmp(next.gametype,"ffa")) strcpy(next.gametype,"dm");
    else if (!strcmp(next.gametype,"hq")) strcpy(next.gametype,"koth");
    if (!strcmp(next.category,"survival")) {
        strcpy(next.gametype,"survival");
        strcpy(next.session,next.map); /* Rules on the unchanged MP level. */
    } else if (!strcmp(next.category,"coop")) strcpy(next.gametype,"coop");
    *out=next;return NULL;
}
#endif
