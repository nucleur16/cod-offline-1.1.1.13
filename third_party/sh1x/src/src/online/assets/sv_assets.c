#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* CoDOL publication of retained engine asset names. */
#ifdef OL_SERVER
#include "script_player_builtins.h"
#include "server.h"
#include "g_shared.h"
#include "scr_vm.h"
#include "sv_state.h"
#include "engine_state.h"
#include "sv_weapons.h"
#include "clientstate_ext.h"
#include "weapon_state.h"
#include "player_worldweapon.h"
#include "weapon_flags.h"
#include "remote_vehicle.h"
#include "cscr_stringlist.h"
#include "effects.h"
#include "sv_assets.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
/* script_hud_notifications.c: the ACTION_SLOT_HELP store. */
unsigned ClientActionSlotHelp(unsigned client);
int PublishServerId(int32_t server_id)
{
    char value[16]; int i;
    const char *old = GetConfigstring(ServerConfigstrings(), CS_SERVER_ID);
    snprintf(value, sizeof(value), "%d", (int)server_id);
    if(old && !strcmp(old, value)) return 1;
    if(!SetServerId(server_id)) return 0;
    if(!svs.clients || !sv_maxclients) return 1;
    for(i = 0; i < sv_maxclients->integer; ++i)
        if(svs.clients[i].state >= CS_PRIMED && svs.clients[i].gamestateSent)
            SV_SendServerCommandNoLoss(&svs.clients[i], "d %u %s", CS_SERVER_ID, value);
    return 1;
}
_Static_assert(MAX_SOUNDS<=SOUND_SLOTS,"native sound IDs fit the CoDOL sound bank");

void BroadcastWeaponConfigstrings(unsigned first,unsigned count)
{
    unsigned i; int recipient;
    if(first>=WEAPON_SLOTS || count>WEAPON_SLOTS-first) return;
    for(i=first;i<first+count;++i) {
        const char *name=GetConfigstring(ServerConfigstrings(),CS_WEAPONS+i);
        if(!name || !*name) continue;
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",CS_WEAPONS+i,name);
        }
    }
    /* This reliable command must follow every newly appended definition:
     * the C1 registration loop dereferences all slots below the new count. */
    if(count) {
        const char *total=GetConfigstring(ServerConfigstrings(),CS_WEAPON_COUNT);
        if(total && *total) for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",CS_WEAPON_COUNT,total);
        }
    }

}

static unsigned model_strings[MAX_MODELS], tag_strings[MAX_TAGS];
static int model_slots[NATIVE_MODELS], tag_slots[MAX_TAGS];
/* Native model slots MAX_MODELS..NATIVE_MODELS-1: referenced script
 * strings, the IW3 configstring's role for the lower slots. */
static unsigned high_model_names[NATIVE_MODELS-MAX_MODELS];
static unsigned material_strings[MAX_MATERIALS],localized_strings[MAX_LOCALIZEDSTRINGS];
static int ol_material_slots[MAX_MATERIALS],localized_slots[MAX_LOCALIZEDSTRINGS];
/* Retained G_EffectIndex uses base0x63e and100 slots; C1 has320 slots. */
static unsigned effect_strings[100];
static int effect_slots[EFFECT_SLOTS];
static int byte_effect_slots[EFFECT_SLOTS];
static unsigned char reserved_byte_effects[BYTE_EFFECT_SLOTS];
static int RefreshInheritedRange(int localized);
/* Whether an inherited byte-bank effect is absent from this map's zones, asked once per level (reclaim_byte_effect_slot). */
static struct { const char *name; int absent; } inherited_byte_effects[BYTE_EFFECT_SLOTS];
/* Extra native slots cannot live at CS_FX+index: slot100 overlaps the
 * retained tagged-effect range. Their names belong to this map's registry. */
static char extra_effect_names[EFFECT_SLOTS-100][64];
static int tagged_effect_slots[256];
static struct { unsigned tagged_slot; uint32_t audience; } masked_tagged_effects[256];
static int weapon_map_warned;
/* Hint string slots 32..95 (see OL_G_GetHintStringIndex). */
static char high_hints[HINTSTRING_SLOTS-HINTSTRING_RETAINED][MAX_STRING_CHARS];
extern struct XModel *cached_models[NATIVE_MODELS];
struct XModel *__cdecl SV_XModelGet(const char *name);
unsigned int __cdecl SV_GetConfigstringConst(int index);
void ResetServerAssetNames(void)
{
    unsigned model;
    for(model=0;model<NATIVE_MODELS-MAX_MODELS;++model) {
        if(high_model_names[model]) SL_RemoveRefToString(high_model_names[model]);
        high_model_names[model]=0;
        cached_models[MAX_MODELS+model]=NULL;
    }
    memset(high_hints, 0, sizeof(high_hints));
    weapon_map_warned = 0;
    memset(model_strings, 0, sizeof(model_strings));
    memset(tag_strings, 0, sizeof(tag_strings));
    memset(model_slots, 0, sizeof(model_slots));
    memset(tag_slots, 0, sizeof(tag_slots));
    memset(material_strings,0,sizeof(material_strings));
    memset(localized_strings,0,sizeof(localized_strings));
    memset(ol_material_slots,0,sizeof(ol_material_slots));
    memset(localized_slots,0,sizeof(localized_slots));
    memset(effect_strings,0,sizeof(effect_strings));
    memset(extra_effect_names,0,sizeof(extra_effect_names));
    memset(effect_slots,0,sizeof(effect_slots));
    memset(byte_effect_slots,0,sizeof(byte_effect_slots));
    memset(reserved_byte_effects,0,sizeof(reserved_byte_effects));
    memset(inherited_byte_effects,0,sizeof(inherited_byte_effects));
    memset(tagged_effect_slots,0,sizeof(tagged_effect_slots));
    memset(masked_tagged_effects,0,sizeof(masked_tagged_effects));
}
int ServerModelToWire(unsigned index)
{ return index < NATIVE_MODELS && (!index || model_slots[index]) ? model_slots[index] : -1; }
/* G_ModelName for every native slot: the IW3 configstring below MAX_MODELS,
 * the CoDOL table above it, the empty string for a free slot. */
unsigned OL_G_ModelName(int index)
{
    if(index>=0 && index<MAX_MODELS) return SV_GetConfigstringConst(CS_MODELS+index);
    if(index>=MAX_MODELS && index<NATIVE_MODELS && high_model_names[index-MAX_MODELS])
        return high_model_names[index-MAX_MODELS];
    return sv.emptyConfigString;
}
/* The native slot >= MAX_MODELS already holding this lowercase script
 * string, or 0. */
int FindHighModel(unsigned handle)
{
    unsigned index;
    if(!handle || handle==sv.emptyConfigString) return 0;
    for(index=0;index<NATIVE_MODELS-MAX_MODELS;++index)
        if(high_model_names[index]==handle) return (int)(MAX_MODELS+index);
    return 0;
}
/* Allocate a native slot >= MAX_MODELS once the IW3 slots are full, with
 * G_ModelIndex's stores (cached XModel first, then the name) and the wire
 * registration RefreshAssetRange performs for the lower slots. 0 when
 * C1's 2048 are exhausted or the name cannot be published; nothing is left
 * half registered. */
