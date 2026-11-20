/* Player-only playfx method, IW4 5dd1c0 -> shared factory40aa80.
 * Native event/angle layout is IW3; the CoDOL snapshot adapters translate it.
 * Do not copy IW4 event numbers or entity field offsets into native structs. */
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "server.h"
#include "q_math.h"
#include "configstrings.h"
#include "script_entity_fx.h"
#include "bg.h"
#include "ol_events.h"
#include "sv_assets.h"
#include "cscr_variable.h"
#include "cscr_stringlist.h"
#include <string.h>

void Scr_ReserveEntityFx(void)
{
    int effect,slot;
    if(Scr_GetNumParam()!=1) {
        Scr_Error("codol_reserveEntityFx requires one precached effect"); return;
    }
    effect=Scr_GetInt(0);
    slot=effect>0 ? ReserveServerEntityEffect((unsigned)effect) : -1;
    if(slot<=0) {
        Scr_Error("codol_reserveEntityFx requires map initialization before any gamestate and an available unpinned byte effect slot");
        return;
    }
    Scr_AddInt(effect); /* retain the native identity accepted by spawnFx */
}

void PlayerCmd_ViewmodelFXReady(scr_entref_t ref)
{
    playerState_t *ps;
    if(ref.classnum || ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError("viewmodel FX readiness requires a player"); return;
    }
    if(Scr_GetNumParam()) { Scr_Error("codol_viewmodelFXReady takes no arguments"); return; }
    ps=&g_entities[ref.entnum].client->ps;
    Scr_AddBool(g_entities[ref.entnum].health>0 && ps->viewlocked==0 &&
                (ps->weaponstate==0 || ps->weaponstate==5));
}

extern gentity_t *G_TempEntity(const float *origin, int event);
/* Retained assembler receives argc in EAX, basis in EDX, output in ECX. */
extern void __regparm3 Scr_SetFxAngles(int axisCount, vec3_t axis[3], float *angles);
extern unsigned int __cdecl Scr_GetConstLowercaseString(unsigned int);
extern int __cdecl SV_DObjGetBoneIndex(gentity_t *,unsigned int);
extern void __cdecl SV_DObjDumpInfo(gentity_t *);
extern void __cdecl G_AddEvent(gentity_t *,int,int);

static int add_fx_recipient(unsigned object,uint32_t *audience)
{
    scr_entref_t ref;
    if(Scr_GetObjectType(object)!=VAR_ENTITY) {
        Scr_ParamError(3,"expected a player entity"); return 0;
    }
    ref=Scr_GetEntityIdRef(object);
    if(ref.classnum || ref.entnum>=32 || !g_entities[ref.entnum].client) {
        Scr_ParamError(3,"expected a player entity"); return 0;
    }
    *audience|=1u<<ref.entnum; return 1;
}

/* IW4 5f6190 ->43f170. Its client masks are embedded in a tagged-effect
 * configstring. C1 event89 consumes an ordinary effect/tag compound, so keep
 * the audience native and filter each recipient's queued event instead. */
