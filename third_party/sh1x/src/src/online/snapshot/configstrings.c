/* Derived from alpha0022's active build_configstring_block/build_gamestate,
 * with 1.1.1.13 settings from 2-START-GAMESERVER.cmd (no IW4 section counts).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "configstrings.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "notetrack_effects.h"

static unsigned effect_prefix(const char *value)
{
    if(!value || strlen(value)<3 || value[0]<'0' || value[0]>'9' ||
       value[1]<'0' || value[1]>'9' || value[2]<'0' || value[2]>'9') return 0;
    return (value[0]-'0')*100+(value[1]-'0')*10+value[2]-'0';
}
static unsigned notetrack_effect(const char *row,size_t *start,size_t *end)
{
    size_t i=0,begin=0; unsigned field=0,effect=0;
    if(!row) return 0;
    for(;;++i) {
        if(row[i]!=';' && row[i]) continue;
        if(field==3) {
            size_t j;
            if(!row[i] || i==begin || i-begin>3) return 0;
            for(j=begin;j<i;++j) {
                if(row[j]<'0' || row[j]>'9') return 0;
                effect=effect*10+row[j]-'0';
            }
            *start=begin; *end=i; return effect;
        }
        if(!row[i]) return 0;
        ++field; begin=i+1;
    }
}
int ReserveByteEffectSlot(configstrings_t *bank,const configstrings_t *base,
    unsigned effect,const unsigned char pinned[BYTE_EFFECT_SLOTS])
{
    configstrings_t candidate={0};
    unsigned char protected[BYTE_EFFECT_SLOTS]={0};
    const char *wanted,*displaced; unsigned slot,i,ref;
    size_t start,end;
    char rewritten[1024];
    if(!bank || !base || bank==base || !pinned || !effect || effect>=EFFECT_SLOTS) return -1;
    wanted=GetEffectiveConfigstring(bank,base,CS_EFFECTS+effect);
    if(!wanted || !*wanted || strlen(wanted)>=64) return -1;
    if(effect<BYTE_EFFECT_SLOTS) return (int)effect;
    memcpy(protected,pinned,sizeof(protected)); protected[0]=1;
    /* Constants can hold numeric selectors without a native binding. */
    for(i=0;i<TAGGED_EFFECT_SLOTS+TEAM_THERMAL_SLOTS;++i) {
        unsigned index=i<TAGGED_EFFECT_SLOTS ? CS_TAGGED_EFFECTS+i :
            CS_TEAM_THERMAL+i-TAGGED_EFFECT_SLOTS;
        ref=effect_prefix(GetEffectiveConfigstring(bank,base,index));
        if(ref<BYTE_EFFECT_SLOTS) protected[ref]=1;
    }
    for(i=0;i<NOTETRACK_EFFECT_SLOTS;++i) {
        ref=notetrack_effect(GetEffectiveConfigstring(bank,base,CS_NOTETRACK_EFFECTS+i),&start,&end);
        if(ref<BYTE_EFFECT_SLOTS) protected[ref]=1;
    }
    for(slot=1;slot<BYTE_EFFECT_SLOTS;++slot) if(!protected[slot]) break;
    if(slot==BYTE_EFFECT_SLOTS) return -1;
    displaced=GetEffectiveConfigstring(bank,base,CS_EFFECTS+slot);
    for(i=0;i<OL_MAX_CONFIGSTRINGS;++i)
        if(bank->values[i] && !OL_SetConfigstring(&candidate,i,bank->values[i])) goto fail;
    if(!OL_SetConfigstring(&candidate,CS_EFFECTS+slot,wanted) ||
       !OL_SetConfigstring(&candidate,CS_EFFECTS+effect,displaced ? displaced : "")) goto fail;
    for(i=0;i<TAGGED_EFFECT_SLOTS+TEAM_THERMAL_SLOTS;++i) {
        unsigned index=i<TAGGED_EFFECT_SLOTS ? CS_TAGGED_EFFECTS+i :
            CS_TEAM_THERMAL+i-TAGGED_EFFECT_SLOTS;
        const char *value=GetEffectiveConfigstring(bank,base,index);
        if(effect_prefix(value)!=effect) continue;
        if(strlen(value)>=sizeof(rewritten)) goto fail;
        snprintf(rewritten,sizeof(rewritten),"%03u%s",slot,value+3);
        if(!OL_SetConfigstring(&candidate,index,rewritten)) goto fail;
    }
    for(i=0;i<NOTETRACK_EFFECT_SLOTS;++i) {
        unsigned index=CS_NOTETRACK_EFFECTS+i;
        const char *value=GetEffectiveConfigstring(bank,base,index);
        if(notetrack_effect(value,&start,&end)!=effect) continue;
        if(strlen(value)>=sizeof(rewritten)-3) goto fail;
        memcpy(rewritten,value,start);
        snprintf(rewritten+start,sizeof(rewritten)-start,"%u%s",slot,value+end);
        if(!OL_SetConfigstring(&candidate,index,rewritten)) goto fail;
    }
    if(!ConfigstringPoolSize(&candidate,base,NULL)) goto fail;
    ClearConfigstrings(bank); *bank=candidate; return (int)slot;