int AllocHighModel(const char *name)
{
    configstrings_t *bank=ServerConfigstrings();
    const configstrings_t *inherited=ServerInheritedConfigstrings();
    unsigned index,handle;
    const char *p;
    int slot,recipient;
    struct XModel *model;
    if(!name || !*name || strlen(name)>=64 || !bank || !inherited) return 0;
    handle=SL_FindLowercaseString(name);
    if((slot=FindHighModel(handle))>0) return slot;
    for(p=name;*p;++p)
        if((unsigned char)*p<=32 || *p==127 || *p=='"' || *p==';' || *p=='\\') return 0;
    for(index=0;index<NATIVE_MODELS-MAX_MODELS;++index)
        if(!high_model_names[index]) break;
    if(index>=NATIVE_MODELS-MAX_MODELS) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL models: all %d native model slots are in use; '%s' refused\n",
            NATIVE_MODELS-1,name);
        return 0;
    }
    model=SV_XModelGet(name);
    if(!model) return 0;
    slot=RegisterInheritedModelName(bank,inherited,name);
    if(slot<1) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL models: C1 model bank cannot take '%s'\n",name);
        return 0;
    }
    cached_models[MAX_MODELS+index]=model;
    high_model_names[index]=SL_GetLowercaseString(name,0);
    model_slots[MAX_MODELS+index]=slot;
    if(svs.clients && sv_maxclients)
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",OL_CS_MODELS+(unsigned)slot,name);
        }
    return (int)(MAX_MODELS+index);
}
int ServerTagToWire(unsigned index)
{ return index < MAX_TAGS && (!index || tag_slots[index]) ? tag_slots[index] : -1; }
int ServerMaterialToWire(unsigned index)
{ return index<MAX_MATERIALS && (!index || ol_material_slots[index]) ? ol_material_slots[index] : -1; }
int ServerLocalizedToWire(unsigned index)
{ return index<MAX_LOCALIZEDSTRINGS && (!index || localized_slots[index]) ? localized_slots[index] : -1; }
int ServerEffectToWire(unsigned index)
{ return index<EFFECT_SLOTS && (!index || effect_slots[index]) ? effect_slots[index] : -1; }
int ServerByteEffectToWire(unsigned index)
{ return index<EFFECT_SLOTS && (!index || byte_effect_slots[index]>0) ? byte_effect_slots[index] : -1; }
int ServerTaggedEffectToWire(unsigned index)
{ return index<256 && (!index || tagged_effect_slots[index]) ? tagged_effect_slots[index] : -1; }
/* Runs before every snapshot for every effect without a byte slot: on a full bank (zm) that was the whole bank's effects
 * looked up in the mounted zones each time, 38 ms a frame with the zones shared with C1 (12 million lookups in 20 s). */
static int inherited_byte_effect_absent(unsigned slot,const char *name)
{
    if(inherited_byte_effects[slot].name!=name) {
        const effect_source_t *source;
        inherited_byte_effects[slot].absent=LoadMountedEffectSource(name,&source)!=1;
        inherited_byte_effects[slot].name=name;
    }
    return inherited_byte_effects[slot].absent;
}
/* The byte bank (ET_FX / ET_LOOP_FX selectors, 1..255) shares its slots with
 * the zone type's inherited constant effects; on zm those fill 249 of them,
 * so all but six script effects (trap zappers and lights, the monkey glow,
 * the black-hole portal, ...) found no slot and every spawnFx of them was
 * dropped from snapshots ("entity type 8 cannot be seeded"). An inherited
 * slot whose effect is absent from the zones this match mounted is one C1
 * can never draw (it resolves it to the missing-effect default), so such a
 * slot may carry a script effect instead -- unless a script override or any
 * effect mapping of this server already uses it. Inherited effects that
 * exist are never overwritten. */
static int reclaim_byte_effect_slot(configstrings_t *bank,const configstrings_t *inherited,const char *name)
{
    unsigned slot,index;
    for(slot=1;slot<BYTE_EFFECT_SLOTS;++slot) {
        const char *base=GetConfigstring(inherited,CS_EFFECTS+slot);
        const char *changed=bank->values[CS_EFFECTS+slot];
        int used=0;
        if(reserved_byte_effects[slot]) continue;
        if(changed && *changed) continue;
        if(!base || !*base || !inherited_byte_effect_absent(slot,base)) continue;
        for(index=1;index<EFFECT_SLOTS && !used;++index)
            used=effect_slots[index]==(int)slot || byte_effect_slots[index]==(int)slot;
        if(used) continue;
        if(!OL_SetConfigstring(bank,CS_EFFECTS+slot,name)) return -1;
        Com_DPrintf(CON_CHANNEL_SERVER,"CoDOL effects: byte slot %u reclaimed from absent '%s' for '%s'\n",slot,base,name);
        return (int)slot;
    }
    return -1;
}
int ReserveServerEntityEffect(unsigned effect)
{
    unsigned char pinned[BYTE_EFFECT_SLOTS]; unsigned i;
    int wide,slot;
    /* SS_LOADING is the full map setup interval, before reconnect/gamestate.
     * level.initializing also enforces the GSC precache contract. */
    if(!level.initializing || sv.state!=SS_LOADING || !effect || effect>=EFFECT_SLOTS) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL entity FX reserve: rejected native %u: initializing=%d state=%d\n",
            effect,level.initializing,sv.state);
        return -1;
    }
    if(svs.clients && sv_maxclients) for(i=0;i<(unsigned)sv_maxclients->integer;++i)
        if(svs.clients[i].gamestateSent) {
            Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL entity FX reserve: rejected native %u: client %u has sent gamestate\n",effect,i);
            return -1;
        }
    /* SV_SetConfigstring stores precached native names during SS_LOADING but
     * returns before PublishAssetConfigstring. Bind that pending range
     * here rather than assuming a snapshot refresh has already run. */
    if(!RefreshInheritedRange(2)) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL entity FX reserve: native %u effect-name publication failed\n",effect);
        return -1;
    }
    wide=effect_slots[effect];
    if(wide<=0) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL entity FX reserve: native %u has no precached effect binding\n",effect);
        return -1;
    }
    slot=byte_effect_slots[effect];
    if(slot>0 && slot<BYTE_EFFECT_SLOTS) {
        reserved_byte_effects[slot]=1;
        return slot;
    }
    memcpy(pinned,reserved_byte_effects,sizeof(pinned));
    for(i=1;i<EFFECT_SLOTS;++i)
        if(byte_effect_slots[i]>0 && byte_effect_slots[i]<BYTE_EFFECT_SLOTS)
            pinned[byte_effect_slots[i]]=1;
    slot=ReserveByteEffectSlot(ServerConfigstrings(),ServerInheritedConfigstrings(),
        (unsigned)wide,pinned);
    if(slot<=0) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL entity FX reserve: native %u wire %d has no unpinned byte selector or transaction failed\n",effect,wide);
        return -1;
    }
    /* The transaction has committed; all binding changes below are infallible.
     * Existing tagged and thermal configstrings were rewritten in place. */
    if(slot!=wide) for(i=1;i<EFFECT_SLOTS;++i) {
        if(effect_slots[i]==wide) {
            effect_slots[i]=slot; byte_effect_slots[i]=slot;
        } else if(effect_slots[i]==slot) {
            effect_slots[i]=wide; byte_effect_slots[i]=-1;
        }
    }
    byte_effect_slots[effect]=slot;
    reserved_byte_effects[slot]=1;
    return slot;
}
static void RefreshByteEffects(void)
{
    configstrings_t *bank=ServerConfigstrings();
    const configstrings_t *inherited=ServerInheritedConfigstrings();
    unsigned index;
    /* Run during name publication, before snapshot construction. Never allocate
     * from a per-recipient serializer or overwrite an inherited asset. The
     * wider mapping remains valid even when the low bank is exhausted. */
    for(index=1;index<EFFECT_SLOTS;++index) {
        int wide=effect_slots[index],slot=byte_effect_slots[index],recipient;
        const char *name,*old;
        if(wide<=0 || !inherited) { byte_effect_slots[index]=0; continue; }
        name=GetConfigstring(bank,CS_EFFECTS+(unsigned)wide);
        if(!name) name=GetConfigstring(inherited,CS_EFFECTS+(unsigned)wide);
        if(!name || !*name) { byte_effect_slots[index]=0; continue; }
        if(slot>0) {
            old=GetConfigstring(bank,CS_EFFECTS+(unsigned)slot);
            if(!old) old=GetConfigstring(inherited,CS_EFFECTS+(unsigned)slot);
            if(old && !strcmp(old,name)) continue;
        }
        slot=RegisterByteEffectName(bank,inherited,name);
        if(slot<=0) slot=reclaim_byte_effect_slot(bank,inherited,name);
        byte_effect_slots[index]=slot;
        if(slot<=0 || !bank->values[CS_EFFECTS+(unsigned)slot]) continue;
        if(!svs.clients || !sv_maxclients) continue;
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",CS_EFFECTS+(unsigned)slot,name);
        }
    }
}
/* Native overflow effect slots, separate from legacy configstring addresses.
 * Called by the full-width effect producer once its consumers are connected.
 * Return0 for a missing late lookup, -1 for invalid/exhausted registration. */