void Scr_PlayFXOnTagForClients(void)
{
    int effect,identity;
    gentity_t *target;
    unsigned tag,recipients,child;
    const char *tagName;
    uint32_t audience=0;
    if(Scr_GetNumParam()<4) {
        Scr_Error("Incorrect number of parameters"); return;
    }
    effect=Scr_GetInt(0);
    target=Scr_GetEntity(1);
    tag=Scr_GetConstLowercaseString(2);
    tagName=SL_ConvertToString(tag);
    if(strchr(tagName,'"')) {
        Scr_ParamError(2,"cannot use \" char"); return;
    }
    recipients=Scr_GetObject(3);
    if(Scr_GetObjectType(recipients)==VAR_ARRAY) {
        if(GetArraySize(recipients)>18) {
            Scr_ParamError(3,"player array exceeds 18 entries"); return;
        }
        for(child=FindFirstSibling(recipients);child;child=FindNextSibling(child)) {
            if(GetValueType(child)!=VAR_POINTER) {
                Scr_ParamError(3,"expected an array of player entities"); return;
            }
            if(!add_fx_recipient(GetVariableValueAddress(child)->pointerValue,&audience)) return;
        }
    } else if(!add_fx_recipient(recipients,&audience)) return;
    /* Expanded CoDOL effect range, as in the ordinary native tagged producer.
     * IW4 validates in its factory, after parsing the audience. */
    if(effect<=0 || effect>=EFFECT_SLOTS) {
        Scr_ParamError(0,va("effect id %i is invalid",effect)); return;
    }
    if(!target->model) {
        Scr_ParamError(1,"cannot play fx on entity with no model"); return;
    }
    if(SV_DObjGetBoneIndex(target,tag)<0) {
        SV_DObjDumpInfo(target);
        Scr_ParamError(2,va("tag %s does not exist on model %s",tagName,
            SL_ConvertToString(G_ModelName(target->model)))); return;
    }
    identity=RegisterMaskedTaggedEffect((unsigned)effect,tagName,audience);
    if(identity<=0) {
        Scr_Error("masked tagged-effect registry exhausted or invalid"); return;
    }
    /* Empty arrays still queue an event with an empty audience, like IW4.
     * Both native G_AddEvent branches preserve the full parameter word. */
    G_AddEvent(target,EV_PLAY_FX_ON_TAG_FOR_CLIENTS,identity);
}

void PlayerCmd_PlayFX(scr_entref_t ref)
{
    unsigned count = Scr_GetNumParam();
    int effect, axisCount = 0;
    vec3_t origin, axis[3] = {{0}};
    gentity_t *event;

    if(ref.classnum) { Scr_ObjectError("not an entity"); return; }
    if(ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("entity %i is not a player", ref.entnum));
        return;
    }
    if(count < 2 || count > 4) {
        Scr_Error("Incorrect number of parameters");
        return;
    }
    effect = Scr_GetInt(0);
    Scr_GetVector(1, origin);
    if(count > 2) {
        Scr_GetVector(2, axis[0]);
        axisCount = 1;
    }
    if(count > 3) {
        Scr_GetVector(3, axis[2]);
        axisCount = 2;
    }
    /* IW4 reads every supplied vector before its shared factory validates
     * the effect. CoDOL's event adapter carries the expanded1..319 bank. */
    if(effect <= 0 || effect >= EFFECT_SLOTS) {
        Scr_Error(va("Scr_PlayFX: invalid effect id %d", effect));
        return;
    }
    event = G_TempEntity(origin, EV_PLAY_FX);
    event->s.eventParm = effect;
    if(axisCount && Vec3Normalize(axis[0]) == 0.0f) {
        Scr_ParamError(2, "playfx called with zero forward vector");
        return;
    }
    if(axisCount == 2 && Vec3Normalize(axis[2]) == 0.0f) {
        Scr_ParamError(3, "playfx called with zero up vector");
        return;
    }
    /* IW4 48d5f0: default270 pitch; one axis uses vectoangles; two axes
     * project up perpendicular to forward, normalize, cross and convert.
     * Retained helper implements these same branches and collinearity error. */
    Scr_SetFxAngles(axisCount, axis, event->s.lerp.apos.trBase);
    /* A SET mask bit suppresses that recipient (sv_snapshot.c). Extend IW4's
     * single complement mask to both native words, then admit only receiver.
     * Snapshot archives already capture both words with the entity. */
    memset(event->r.clientMask, 0xff, sizeof(event->r.clientMask));
    event->r.clientMask[ref.entnum >> 5] &= ~(1u << (ref.entnum & 31));
    Scr_AddEntity(event);
}
/* C1 PlayViewModelFXOnTag 544ff0 / StopViewModelFXOnTag 545230: the player
 * methods T5's client-script PlayViewmodelFx became in C1. Each takes an
 * effect id and a viewmodel tag (never checked against the server model, which
 * has no viewmodel), registers the "%03d%s" compound in the tagged-effect bank
 * at 545109 / 54536b, and queues event 92 / 93 on a temp entity at the player
 * (G_TempEntity 572c00 at 545202) whose clientMask ~(1 << entnum) (54520e)
 * admits only that player; the slot rides in +ac (545217).
 *
 * C1 also writes a hand selector into +c4 (545221): Play's optional third
 * argument "right" (0) or "left" (1); Stop's optional third argument (bit 0)
 * and fourth, the hand (bit 1). The wire field is not published yet, so only
 * the defaults -- the right hand, bit 0 clear -- are accepted. */