fail:
    ClearConfigstrings(&candidate); return -1;
}

int ConstConfigChecksum(const char *zone_type, uint32_t *checksum)
{
    uint32_t value;
    if (!zone_type || !checksum) return 0;
    /* Active FS_CRC_BY_ZONE_TYPE in alpha0022/zone_crc.py. The zm measurement
     * was added after that file's introductory "unknown" paragraph. */
    if (!strcmp(zone_type, "mp")) value = UINT32_C(0xad1e1713);
    else if (!strcmp(zone_type, "coop")) value = UINT32_C(0xe8139f9b);
    else if (!strcmp(zone_type, "zm")) value = UINT32_C(0x801cef02);
    /* Survival uses the multiplayer table (MP weapons and classes); C1's
     * survival category keeps C1's own checksum of it. */
    else if (!strcmp(zone_type, "survival")) value = UINT32_C(0xad1e1713);
    else return 0;
    *checksum = value; return 1;
}

void ClearConfigstrings(configstrings_t *state)
{
    unsigned i;
    if (!state) return;
    for (i = 0; i < OL_MAX_CONFIGSTRINGS; ++i) free(state->values[i]);
    memset(state, 0, sizeof(*state));
}

int OL_SetConfigstring(configstrings_t *state, unsigned index, const char *value)
{
    char *copy = NULL;
    size_t size = value ? strlen(value) + 1 : 0;
    size_t old;
    if (!state || index >= OL_MAX_CONFIGSTRINGS) return 0;
    old = state->values[index] ? strlen(state->values[index]) + 1 : 0;
    if (size > SIZE_MAX - (state->bytes - old)) return 0;
    if (value) {
        copy = malloc(size);
        if (!copy) return 0;
        memcpy(copy, value, size);
    }
    free(state->values[index]); state->values[index] = copy;
    state->count = state->count - !!old + !!size;
    state->bytes = state->bytes - old + size;
    return 1;
}

const char *GetConfigstring(const configstrings_t *state, unsigned index)
{
    return state && index < OL_MAX_CONFIGSTRINGS ? state->values[index] : NULL;
}

const char *GetEffectiveConfigstring(const configstrings_t *overrides,
    const configstrings_t *inherited, unsigned index)
{
    const char *value = GetConfigstring(overrides, index);
    return value ? value : GetConfigstring(inherited, index);
}