int RegisterExtraServerEffect(const char *name,int create)
{
    char canonical[64];
    configstrings_t *bank=ServerConfigstrings();
    const configstrings_t *inherited=ServerInheritedConfigstrings();
    unsigned index,free_slot=0;
    int slot,recipient;
    if(!name || !*name || strlen(name)>=64 || strchr(name,'\n') ||
       strchr(name,'\r')) return -1;
    /* Retained G_FindConfigstringIndex uses lowercase string identities for
     * effect names (base63e >334). Match that identity in the extra pool. */
    for(index=0;name[index];++index)
        canonical[index]=name[index]>='A' && name[index]<='Z' ? name[index]+32 : name[index];
    canonical[index]=0;
    name=canonical;
    for(index=100;index<EFFECT_SLOTS;++index) {
        const char *current=extra_effect_names[index-100];
        if(*current && !strcmp(current,name)) return (int)index;
        if(!*current && !free_slot) free_slot=index;
    }
    if(!create) return 0;
    if(!free_slot || !bank || !inherited) return -1;
    slot=RegisterEffectName(bank,inherited,name);
    if(slot<=0) return -1;
    strcpy(extra_effect_names[free_slot-100],name);
    effect_slots[free_slot]=slot;
    if(bank->values[CS_EFFECTS+(unsigned)slot] && svs.clients && sv_maxclients)
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",CS_EFFECTS+(unsigned)slot,name);
        }
    RefreshByteEffects();
    return (int)free_slot;
}
/* Preserve retained low-slot/constant-configstring allocation, then spill into
 * map-owned slots. The last retained helper argument is a diagnostic label,
 * not an overflow boolean. */