static int viewmodel_fx_player(scr_entref_t ref,unsigned minimum,unsigned maximum,const char *name)
{
    unsigned count=Scr_GetNumParam();
    if(ref.classnum) { Scr_ObjectError("not an entity"); return 0; }
    if(ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("entity %i is not a player",ref.entnum)); return 0;
    }
    if(count<minimum || count>maximum) {
        Scr_Error(va("Invalid params to %s",name)); return 0;
    }
    return 1;
}
static int viewmodel_fx_hand(unsigned index,const char *name)
{
    const char *hand=Scr_GetString(index);
    if(!Q_stricmp(hand,"right")) return 1;
    if(!Q_stricmp(hand,"left")) {
        Scr_ParamError(index,va("%s: the left hand is not supported by this server yet",name)); return 0;
    }
    Scr_ParamError(index,va("%s: Hand must be 'right' or 'left'",name)); return 0;
}
static void viewmodel_fx_event(scr_entref_t ref,int event,const char *name)
{
    gentity_t *player=&g_entities[ref.entnum],*temp;
    int effect=Scr_GetInt(0),tagged;
    const char *tagName=SL_ConvertToString(Scr_GetConstLowercaseString(1));
    if(effect<=0 || effect>=EFFECT_SLOTS) {
        Scr_ParamError(0,va("effect id %i is invalid",effect)); return;
    }
    if(!tagName || !*tagName || strchr(tagName,'"')) {
        Scr_ParamError(1,va("%s: invalid tag",name)); return;
    }
    tagged=RegisterNativeTaggedEffect((unsigned)effect,tagName);
    if(tagged<=0) {
        Scr_ParamError(0,va("could not register effect %i on tag %s",effect,tagName)); return;
    }
    temp=G_TempEntity(player->r.currentOrigin,event);
    temp->s.eventParm=tagged;
    /* A SET mask bit suppresses that recipient, as in PlayerCmd_PlayFX. */
    memset(temp->r.clientMask,0xff,sizeof(temp->r.clientMask));
    temp->r.clientMask[ref.entnum>>5]&=~(1u<<(ref.entnum&31));
}
void PlayerCmd_PlayViewModelFXOnTag(scr_entref_t ref)
{
    if(!viewmodel_fx_player(ref,2,3,"PlayViewModelFXOnTag")) return;
    if(Scr_GetNumParam()==3 && !viewmodel_fx_hand(2,"PlayViewModelFXOnTag")) return;
    viewmodel_fx_event(ref,EV_PLAY_VIEWMODEL_FX_ON_TAG,"PlayViewModelFXOnTag");
}
void PlayerCmd_StopViewModelFXOnTag(scr_entref_t ref)
{
    unsigned count;
    if(!viewmodel_fx_player(ref,2,4,"StopViewModelFXOnTag")) return;
    count=Scr_GetNumParam();
    if(count>=3 && (Scr_GetInt(2)&1)) {
        Scr_ParamError(2,"StopViewModelFXOnTag: this flag is not supported by this server yet"); return;
    }
    if(count==4 && !viewmodel_fx_hand(3,"StopViewModelFXOnTag")) return;
    viewmodel_fx_event(ref,EV_STOP_VIEWMODEL_FX_ON_TAG,"StopViewModelFXOnTag");
}
#endif