int ConfigstringPoolSize(const configstrings_t *overrides,
    const configstrings_t *inherited, size_t *bytes)
{
    size_t total = 1;
    unsigned i;
    if (!overrides) return 0;
    for (i = 0; i < OL_MAX_CONFIGSTRINGS; ++i) {
        const char *value = overrides->values[i];
        size_t length, remaining;
        if (!value && inherited) value = inherited->values[i];
        if (!value || !*value) continue;
        remaining = CONFIGSTRING_POOL_BYTES - total;
        /* Reserve one byte for this value's NUL without overflowing a size
         * calculation, and stop scanning once the client capacity is exceeded. */
        for (length = 0; length < remaining && value[length]; ++length) {}
        if (length == remaining) return 0;
        total += length + 1;
    }
    if (bytes) *bytes = total;
    return 1;
}

int ImportConstantConfigstrings(configstrings_t *out,size_t rows,const char *const *cells)
{
    configstrings_t result={0}; size_t i;
    if(!out || rows>OL_MAX_CONFIGSTRINGS || (rows && !cells)) return 0;
    for(i=0;i<rows;++i) {
        const unsigned char *p=(const unsigned char *)cells[2*i]; unsigned index=0;
        if(!p || !*p || !cells[2*i+1]) goto fail;
        for(;*p;++p) {
            /* The overflow guard compares an unsigned running index against a
             * signed expression; make the bound unsigned so the comparison has
             * one signedness. The value is always in range: the digit is 0..9
             * and OL_MAX_CONFIGSTRINGS is far larger. */
            if(*p<'0' || *p>'9' ||
               index>(unsigned)(OL_MAX_CONFIGSTRINGS-1-(*p-'0'))/10u) goto fail;
            index=index*10+(*p-'0');
        }
        if(result.values[index] || !OL_SetConfigstring(&result,index,cells[2*i+1])) goto fail;
    }
    ClearConfigstrings(out); *out=result; return 1;
fail:
    ClearConfigstrings(&result); return 0;
}
/* Set by const_models.c: a constant slot it published under the shipped
 * name keeps answering lookups by the constant name. */
int (*const_alias_hook)(unsigned first,const char *base,const char *changed);
static int register_inherited_name(configstrings_t *overrides,const configstrings_t *inherited,
    const char *name,unsigned first,unsigned count,size_t limit,int token)
{
    unsigned slot,available=0; size_t length=0;
    if(!overrides || !inherited || overrides==inherited || !name) return -1;
    if(!*name) return 0;
    while(name[length] && length<limit) {
        unsigned char c=(unsigned char)name[length++];
        if(token && (c<=32 || c==127 || c=='"' || c==';' || c=='\\')) return -1;
    }
    if(length==limit) return -1;
    for(slot=1;slot<count;++slot) {
        const char *base=inherited->values[first+slot],*changed=overrides->values[first+slot];
        const char *effective=changed ? changed : base;
        if(effective && !strcmp(effective,name)) return (int)slot;
        if(changed && base && const_alias_hook && !strcmp(base,name) &&
           const_alias_hook(first,base,changed)) return (int)slot;
        if(!available && (!base || !*base) && (!changed || !*changed)) available=slot;
    }
    if(!available || !OL_SetConfigstring(overrides,first+available,name)) return -1;
    return (int)available;
}
int RegisterHudMaterial(configstrings_t *overrides,const configstrings_t *inherited,const char *name)
{ return register_inherited_name(overrides,inherited,name,OL_CS_MATERIALS,HUD_MATERIAL_SLOTS,64,1); }
int RegisterLocalizedString(configstrings_t *overrides,const configstrings_t *inherited,const char *text)
{ return register_inherited_name(overrides,inherited,text,OL_CS_LOCALIZEDSTRINGS,LOCALIZED_SLOTS,1024,0); }
int RegisterEffectName(configstrings_t *overrides,const configstrings_t *inherited,const char *name)
{ return register_inherited_name(overrides,inherited,name,CS_EFFECTS,EFFECT_SLOTS,64,1); }
int RegisterByteEffectName(configstrings_t *overrides,const configstrings_t *inherited,const char *name)
{ return register_inherited_name(overrides,inherited,name,CS_EFFECTS,BYTE_EFFECT_SLOTS,64,1); }
int RegisterTaggedEffect(configstrings_t *overrides,const configstrings_t *inherited,unsigned effect,const char *tag)
{
    char value[1024]; int length;
    if(!effect || effect>=EFFECT_SLOTS || !tag || !*tag) return -1;
    /* C1 543e42..543e4c formats c576e0 "%03d%s", then searches
     * slots1..255 and publishes CS3971+slot at543f21..543f2a. */
    length=snprintf(value,sizeof(value),"%03u%s",effect,tag);
    if(length<0 || (size_t)length>=sizeof(value)) return -1;
    return register_inherited_name(overrides,inherited,value,CS_TAGGED_EFFECTS,
        TAGGED_EFFECT_SLOTS,sizeof(value),1);
}
static int register_asset_name(configstrings_t *state, const char *name,
                               unsigned first, unsigned count)
{
    unsigned slot, available = 0;
    size_t length = 0;
    if (!state || !name) return -1;
    if (!*name) return 0;
    while (name[length] && length < 64) {
        if ((unsigned char)name[length] < 32 || name[length] == 127) return -1;
        ++length;
    }
    if (length == 64) return -1;
    for (slot = 1; slot < count; ++slot) {
        const char *old = state->values[first + slot];
        if (old && !strcmp(old, name)) return slot;
        if (!available && (!old || !*old)) available = slot;
    }
    if (!available || !OL_SetConfigstring(state, first + available, name)) return -1;
    return available;
}