int G_FindConfigstringIndex(const char *,int,int,int,const char *);
int RegisterNativeTaggedEffect(unsigned effect,const char *tag)
{
    /* Mark the wider native representation explicitly: legacy two-digit
     * compounds may contain tags beginning with digits and cannot be guessed.
     * Only the translated wire bank contains C1's unmarked three digits. */
    if(!effect || effect>=EFFECT_SLOTS || !tag || !*tag ||
       strlen(tag)>MAX_STRING_CHARS-40 || strchr(tag,'\n') || strchr(tag,'\r')) return 0;
    return G_FindConfigstringIndex(va("@%03u%s",effect,tag),0x6a2,256,1,NULL);
}
int RegisterMaskedTaggedEffect(unsigned effect,const char *tag,uint32_t audience)
{
    unsigned i,freeSlot=0;
    int tagged=RegisterNativeTaggedEffect(effect,tag);
    if(tagged<=0) return 0;
    /* Match IW4's bounded compound bank, with the audience kept native.
     * Never recycle an identity: old snapshot queues still name it. */
    for(i=1;i<256;++i) {
        if(masked_tagged_effects[i].tagged_slot==(unsigned)tagged &&
           masked_tagged_effects[i].audience==audience) return (int)i;
        if(!masked_tagged_effects[i].tagged_slot && !freeSlot) freeSlot=i;
    }
    if(!freeSlot) return 0;
    masked_tagged_effects[freeSlot].tagged_slot=(unsigned)tagged;
    masked_tagged_effects[freeSlot].audience=audience;
    return (int)freeSlot;
}
int ReadMaskedTaggedEffect(unsigned index,unsigned *taggedSlot,uint32_t *audience)
{
    if(!index || index>=256 || !taggedSlot || !audience ||
       !masked_tagged_effects[index].tagged_slot) return 0;
    *taggedSlot=masked_tagged_effects[index].tagged_slot;
    *audience=masked_tagged_effects[index].audience;
    return 1;
}
int NativeEffectIndex(const char *name)
{
    unsigned index;
    int found,create=level.initializing;
    if(!name || !*name) return 0;
    found=G_FindConfigstringIndex(name,0x63e,100,0,NULL);
    if(found>0) return found;
    found=RegisterExtraServerEffect(name,0);
    if(found>0) return found;
    if(found<0) { Scr_Error("invalid effect name"); return 0; }
    if(!create) { Scr_Error(va("effect '%s' not precached",name)); return 0; }
    for(index=1;index<100;++index) {
        unsigned handle=SV_GetConfigstringIndex(0x63e + index);
        if(!handle || handle==sv.emptyConfigString)
            return G_FindConfigstringIndex(name,0x63e,100,1,"effect");
    }
    found=RegisterExtraServerEffect(name,1);
    if(found<0) { Scr_Error("effect registry exhausted or unavailable"); return 0; }
    return found;
}
void GetNativeEffectName(unsigned index,char *out,int capacity)
{
    const char *name="";
    if(!out || capacity<=0) return;
    if(index>0 && index<100) {
        SV_GetConfigstring(0x63e + index,out,capacity);
        return;
    }
    if(index>=100 && index<EFFECT_SLOTS) name=extra_effect_names[index-100];
    Q_strncpyz(out,name,capacity);
}
static int RefreshInheritedRange(int localized)
{
    unsigned first=localized==2 ? 0x63e : localized ? CS_LOCALIZEDSTRINGS : CS_MATERIALS;
    unsigned count=localized==2 ? 100 : localized ? MAX_LOCALIZEDSTRINGS : MAX_MATERIALS;
    unsigned base=localized==2 ? CS_EFFECTS : localized ? OL_CS_LOCALIZEDSTRINGS : OL_CS_MATERIALS;
    unsigned *handles=localized==2 ? effect_strings : localized ? localized_strings : material_strings;
    int *slots=localized==2 ? effect_slots : localized ? localized_slots : ol_material_slots;
    configstrings_t *bank=ServerConfigstrings();
    const configstrings_t *inherited=ServerInheritedConfigstrings(); unsigned index;
    for(index=1;index<count;++index) {
        unsigned handle=SV_GetConfigstringIndex(first+index);
        const char *name,*old; int slot,recipient;
        if(!handle || handle==sv.emptyConfigString) { handles[index]=handle; slots[index]=0; continue; }
        name=SL_ConvertToString(handle);
        if(!inherited || !name || !*name || strlen(name)>MAX_STRING_CHARS-32 || strchr(name,'\n') || strchr(name,'\r')) return 0;
        if(handle==handles[index] && slots[index]>0) {
            old=bank->values[base+slots[index]];
            if(!old) old=inherited->values[base+slots[index]];
            if(old && !strcmp(old,name)) continue;
        }
        slot=localized==2 ? RegisterEffectName(bank,inherited,name) : localized ? RegisterLocalizedString(bank,inherited,name) : RegisterHudMaterial(bank,inherited,name);
        if(slot<1) return 0;
        handles[index]=handle; slots[index]=slot;
        /* Pure inherited matches need no command. An explicit override may
         * have been allocated before this binding, so publish it here too. */
        if(!bank->values[base+(unsigned)slot]) continue;
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",base+(unsigned)slot,name);
        }
    }
    if(localized==2) RefreshByteEffects();
    return 1;
}
static int RefreshTaggedEffects(void)
{
    configstrings_t *bank=ServerConfigstrings();
    const configstrings_t *inherited=ServerInheritedConfigstrings();
    unsigned index;
    if(!RefreshInheritedRange(2)) return 0;
    for(index=1;index<256;++index) {
        unsigned handle=SV_GetConfigstringIndex(0x6a2+index);
        const char *native,*value; int effect,slot,previous,recipient;
        unsigned prefix=0,digits=2,native_effect=0,j;
        if(!handle || handle==sv.emptyConfigString) { tagged_effect_slots[index]=0; continue; }
        native=SL_ConvertToString(handle);
        if(!native) return 0;
        if(native[0]=='@') { prefix=1; digits=3; }
        if(strlen(native)<=prefix+digits || strlen(native)>MAX_STRING_CHARS-33) return 0;
        for(j=prefix;j<prefix+digits;++j) {
            if(native[j]<'0' || native[j]>'9') return 0;
            native_effect=native_effect*10+(unsigned)(native[j]-'0');
        }
        effect=ServerEffectToWire(native_effect);
        if(effect<=0) return 0;
        slot=RegisterTaggedEffect(bank,inherited,(unsigned)effect,native+prefix+digits);
        if(slot<1) return 0;
        previous=tagged_effect_slots[index];
        tagged_effect_slots[index]=slot;
        value=GetConfigstring(bank,CS_TAGGED_EFFECTS+(unsigned)slot);
        if(previous==slot || !value) continue;
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",CS_TAGGED_EFFECTS+(unsigned)slot,value);
        }
    }
    return 1;
}
#include "hud_fontscale.h"
static int ResolveServerHud(void *context,const hudelem_t *source,hud_bindings_t *out)
{
    int label,text,material; unsigned flags; (void)context;
    if(source->soundID || !TranslateEngineHudFlags((unsigned)source->flags,&flags)) return 0;
    label=ServerLocalizedToWire((unsigned)source->label);
    text=ServerLocalizedToWire((unsigned)source->text);
    material=ServerMaterialToWire((unsigned)source->materialIndex);
    if(label<0 || text<0 || material<0) return 0;
    hud_fontscale_t scale;
    if(!GetHudFontScale(source,&scale)) return 0;
    *out=(hud_bindings_t){(unsigned)label,(unsigned)text,(unsigned)material,0,flags,
        scale.from,scale.start,scale.duration}; return 1;
}
int CollectServerHud(hud_state_t *out,const hudElemState_t *source)
{
    if(!out || !source || !RefreshInheritedRange(0) || !RefreshInheritedRange(1)) return 0;
    return CollectEngineHud(out,source,ResolveServerHud,NULL);
}
static int RefreshAssetRange(unsigned first, unsigned count, unsigned *handles,
                                 int *slots, int model)
{
    unsigned index;
    configstrings_t *bank = ServerConfigstrings();
    const configstrings_t *inherited = ServerInheritedConfigstrings();
    for (index = 1; index < count; ++index) {
        unsigned handle = SV_GetConfigstringIndex(first + index);
        const char *name, *p;
        unsigned config_index;
        int slot, recipient;
        if (!handle || handle == sv.emptyConfigString) {
            handles[index] = handle; slots[index] = 0; continue;
        }
        name = SL_ConvertToString(handle);
        if (!name || !*name) return 0;
        if (handle == handles[index] && slots[index] > 0) {
            const char *registered = GetConfigstring(bank,
                (model ? OL_CS_MODELS : OL_CS_TAGS) + slots[index]);
            if(!registered) registered = GetConfigstring(inherited,
                (model ? OL_CS_MODELS : OL_CS_TAGS) + slots[index]);
            if (registered && !strcmp(registered, name)) continue;
        }
        /* Asset names are reliable-command tokens. Never silently alter a name
         * to make it parseable: that would identify a different client asset. */
        for (p = name; *p; ++p)
            if ((unsigned char)*p <= 32 || *p == 127 || *p == '"' || *p == ';' || *p == '\\') return 0;
        slot = model ? RegisterInheritedModelName(bank, inherited, name) :
                       RegisterInheritedTagName(bank, inherited, name);
        if (slot < 1) return 0;
        handles[index] = handle; slots[index] = slot;
        config_index = (model ? OL_CS_MODELS : OL_CS_TAGS) + slot;
        for (recipient = 0; recipient < sv_maxclients->integer; ++recipient) {
            client_t *target = &svs.clients[recipient];
            if (target->state >= CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target, "d %u %s", config_index, name);
        }
    }
    return 1;
}
static int RefreshIndexedNames(unsigned native_base,unsigned wire_base,unsigned count,unsigned first)
{
    unsigned index; configstrings_t *bank=ServerConfigstrings();
    for(index=first;index<count;++index) {
        unsigned handle=SV_GetConfigstringIndex(native_base+index);
        const char *name=(handle && handle!=sv.emptyConfigString) ? SL_ConvertToString(handle) : "";
        const char *old=GetConfigstring(bank,wire_base+index),*p;
        int recipient;
        if(!name) return 0;
        if((!old && !*name) || (old && !strcmp(old,name))) continue;
        for(p=name;*p;++p)
            if((unsigned char)*p<=32 || *p==127 || *p=='"' || *p==';' || *p=='\\') return 0;
        if(!OL_SetConfigstring(bank,wire_base+index,name)) return 0;
        /* These namespaces preserve native indices, including explicit clears. */
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",wire_base+index,name);
        }
    }
    return 1;
}
static int RefreshSoundNames(void)
{
    /* C1 LocalSound 468765 reads CS[3395+index]. */
    return RefreshIndexedNames(CS_SOUNDS,OL_CS_SOUNDS,MAX_SOUNDS,1);
}
static int RefreshShellshockNames(void)
{
    /* Retained GScr_ShellShock searches 0x7a2+1..15; C1 4674ff..46752a
     * loads shock/<name>.shock for CS4227+index into settings[index]. */
    return RefreshIndexedNames(0x7a2,CS_SHELLSHOCKS,SHELLSHOCK_SLOTS,1);
}
int RefreshServerDvars(void)
{
    const configstrings_t *inherited=ServerInheritedConfigstrings();
    configstrings_t *bank=ServerConfigstrings(),*staged;
    char *pending[MAX_REPLICATED_DVARS]={0};
    size_t old_pool_bytes[MAX_REPLICATED_DVARS]={0};
    unsigned i,pass,changed=0;
    if(!inherited) return 1; /* No map constants installed yet. */
    /* Borrow unchanged pointers only for the final-pool view. Never clear this
     * shallow copy: pending[] alone owns the new allocations until commit. */
    staged=malloc(sizeof(*staged));
    if(!staged) return 0;
    memcpy(staged,bank,sizeof(*staged));
    for(i=0;i<MAX_REPLICATED_DVARS;++i) {
        unsigned index=CS_DVAR_NAMES+i,value_index=CS_DVAR_VALUES+i;
        const char *name=GetConfigstring(bank,index),*old,*value;
        cvar_t *var; size_t length;
        if(!name) name=GetConfigstring(inherited,index);
        if(!name || !*name) break; /* C1 replicated-dvar applier 0x466020. */
        var=Cvar_FindVar(name);
        if(!var || !(var->flags&CVAR_SYSTEMINFO)) continue;
        value=Cvar_DisplayableValue(var);
        if(!value || strlen(value)>CONFIGSTRING_VALUE_CHARS ||
           strchr(value,'\n') || strchr(value,'\r')) goto fail;
        old=GetConfigstring(bank,value_index);
        if(!old) old=GetConfigstring(inherited,value_index);
        if(old && !strcmp(old,value)) continue;
        length=strlen(value);
        pending[i]=malloc(length+1);
        if(!pending[i]) goto fail;
        memcpy(pending[i],value,length+1);
        staged->values[value_index]=pending[i];
        old_pool_bytes[i]=(old && *old) ? strlen(old)+1 : 0;
        ++changed;
    }
    if(!changed) { free(staged); return 1; }
    if(!ConfigstringPoolSize(staged,inherited,NULL)) goto fail;
    /* C1 rebuilds the pool after every d command. Decreases first make every
     * intermediate pool no larger than max(old pool, validated final pool).
     * These are value-only updates: the inherited name ordering is unchanged. */
    for(pass=0;pass<2;++pass) {
        for(i=0;i<MAX_REPLICATED_DVARS;++i) {
            unsigned value_index=CS_DVAR_VALUES+i;
            size_t length,new_pool_bytes;
            int recipient;
            if(!pending[i]) continue;
            length=strlen(pending[i]);
            new_pool_bytes=length ? length+1 : 0;
            if((new_pool_bytes>old_pool_bytes[i]) != (pass==1)) continue;
            if(bank->values[value_index])
                bank->bytes-=strlen(bank->values[value_index])+1;
            else ++bank->count;
            free(bank->values[value_index]);
            bank->values[value_index]=pending[i];
            bank->bytes+=length+1;
            pending[i]=NULL; /* ownership transferred; no fallible allocation */
            for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
                client_t *target=&svs.clients[recipient];
                if(target->state>=CS_PRIMED && target->gamestateSent)
                    SV_SendServerCommandNoLoss(target,"d %u %s",value_index,
                        bank->values[value_index]);
            }
        }
    }
    free(staged);
    return 1;
