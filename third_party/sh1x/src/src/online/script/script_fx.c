#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "server.h"
#include "sv_state.h"
#include "configstrings.h"

void Scr_PrecacheFxTeamThermal(void)
{
    int index; char effect[1024]; const char *tag;
    if(!level.initializing) {
        Scr_Error("PrecacheFxTeamThermal must be called before any wait statements"); return;
    }
    if(Scr_GetNumParam()!=2) {
        Scr_Error("PrecacheFxTeamThermal requires an effect ID and attachment tag"); return;
    }
    index=Scr_GetInt(0); tag=Scr_GetString(1);
    if(index<=0 || index>=EFFECT_SLOTS) {
        Scr_Error("PrecacheFxTeamThermal: invalid retained effect ID"); return;
    }
    GetNativeEffectName((unsigned)index,effect,sizeof(effect));
    if(!*effect || RegisterTeamThermal(ServerConfigstrings(),
        ServerInheritedConfigstrings(),effect,tag)<0)
        Scr_Error("PrecacheFxTeamThermal: invalid or exhausted effect/thermal configstrings");
}
#endif