int RegisterModelName(configstrings_t *state, const char *name)
{
    return register_asset_name(state, name, OL_CS_MODELS, MODEL_SLOTS);
}
int RegisterTagName(configstrings_t *state, const char *name)
{
    return register_asset_name(state, name, OL_CS_TAGS, TAG_SLOTS);
}
/* Model and tag banks are inherited with the fastfile just like effects.
 * Allocate only truly unused slots; never overwrite an inherited entry while
 * an entity or archived snapshot can still refer to its numeric index. */
static const configstrings_t empty_asset_inheritance;
int RegisterInheritedModelName(configstrings_t *state,
    const configstrings_t *inherited, const char *name)
{
    return register_inherited_name(state, inherited ? inherited : &empty_asset_inheritance,
        name, OL_CS_MODELS, MODEL_SLOTS, 64, 1);
}
int RegisterInheritedTagName(configstrings_t *state,
    const configstrings_t *inherited, const char *name)
{
    return register_inherited_name(state, inherited ? inherited : &empty_asset_inheritance,
        name, OL_CS_TAGS, TAG_SLOTS, 64, 1);
}

void PlayerNameToken(char output[64], const char *name)
{
    unsigned n = 0;
    int separator = 0;
    if (!name) { output[0] = 0; return; }
    while (*name && n < 63) {
        unsigned char c = (unsigned char)*name++;
        if (c <= 32 || c == 127 || c == '"' || c == '\\' || c == '/' || c == ';') {
            separator = n != 0;
            continue;
        }
        if (separator && n < 63) output[n++] = '_';
        separator = 0;
        if (n < 63) output[n++] = c;
    }
    if (!n) { memcpy(output, "Player", 7); return; }
    output[n] = 0;
}