fail:
    for(i=0;i<MAX_REPLICATED_DVARS;++i) free(pending[i]);
    free(staged);
    return 0;
}
/* Registration-time C1 tag slot allocation. The wire slot for a native tag
 * was only allocated from the gamestate/snapshot path, so a link captured in
 * G_RunFrame before the first snapshot of a map (the three SV_SpawnServer
 * warm-up frames and every frame until a client is primed) had a native
 * index and no slot, and was dropped for that frame. IW4 has no such gap:
 * G_TagIndex publishes CS_TAGS+i at once and that index is the wire index.
 * Idempotent and cheap (32 entries); safe from G_RunFrame. */
int RefreshServerTagNames(void)
{
    if(!sv_maxclients) return 0;
    return RefreshAssetRange(CS_TAGS, MAX_TAGS, tag_strings, tag_slots, 0);
}
int RefreshServerAssetNames(void)
{
    return RefreshAssetRange(CS_MODELS, MAX_MODELS, model_strings, model_slots, 1) &&
           RefreshAssetRange(CS_TAGS, MAX_TAGS, tag_strings, tag_slots, 0) &&
           RefreshSoundNames() && RefreshInheritedRange(0) && RefreshInheritedRange(1) &&
           RefreshIndexedNames(CS_STATUSICONS,CS_STATUS_ICONS,MAX_STATUSICONS,0) &&
           RefreshIndexedNames(CS_HEADICONS,CS_HEAD_ICONS,MAX_HEADICONS,0) &&
           RefreshIndexedNames(827,1312,3,0) &&
           RefreshTaggedEffects() && RefreshShellshockNames() && RefreshServerDvars();
}

int OverlayServerClient(client_state_t *wire, const clientState_t *engine)
{
    client_assets_t assets = {0};
    int model, tag;
    unsigned i;
    if (!wire || !engine || (model = ServerModelToWire(engine->modelindex)) < 0) return 0;
    assets.model = model;
    for (i = 0; i < 6; ++i) {
        model = ServerModelToWire(engine->attachModelIndex[i]);
        tag = ServerTagToWire(engine->attachTagIndex[i]);
        if (model < 0 || tag < 0) return 0;
        assets.attach_model[i] = model; assets.attach_tag[i] = tag;
    }
    /* C1 G_UpdateClientState 0x50d0f0 fills the animation-condition words
     * from the client's playerState. The archived clientState carries none of
     * them, so they come from the live client like rank does. */
    if(engine->clientIndex>=0 && engine->clientIndex<level.maxclients &&
       g_entities[engine->clientIndex].r.inuse && g_entities[engine->clientIndex].client) {
        const gentity_t *player=&g_entities[engine->clientIndex];
        const playerState_t *ps=&player->client->ps;
        assets.dual_wield=WeaponStateIsDualWielding(ps) ? 1u : 0u;
        assets.riot_shield_next=(ps->weapFlags & WEAP_RIOTSHIELD_NEXT) ? 1u : 0u;
        /* 0x50d16a..0x50d18e; ps+0x3334 has no writer in C1, so the value
         * is 1 while the 0x400000 window is open. */
        assets.dive_state=((ps->pm_flags & 0x400000u) && !(ps->pm_flags & 0x800u) && ps->pm_time==0) ? 1u : 0u;
        /* 0x12c/0x130: the right/left world-gun children of this player.
         * C1's world-weapon producer 0x570b70 stores their entity numbers at
         * gclient+0x3594/+0x3598, i.e. clientState+0x12c/+0x130, and its CG
         * plays the third-person muzzle flash on that entity's DObj
         * (0x480180..0x4802c1, when its +68 weapon matches) and takes the
         * bullet's muzzle point from its tag_flash (0x482606). With 0 the
         * client falls back to the player DObj, which holds no gun: no
         * flash, and "No tag_flash or DObj in CalcMuzzlePoint". This word
         * was misread as a rider's vehicle; C1 writes nothing else here. */
        assets.world_weapon_entity[0]=PlayerWorldWeaponEntity((unsigned)engine->clientIndex,0);
        assets.world_weapon_entity[1]=PlayerWorldWeaponEntity((unsigned)engine->clientIndex,1);
        /* Base conversion seeds0. The captured session extension overlays
         * CS+134 afterwards (C1 setter51dbb0 -> client359c -> ci+b4c).
         * Never fetch a live slot here for an archived/theatre state. */
        assets.colour_index=0;
        /* 0x010: the action slot [{ACTION_SLOT_HELP}] names, set by script
         * (setActionSlotHelp) before a killstreak splash; wire is slot - 1. */
        assets.action_slot_help=ClientActionSlotHelp((unsigned)engine->clientIndex);
        if(assets.action_slot_help) --assets.action_slot_help;
    }
    return OverlayEngineClient(wire, engine, &assets);
}
int OverlayServerModelEntity(entity_state_t *wire,const gentity_t *engine)
{
    int model;
    if(!wire || !engine || engine->r.bmodel ||
       (model=ServerModelToWire(engine->s.index))<0) return 0;
    return OverlayEngineModelEntity(wire,&engine->s,(unsigned)model,0);
}
static int RefreshHintStrings(void)
{
    unsigned slot;
    configstrings_t *bank=ServerConfigstrings();
    /* Native G_GetHintStringIndex starts at0x115, with no reserved zero slot.
     * C1 456270 reads CS[584+ps.cursorHintString] as localized message text. */
    for(slot=0;slot<HINTSTRING_RETAINED;++slot) {
        unsigned handle=SV_GetConfigstringIndex(0x115+slot);
        unsigned wire_index=CS_HINTSTRINGS+slot;
        const char *value=(handle && handle!=sv.emptyConfigString) ? SL_ConvertToString(handle) : "";
        const char *old=GetConfigstring(bank,wire_index);
        int recipient;
        if(!value || strlen(value)>MAX_STRING_CHARS-32 || strchr(value,'\n') || strchr(value,'\r')) return 0;
        if((!old && !*value) || (old && !strcmp(old,value))) continue;
        if(!OL_SetConfigstring(bank,wire_index,value)) return 0;
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",wire_index,value);
        }
    }
    return 1;
}
/* G_GetHintStringIndex(int *index, const char *text) for all 96 C1 slots.
 *
 * The retained allocator (and its inline copies in GScr_SetHintString and
 * trigger_use_shared) scanned the 32 configstrings at 0x115 and failed at 32
 * ("Too many different hintstring values"): the block cannot grow because
 * 0x115+32 is CS_LOCALIZEDSTRINGS. C1 allows 96 (0x53c4d0 cmp esi,0x60).
 * Slots 0..31 keep the retained storage so RefreshHintStrings still
 * publishes them; 32..95 are kept here and published straight to
 * CS 584+slot. Both halves allocate in order and never free within a map,
 * like the original. Returns 1 with *index set, or 0 with *index = -1. */
int OL_G_GetHintStringIndex(int *index, const char *text)
{
    char current[MAX_STRING_CHARS];
    unsigned slot;
    configstrings_t *bank;
    int recipient;
    if(!index) return 0;
    *index=-1;
    if(!text) return 0;
    for(slot=0;slot<HINTSTRING_RETAINED;++slot) {
        SV_GetConfigstring(0x115+slot,current,sizeof(current));
        if(!current[0]) {
            SV_SetConfigstring(0x115+slot,text);
            *index=(int)slot;
            return 1;
        }
        if(!strcmp(current,text)) { *index=(int)slot; return 1; }
    }
    for(slot=HINTSTRING_RETAINED;slot<HINTSTRING_SLOTS;++slot) {
        char *stored=high_hints[slot-HINTSTRING_RETAINED];
        if(stored[0]) {
            if(!strcmp(stored,text)) { *index=(int)slot; return 1; }
            continue;
        }
        bank=ServerConfigstrings();
        if(!bank || strlen(text)>MAX_STRING_CHARS-32 || strchr(text,'\n') || strchr(text,'\r') ||
           !OL_SetConfigstring(bank,CS_HINTSTRINGS+slot,text)) {
            Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL hint strings: cannot publish slot %u\n",slot);
            return 0;
        }
        Q_strncpyz(stored,text,MAX_STRING_CHARS);
        if(svs.clients && sv_maxclients)
            for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
                client_t *target=&svs.clients[recipient];
                if(target->state>=CS_PRIMED && target->gamestateSent)
                    SV_SendServerCommandNoLoss(target,"d %u %s",CS_HINTSTRINGS+slot,text);
            }
        *index=(int)slot;
        return 1;
    }
    return 0;
}
/* C1 keeps the two team scores in cg+0xBEB20 and cg+0xBEB24. Two routes reach
 * them: the reliable commands C and D (4699d5 / 469a12), which the retained
 * setteamscore and updatescores builtins now send, and configstrings 5 and 6,
 * which the client also reads for its win/lose banner (550960) and which a
 * late joiner needs in its gamestate. The retained engine never writes those
 * two configstrings - CS_SCORES1/2 are inside a commented-out block in
 * g_shared.h - so publish them here whenever level.teamScores changes.
 * The bank itself is the previous-value record, so a new map republishes. */