int SetPlayerNames(configstrings_t *state, unsigned count, const char *const *names)
{
    char *pending[PLAYERNAME_SLOTS] = {0};
    unsigned i;
    if (!state || count > PLAYERNAME_SLOTS || (count && !names)) return 0;
    for (i = 0; i < PLAYERNAME_SLOTS; ++i) {
        const char *name = i < count && names[i] ? names[i] : "";
        size_t length = 0;
        while (length < 63 && name[length]) ++length;
        pending[i] = malloc(length + 1);
        if (!pending[i]) goto fail;
        memcpy(pending[i], name, length); pending[i][length] = 0;
    }
    for (i = 0; i < PLAYERNAME_SLOTS; ++i) {
        unsigned index = CS_PLAYERNAMES + i;
        if (state->values[index]) state->bytes -= strlen(state->values[index]) + 1;
        else ++state->count;
        free(state->values[index]); state->values[index] = pending[i];
        state->bytes += strlen(pending[i]) + 1;
    }
    return 1;
fail:
    for (i = 0; i < PLAYERNAME_SLOTS; ++i) free(pending[i]);
    return 0;
}

int SetReplicatedDvars(configstrings_t *state, unsigned count,
                            const char *const *names, const char *const *values)
{
    char *pending[400] = {0};
    unsigned i, j;
    if (!state || count > MAX_REPLICATED_DVARS || (count && (!names || !values))) return 0;
    /* Allocate/validate before changing the bank, so a failed replacement is atomic. */
    for (i = 0; i < count; ++i) {
        if (!names[i] || !*names[i] || !values[i]) goto fail;
        for (j = 0; j < i; ++j) if (!strcmp(names[i], names[j])) goto fail;
        pending[i] = malloc(strlen(names[i]) + 1);
        pending[i+200] = malloc(strlen(values[i]) + 1);
        if (!pending[i] || !pending[i+200]) goto fail;
        strcpy(pending[i], names[i]); strcpy(pending[i+200], values[i]);
    }
    /* Publish explicit empties in the tail: clients may inherit const names there.
     * The client stops at the first empty name and pairs values with name + 200. */
    for (i = count; i < 200; ++i) {
        pending[i] = calloc(1, 1); pending[i+200] = calloc(1, 1);
        if (!pending[i] || !pending[i+200]) goto fail;
    }
    for (i = 0; i < 400; ++i) {
        unsigned index = CS_DVAR_NAMES + i;
        if (state->values[index]) state->bytes -= strlen(state->values[index]) + 1;
        else ++state->count;
        free(state->values[index]); state->values[index] = pending[i];
        state->bytes += strlen(pending[i]) + 1;
    }
    return 1;
fail:
    for (i = 0; i < 400; ++i) free(pending[i]);
    return 0;
}

int WriteConfigstrings(ol_msg_t *msg, const configstrings_t *state)
{
    unsigned i;
    if (!msg || !state) return 0;
    /* The reference emits this as a fresh substream, independently of any
     * preceding message's partially filled bit pool. */
    msg->bit = 0;
    WriteByte(msg, 3);
    WriteShort(msg, state->count);
    for (i = 0; i < OL_MAX_CONFIGSTRINGS; ++i) {
        if (!state->values[i]) continue;
        WriteBits(msg, 0, 1); /* absolute index */
        WriteBits(msg, i, 13);
        OL_WriteString(msg, state->values[i]);
    }
    WriteByte(msg, 5); /* gamestate command block terminator */
    return !msg->failed;
}

int WriteGamestate(ol_msg_t *msg, const configstrings_t *state,
                         const char *map, const char *zone_type, uint32_t const_crc,
                         uint32_t command_sequence, uint32_t client_number,
                         uint32_t checksum_feed)
{
    if (!msg || !state || !map || !*map || !zone_type ||
        (strcmp(zone_type, "mp") && strcmp(zone_type, "coop") && strcmp(zone_type, "zm") &&
         strcmp(zone_type, "survival"))) {
        if (msg) msg->failed = 1;
        return 0;
    }
    WriteByte(msg, 2);
    WriteLong(msg, command_sequence);
    OL_WriteString(msg, map);
    OL_WriteString(msg, zone_type);
    WriteLong(msg, const_crc);
    if (!WriteConfigstrings(msg, state)) return 0;
    WriteLong(msg, client_number);
    WriteLong(msg, checksum_feed);
    WriteByte(msg, 5);
    return !msg->failed;
}