void PublishTeamScores(void)
{
    static const unsigned index[2] = { CS_TEAM_SCORE1, CS_TEAM_SCORE2 };
    int score[2];
    unsigned i;
    configstrings_t *bank;
    if (!level.clients || !svs.clients || !sv_maxclients) return;
    score[0] = level.teamScores[TEAM_RED];
    score[1] = level.teamScores[TEAM_BLUE];
    bank = ServerConfigstrings();
    for (i = 0; i < 2; ++i) {
        char value[16];
        const char *old;
        int recipient, length;
        length = snprintf(value, sizeof(value), "%i", score[i]);
        if (length < 0 || (size_t)length >= sizeof(value)) continue;
        old = GetConfigstring(bank, index[i]);
        if (old && !strcmp(old, value)) continue;
        if (!OL_SetConfigstring(bank, index[i], value)) return;
        for (recipient = 0; recipient < sv_maxclients->integer; ++recipient) {
            client_t *target = &svs.clients[recipient];
            if (target->state >= CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target, "d %u %s", index[i], value);
        }
    }
}
/* Tell the client to rebuild its weapon HUD when the weapon or the offhand
 * changes. C1's F command (469a8f) reads one element index, bounds it with
 * "cmp eax,7 / ja" and dispatches through the eight-entry table at 0x4682f4
 * inside 0x468140 (CG_MenuShowNotify). Each arm stamps a cg fade time and sets
 * the visible bit (menu+0x48 |= 4) of legacy HUD menus by name, measured:
 *
 *   0 0x468167  "Health"                               cg+0xBFA2C
 *   1 0x46822b  "weaponinfo", "weaponinfo_lowdef"      cg+0xBFA30
 *               then "offhandinfo" (0x468281)          cg+0xBFA3C
 *   2 0x468198  "Compass"                              cg+0xBFA28
 *   3 0x4681c9  "stance"                               cg+0xBFA34
 *   4 0x46822b  same arm as 1: weapon + offhand info
 *   5 0x46829c  "objectiveinfo"                        cg+0xBF038
 *   6 0x4681fa  "sprintMeter"                          cg+0xBFA38
 *   7 0x4682b7  "RCBombHints"                          cg+0xBFA44
 *
 * There is no offhand-only arm: the offhand panel is the second half of the
 * weapon arm, exactly as CG_SetEquippedOffHand (486dd0) shows it. Index 5 is
 * the scoreboard's objective/score panel - cg+0xBF038 is the score fade time
 * next to showScores (cg+0xBF034), the one CG_ScoresUp (436e70) stamps and
 * 43b260 tests - so the "F 5" this function used to send whenever the
 * offhand index changed, which is the start of a lethal, tactical or
 * equipment throw, put the TAB panel up (reported as the TAB menu opening
 * on the first throw after a pause; throws in quick succession found it
 * still up). Bots never saw it: they have no gamestate, so nothing here
 * reaches them.
 *
 * A retail server sends it after a weapon change; this server never did, which
 * leaves the ammo and offhand panels showing whatever they were built with.
 * Driving it from here rather than from the retained weapon assembly catches
 * every path that can change the weapon - give, switch, pickup, class change,
 * spawn - with no extra send site. One notify covers both panels, so a frame
 * that changes the weapon and the offhand together sends it once. */
#define HUD_NOTIFY_WEAPON_INFO 4
void RefreshWeaponHud(void)
{
    static int last_weapon[MAX_CLIENTS];
    static int last_offhand[MAX_CLIENTS];
    static unsigned char primed[MAX_CLIENTS];
    int i, limit;
    if (!level.clients || !svs.clients || !sv_maxclients) return;
    limit = sv_maxclients->integer;
    if (limit > level.maxclients) limit = level.maxclients;
    if (limit > MAX_CLIENTS) limit = MAX_CLIENTS;
    for (i = 0; i < limit; ++i) {
        client_t *client = &svs.clients[i];
        const playerState_t *ps;
        if (client->state != CS_ACTIVE || !client->gamestateSent) { primed[i] = 0; continue; }
        ps = &level.clients[i].ps;
        if (!primed[i]) {
            /* The first frame after a client goes active is its own refresh. */
            primed[i] = 1;
            last_weapon[i] = ps->weapon;
            last_offhand[i] = ps->offHandIndex;
            continue;
        }
        if (ps->weapon == last_weapon[i] && ps->offHandIndex == last_offhand[i]) continue;
        last_weapon[i] = ps->weapon;
        last_offhand[i] = ps->offHandIndex;
        SV_SendServerCommandNoLoss(client, "F %i", HUD_NOTIFY_WEAPON_INFO);
    }
}
int PublishAssetConfigstring(unsigned index)
{
    /* Retained SaveRegisteredWeapons writes its space-delimited native list
     * to CS 0x8d2.  A CoDOL client has no such list; it registers definitions
     * from CS_WEAPONS (5362+) and CS_WEAPON_COUNT (6517).  Refresh that real
     * registry here and consume the native update.  Publishing the retained
     * list both targets an unrelated C1 slot and exceeds C1's 1015-byte
     * single-configstring limit once the IW4 weapon set is registered. */
    if(index==0x8d2) {
        if(RefreshServerWeaponMappings()) return 1;
        /* The refresh fails once the retained table has outgrown the 128-entry
         * engine->wire map, which happens as soon as anything registers a
         * weapon after that map is full (append_weapon_set's native_count>128
         * guard in sv_weapons.c). Keeping the previous map leaves only
         * the unmapped definitions unreachable - WeaponToWire answers -1
         * for them and every consumer already handles that - whereas returning
         * -1 here ERR_DROPs the server and ends the match for everyone. Warn
         * once per level so the capacity problem stays visible. */
        if(!weapon_map_warned) {
            weapon_map_warned = 1;
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL weapon map refresh failed (%s); keeping the previous map. Definitions past the retained 128-slot table have no wire index\n",
                ServerWeaponRegistryError());
        }
        return 1;
    }
    /* Retained SaveRegisteredItems similarly writes its compact ownership
     * mask to 0x90a.  CoDOL receives owned/equipped weapons in playerState's
     * inventory section and does not consume this native configstring. */
    if(index==0x90a) return 1;
    /* These are retained server vote bookkeeping, not CoDOL wire slots.
     * C1's aligned dispatcher467350 sends13 to fog4667e0,15 to match
     * time466780 and16 to map center49e190. Raw fallback publication would
     * corrupt those fields after every vote. Keep native values for vote
     * logic; CoDOL voting presentation needs its own verified producer. */
    if(index>=CS_VOTE_TIME && index<=CS_VOTE_NO) return 1;
    /* Retained precache handlers allocate every slot, including offset zero.
     * C1's icon banks are separate from its general material namespace. */
    if(index>=CS_STATUSICONS && index<CS_STATUSICONS+MAX_STATUSICONS)
        return RefreshIndexedNames(CS_STATUSICONS,CS_STATUS_ICONS,MAX_STATUSICONS,0) ? 1 : -1;
    if(index>=CS_HEADICONS && index<CS_HEADICONS+MAX_HEADICONS)
        return RefreshIndexedNames(CS_HEADICONS,CS_HEAD_ICONS,MAX_HEADICONS,0) ? 1 : -1;
    /* C1 selector lookup541130: config bank27d3cba +2*1312. */
    if(index>=827 && index<830)
        return RefreshIndexedNames(827,1312,3,0) ? 1 : -1;
    if(index>=0x115 && index<0x115+HINTSTRING_RETAINED)
        return RefreshHintStrings() ? 1 : -1;
    if(index>=0x63e && index<0x63e + 100) return RefreshTaggedEffects() ? 1 : -1;
    if(index>=0x6a2 && index<0x7a2) return RefreshTaggedEffects() ? 1 : -1;
    if(index>=0x7a2 && index<0x7a2+SHELLSHOCK_SLOTS)
        return RefreshShellshockNames() ? 1 : -1;
    if(index>=CS_MATERIALS && index<CS_MATERIALS+MAX_MATERIALS) return RefreshInheritedRange(0) ? 1 : -1;
    if(index>=CS_LOCALIZEDSTRINGS && index<CS_LOCALIZEDSTRINGS+MAX_LOCALIZEDSTRINGS) return RefreshInheritedRange(1) ? 1 : -1;
    if(index==9 || index==11 || index==12 || index==821 || index==822 || index==823)
        return RefreshServerMapConfigstrings() ? 1 : -1;
    if(index>=CS_SOUNDS && index<CS_SOUNDS+MAX_SOUNDS)
        return RefreshSoundNames() ? 1 : -1;
    if(index>=CS_MODELS && index<CS_MODELS+MAX_MODELS)
        return RefreshAssetRange(CS_MODELS,MAX_MODELS,model_strings,model_slots,1) ? 1 : -1;
    if(index>=CS_TAGS && index<CS_TAGS+MAX_TAGS)
        return RefreshAssetRange(CS_TAGS,MAX_TAGS,tag_strings,tag_slots,0) ? 1 : -1;
    return 0;
}
/* Retained setexpfog produces start,density,r,g,b,milliseconds. C1's
 * non-sun fog producer545fd0 uses start,density,r,g,b,opacity,milliseconds,0
 * (format c57acc). Preserve the original tokens, adding full opacity and
 * disabling the optional sun extension. G_InitGame's single "0" clears fog.
 * This translates the existing six-argument method, not C1's richer API. */
static int TranslateRetainedFog(const char *value,char output[512])
{
    char part[18][64];
    const char *cursor,*start;
    int count=0,length;
    if(!value) return 0;
    if(!*value || !strcmp(value,"0")) {
        strcpy(output,value);
        return 1;
    }
    cursor=value;
    while(*cursor) {
        size_t bytes;
        while(isspace((unsigned char)*cursor)) ++cursor;
        if(!*cursor) break;
        start=cursor;
        while(*cursor && !isspace((unsigned char)*cursor)) ++cursor;
        bytes=(size_t)(cursor-start);
        if(count==18 || bytes>=sizeof(part[0])) return 0;
        memcpy(part[count],start,bytes); part[count++][bytes]=0;
    }
    /* The native CoDOL GSC producer already writes the full client format. */
    if((count==8 && !strcmp(part[7],"0")) ||
       (count==17 && !strcmp(part[7],"1"))) {
        if(strlen(value)>=512) return 0;
        strcpy(output,value);
        return 1;
    }
    if(count!=6) return 0;
    length=snprintf(output,512,"%s %s %s %s %s 1 %s 0",
        part[0],part[1],part[2],part[3],part[4],part[5]);
    return length>=0 && length<512;
}
int RefreshServerMapConfigstrings(void)
{
    static const unsigned mapping[][2]={{822,CS_NORTHYAW},{823,CS_MINIMAP},
        {11,CS_MATCH_END_TIME},{12,CS_MAP_CENTER},
        {821,CS_AMBIENT},{9,CS_FOG}};
    unsigned i; configstrings_t *bank=ServerConfigstrings();
    if(!RefreshHintStrings()) return 0;
    for(i=0;i<sizeof(mapping)/sizeof(mapping[0]);++i) {
        unsigned handle=SV_GetConfigstringIndex(mapping[i][0]), wire_index=mapping[i][1];
        const char *value=(handle && handle!=sv.emptyConfigString) ? SL_ConvertToString(handle) : "";
        const char *old=GetConfigstring(bank,wire_index);
        char fog_value[512];
        int recipient;
        /* SV_SetGameEndTime/MapCenter use IW3 11/12. C1 466780 reads
         * end time from 15; 467020 parses center from 16 into 6c450c8.
         * Unset initial values inherit the empty client defaults. */
        if(i>=2 && !old && value && !*value) continue;
        if(wire_index==CS_FOG) {
            if(!TranslateRetainedFog(value,fog_value)) return 0;
            value=fog_value;
        }
        /* Preserve each field's payload, including ambience's n/t info keys.
         * C1 ambientStop5427d0 uses1300; dispatcher4676b6 feeds44f2e0,
         * which reads name n and deadline t, just like the retained producer.
         * Bound the complete reliable command. */
        if(!value || strlen(value)>MAX_STRING_CHARS-32 || strchr(value,'\n') || strchr(value,'\r')) return 0;
        if(old && !strcmp(old,value)) continue;
        if(!OL_SetConfigstring(bank,wire_index,value)) return 0;
        for(recipient=0;recipient<sv_maxclients->integer;++recipient) {
            client_t *target=&svs.clients[recipient];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",wire_index,value);
        }
    }
    return 1;
}
static int collect_clients(int first, int count, client_state_t *out, size_t capacity,
    snapshot_client_resolver_t resolve,void *context)
{
    client_state_t clients[32] = {{0}};
    int i, ring_count = svsHeader.numSnapshotClients, next = svsHeader.nextSnapshotClients;
    if (count < 0 || count > 32 || (size_t)count > capacity || (count && !out) ||
        first < 0 || next < 0 || first > next || (int64_t)first + count > next) return 0;
    if (!count) return 1;
    if (!svsHeader.snapshotClients || ring_count <= 0 || ring_count > 0x20000 ||
        count > ring_count || first < (int64_t)next - ring_count) return 0;
    for (i = 0; i < count; ++i) {
        const clientState_t *source = &svsHeader.snapshotClients[((int64_t)first+i) % ring_count];
        if(resolve && !resolve(context,source,&clients[i])) return 0;
        if (!OverlayServerClient(&clients[i], source) ||
            !OverlayCapturedClientAbilities(first+i, &clients[i]) ||
            !OverlayClientCards(&clients[i]) ||
            (i && clients[i].number <= clients[i-1].number)) return 0;
    }
    if (count) memcpy(out, clients, (size_t)count*sizeof(*out));
    return 1;
}
int CollectServerClients(int first,int count,client_state_t *out,size_t capacity)
{ return collect_clients(first,count,out,capacity,NULL,NULL); }
int CollectServerClientsResolved(int first,int count,client_state_t *out,size_t capacity,
    snapshot_client_resolver_t resolve,void *context)
{
    if(count && !resolve) return 0;
    return collect_clients(first,count,out,capacity,resolve,context);
}
#endif

#ifdef OL_SERVER
#include "persistent_world.h"
static int resident_world_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}
int PersistentAssetBindingsEmpty(void){
    for(unsigned i=0;i<NATIVE_MODELS-MAX_MODELS;++i)
        if(high_model_names[i]||cached_models[MAX_MODELS+i])return 0;
    return resident_world_zero(model_strings,sizeof(model_strings)) &&
        resident_world_zero(tag_strings,sizeof(tag_strings)) &&
        resident_world_zero(model_slots,sizeof(model_slots)) &&
        resident_world_zero(tag_slots,sizeof(tag_slots)) &&
        resident_world_zero(material_strings,sizeof(material_strings)) &&
        resident_world_zero(localized_strings,sizeof(localized_strings)) &&
        resident_world_zero(ol_material_slots,sizeof(ol_material_slots)) &&
        resident_world_zero(localized_slots,sizeof(localized_slots)) &&
        resident_world_zero(effect_strings,sizeof(effect_strings)) &&
        resident_world_zero(extra_effect_names,sizeof(extra_effect_names)) &&
        resident_world_zero(effect_slots,sizeof(effect_slots)) &&
        resident_world_zero(byte_effect_slots,sizeof(byte_effect_slots)) &&
        resident_world_zero(tagged_effect_slots,sizeof(tagged_effect_slots)) &&
        resident_world_zero(masked_tagged_effects,sizeof(masked_tagged_effects)) &&
        resident_world_zero(high_hints,sizeof(high_hints));}

#endif
