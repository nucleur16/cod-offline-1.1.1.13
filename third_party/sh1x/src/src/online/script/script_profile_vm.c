#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#include "script_profile_vm.h"
#include "ol_profile.h"
#include "ol_stats.h"
#include "profile_updates.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* Definition sets stay cached for as long as the assets that own their strings
 * and offset maps stay mounted; ResetScriptProfiles drops them. A failed
 * load is not cached, so a def that mounts later is still picked up. */
static struct {
    structured_schema_t schema;
    int loaded;
} definitions[2];
static const char *const definition_names[2]={"mp/playerdata.def","mp/playersecuredata.def"};
const structured_schema_t *ScriptProfileSchema(profile_buffer_t buffer)
{
    unsigned i=(unsigned)buffer;
    if(i>1)return NULL;
    if(!definitions[i].loaded) {
        structured_schema_t schema={0};
        if(LoadMountedStructuredSchema(definition_names[i],&schema)!=1) {
            FreeStructuredSchema(&schema);return NULL;
        }
        definitions[i].schema=schema;definitions[i].loaded=1;
    }
    return &definitions[i].schema;
}
const uint8_t *ScriptProfileBlob(unsigned client,profile_buffer_t buffer)
{
    if((unsigned)buffer>1)return NULL;
    return buffer==PROFILE_SECURE ? ClientSecureProfile(client,NULL)
                                        : ClientStats(client);
}
int ScriptProfileView(unsigned client,profile_buffer_t buffer,
    const structured_definition_t **definition,const uint8_t **record,size_t *capacity)
{
    const structured_schema_t *schema=ScriptProfileSchema(buffer);
    const uint8_t *blob=ScriptProfileBlob(client,buffer);
    if(!schema || !blob)return 0;
    return ProfileRecord(blob,STATS_SIZE,schema,definition,record,capacity);
}
void ResetScriptProfiles(void)
{
    unsigned i;
    for(i=0;i<2;++i) {
        FreeStructuredSchema(&definitions[i].schema);
        memset(&definitions[i],0,sizeof(definitions[i]));
    }
}
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "qcommon.h"
#include "zlib/zlib.h"
/* A session copy keeps the stored layout, so its leading crc32(blob+4) is
 * refreshed after every accepted write. That keeps the copy self-consistent
 * for readers that check it; it is not a claim that the edited profile was
 * uploaded, stored or authenticated. */
static void seal_session(uint8_t *blob)
{
    uint32_t crc=(uint32_t)crc32(0,blob+4,STATS_SIZE-4);unsigned i;
    for(i=0;i<4;++i)blob[i]=(uint8_t)(crc>>(8*i));
}
/* A path cannot consume more values than the VM argument stack, so its storage
 * comes from that capacity rather than from an assumed schema depth. This is
 * the bound the client match data wrapper already uses. */
#define PROFILE_MAX_PATH \
    (sizeof(((scrVmPub_t *)0)->stack)/sizeof(((scrVmPub_t *)0)->stack[0]))
/* Does any definition in this set declare this member on its root struct?
 * Ownership is answered from the definition set alone: no blob, no version or
 * checksum selection, so it does not depend on which profiles a client has. */
static int schema_declares_root(const structured_schema_t *schema,const char *name)
{
    uint32_t i,p;
    if(!schema || !schema->definitions || !name)return 0;
    for(i=0;i<schema->count;++i) {
        const structured_definition_t *d=schema->definitions+i;
        const structured_struct_t *s;
        if(d->root.kind!=5 || !d->structs || d->root.index>=d->struct_count)continue;
        s=d->structs+d->root.index;
        if(s->count && !s->properties)continue;
        for(p=0;p<s->count;++p)
            if(s->properties[p].name && !strcmp(name,s->properties[p].name))return 1;
    }
    return 0;
}
/* IW4-only persistent members.
 *
 * The IW4 gametype scripts this server compiles read and write members that
 * only IW4's playerdata.def declares: misses, timePlayedAllies/Opfor/Other,
 * restXPGoal, prestige, money, timeSinceLastLoot, featureNew, awards, bests,
 * challengeState, challengeProgress, and any others they pass by variable.
 * CoDOL's two definitions have no such members, so C1 would reject them and
 * the scripts would never get past connect. IW4 behaviour is that they exist
 * and start at zero, so this module answers every root neither CoDOL
 * definition declares from a per-client session store that is discarded on
 * connect/disconnect. profile_progress saves/restores this store by
 * account identity outside CoDOL's blobs; no client upload carries these
 * members. A name CoDOL does declare is
 * never shadowed, and each root taking this route is reported once in
 * developer mode so the adaptation stays visible. customClasses and
 * killstreaks are served by the native loadout bridge (codol_getLoadout*)
 * through scripts/codol's _class.gsc, and never reach this store. */
#define LEGACY_ENTRIES 1024u
#define LEGACY_KEY 96u
#define LEGACY_TEXT 64u
typedef struct {
    char key[LEGACY_KEY];
    int is_text;
    int value;
    char text[LEGACY_TEXT];
} legacy_entry_t;
static struct {
    legacy_entry_t *entries;
    unsigned count;
} legacy[MAX_CLIENTS];
/* Roots reported so far this level, so the developer trace prints once each. */
static char legacy_reported[64][48];
static unsigned legacy_reported_count;
static void legacy_report(const char *name)
{
    unsigned i;
    if(!com_developer || !com_developer->integer)return;
    for(i=0;i<legacy_reported_count;++i)
        if(!strcmp(legacy_reported[i],name))return;
    if(legacy_reported_count<sizeof(legacy_reported)/sizeof(*legacy_reported)) {
        strncpy(legacy_reported[legacy_reported_count],name,47);
        legacy_reported[legacy_reported_count][47]=0;
        ++legacy_reported_count;
    }
    ((void)0);
}
/* Legacy routing applies only to a root that neither mounted CoDOL definition
 * declares, so a name CoDOL does own is never shadowed. An unmounted
 * definition is left to route_buffer, which reports it. */
static int legacy_routed(int secure_only,const char *name)
{
    const structured_schema_t *public_set,*secure_set;
    if(secure_only || !name)return 0;
    public_set=ScriptProfileSchema(PROFILE_PUBLIC);
    secure_set=ScriptProfileSchema(PROFILE_SECURE);
    if(!public_set || !secure_set)return 0;
    if(schema_declares_root(public_set,name) || schema_declares_root(secure_set,name))return 0;
    legacy_report(name);
    return 1;
}
static int legacy_key(const structured_path_t *path,unsigned count,char *key,size_t size)
{
    unsigned i;size_t used=0;
    for(i=0;i<count;++i) {
        int n;
        if(path[i].name)n=snprintf(key+used,size-used,"%s%s",i ? "/" : "",path[i].name);
        else n=snprintf(key+used,size-used,"%s#%d",i ? "/" : "",path[i].index);
        if(n<0 || (size_t)n>=size-used)return 0;
        used+=(size_t)n;
    }
    return used>0;
}
static legacy_entry_t *legacy_find(unsigned client,const char *key,int create)
{
    legacy_entry_t *e;unsigned i;
    if(client>=MAX_CLIENTS)return NULL;
    for(i=0;i<legacy[client].count;++i)
        if(!strcmp(legacy[client].entries[i].key,key))return legacy[client].entries+i;
    if(!create)return NULL;
    if(!legacy[client].entries) {
        legacy[client].entries=calloc(LEGACY_ENTRIES,sizeof(legacy_entry_t));
        if(!legacy[client].entries)return NULL;
    }
    if(legacy[client].count>=LEGACY_ENTRIES)return NULL;
    e=legacy[client].entries+legacy[client].count++;
    memset(e,0,sizeof(*e));
    strncpy(e->key,key,LEGACY_KEY-1);
    return e;
}
void ScriptProfileForgetLegacy(unsigned client)
{
    if(client<MAX_CLIENTS)legacy[client].count=0;
}
/* Enumeration and insertion for the IW4-only root store, so it can be carried
 * across a session by profile_progress.c. Nothing here interprets a
 * value: the store is a flat key to int-or-text map and stays that way. */
unsigned ScriptProfileLegacyCount(unsigned client)
{
    return client<MAX_CLIENTS ? legacy[client].count : 0;
}
int ScriptProfileLegacyEntry(unsigned client,unsigned index,const char **key,
    int *is_text,int *value,const char **text)
{
    const legacy_entry_t *e;
    if(client>=MAX_CLIENTS || index>=legacy[client].count) return 0;
    e=legacy[client].entries+index;
    if(key)*key=e->key;
    if(is_text)*is_text=e->is_text;
    if(value)*value=e->value;
    if(text)*text=e->text;
    return 1;
}
/* Read one integer root out of the store. Returns 0 when the key is absent
 * or holds text, leaving *value untouched. */
int ScriptProfileLegacyGetInt(unsigned client,const char *key,int *value)
{
    const legacy_entry_t *e;
    if(client>=MAX_CLIENTS || !key || !*key) return 0;
    e=legacy_find(client,key,0);
    if(!e || e->is_text) return 0;
    if(value) *value=e->value;
    return 1;
}
int ScriptProfileLegacySet(unsigned client,const char *key,int is_text,int value,
    const char *text)
{
    legacy_entry_t *e;
    if(client>=MAX_CLIENTS || !key || !*key || strlen(key)>=LEGACY_KEY) return 0;
    if(is_text && (!text || strlen(text)>=LEGACY_TEXT)) return 0;
    e=legacy_find(client,key,1);
    if(!e) return 0;
    e->is_text=is_text ? 1 : 0;
    e->value=value;
    if(is_text) { strncpy(e->text,text,LEGACY_TEXT-1); e->text[LEGACY_TEXT-1]=0; }
    else e->text[0]=0;
    return 1;
}
static void legacy_get(unsigned client,const structured_path_t *path,unsigned count)
{
    char key[LEGACY_KEY];const legacy_entry_t *e;
    if(!legacy_key(path,count,key,sizeof(key))) {
        Scr_Error("getPlayerData: persistent data path is too long");return;
    }
    e=legacy_find(client,key,0);
    if(!e){Scr_AddInt(0);return;}
    if(e->is_text)Scr_AddString(e->text);
    else Scr_AddInt(e->value);
}
static void legacy_set(unsigned client,const structured_path_t *path,unsigned count,
    unsigned value_param)
{
    char key[LEGACY_KEY];legacy_entry_t *e;int type=Scr_GetType(value_param);
    if(!legacy_key(path,count,key,sizeof(key))) {
        Scr_Error("setPlayerData: persistent data path is too long");return;
    }
    e=legacy_find(client,key,1);
    if(!e) {
        Scr_Error(va("setPlayerData: no room to hold IW4-only member \"%s\" for this player",key));
        return;
    }
    if(type==VAR_STRING || type==VAR_ISTRING) {
        const char *text=Scr_GetString(value_param);
        e->is_text=1;
        strncpy(e->text,text ? text : "",LEGACY_TEXT-1);e->text[LEGACY_TEXT-1]=0;
    } else if(type==VAR_FLOAT) {
        e->is_text=0;e->value=(int)Scr_GetFloat(value_param);
    } else {
        e->is_text=0;e->value=Scr_GetInt(value_param);
    }
}
/* C1 consumes one script parameter per path step: a string names a struct
 * property or an enum-array entry, an int indexes an indexed array. Any other
 * script type is rejected against that parameter, as C1 rejects a non-int
 * index with "type %s is not an int". */
static int build_path(const char *method,unsigned count,structured_path_t *path)
{
    unsigned n;
    if(count>PROFILE_MAX_PATH) {
        Scr_Error(va("%s path exceeds the VM argument capacity",method));return 0;
    }
    for(n=0;n<count;++n) {
        unsigned type=Scr_GetType(n);
        if(type==VAR_STRING || type==VAR_ISTRING) {
            const char *name=Scr_GetString(n);
            if(!name){Scr_ParamError((int)n,va("%s: expected a data name",method));return 0;}
            path[n].name=name;path[n].index=0;
        } else if(type==VAR_INTEGER) {
            path[n].name=NULL;path[n].index=Scr_GetInt(n);
        } else {
            Scr_ParamError((int)n,va("%s: expected a data name or an array index",method));return 0;
        }
    }
    return 1;
}
/* Report the first path element the definition rejects, with C1's own text for
 * that result code, rather than one opaque failure for the whole path. */
static void path_error(const char *method,const char *owner,
    const structured_definition_t *d,const structured_path_t *path,unsigned count)
{
    structured_field_t field;unsigned n;int status=STRUCTURED_TYPE;
    for(n=1;n<=count;++n)
        if(!ResolveStructuredFieldStatus(d,path,n,&field,&status) &&
           (status!=STRUCTURED_TYPE || n==count)) {
            Scr_ParamError((int)(n-1),va("%s: %s in %s",method,
                StructuredStatusText(status),owner));
            return;
        }
    Scr_Error(va("%s: %s in %s",method,StructuredStatusText(STRUCTURED_TYPE),owner));
}
static int profile_entity(scr_entref_t ref,const char *method,unsigned *client)
{
    if(ref.classnum || ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client ||
       !g_entities[ref.entnum].r.inuse) {
        Scr_Error(va("%s: entity must be a player entity",method));return 0;
    }
    *client=ref.entnum;return 1;
}
/* Routing, stated as adaptation and not as C1 behaviour.
 *
 * C1 splits these reads into two script accessors, one per blob, and this
 * module keeps that split: getSecurePlayerData only ever reads mpdata_s. The
 * IW4 gametype sources this server compiles call getPlayerData for members
 * that CoDOL stores in either blob, so getPlayerData routes by which
 * definition set DECLARES the first path element:
 *   - declared by mp/playerdata.def            -> public blob
 *   - otherwise declared by mp/playersecuredata.def -> secure blob
 *   - declared by neither                      -> script error naming both
 * The decision is made before any blob is read, so a name owned by the public
 * def can never be answered from the secure blob when the public upload is
 * missing: that case is an error about the missing public profile. A name
 * declared by both defs resolves against the public one, and the chosen owner
 * is named in every error this call can raise. */
static int route_buffer(const char *method,int secure_only,const structured_path_t *path,
    profile_buffer_t *buffer)
{
    const structured_schema_t *public_set,*secure_set;
    if(!path[0].name) {
        Scr_ParamError(0,va("%s: the first parameter must name a member",method));return 0;
    }
    secure_set=ScriptProfileSchema(PROFILE_SECURE);
    if(secure_only) {
        if(!secure_set) {
            Scr_Error(va("%s: %s is not mounted",method,definition_names[PROFILE_SECURE]));return 0;
        }
        if(!schema_declares_root(secure_set,path[0].name)) {
            Scr_ParamError(0,va("%s: %s in %s",method,
                StructuredStatusText(STRUCTURED_NAME),
                definition_names[PROFILE_SECURE]));
            return 0;
        }
        *buffer=PROFILE_SECURE;return 1;
    }
    public_set=ScriptProfileSchema(PROFILE_PUBLIC);
    /* Public ownership has priority. Without its definition, a secure match
     * cannot establish that the same name is not public-owned. */
    if(!public_set) {
        Scr_Error(va("%s: %s is not mounted",method,
            definition_names[PROFILE_PUBLIC]));
        return 0;
    }
    if(schema_declares_root(public_set,path[0].name)) {*buffer=PROFILE_PUBLIC;return 1;}
    if(!secure_set) {
        Scr_Error(va("%s: %s is not mounted",method,
            definition_names[PROFILE_SECURE]));
        return 0;
    }
    if(schema_declares_root(secure_set,path[0].name)) {*buffer=PROFILE_SECURE;return 1;}
    Scr_ParamError(0,va("%s: %s: neither %s nor %s declares \"%s\"",method,
        StructuredStatusText(STRUCTURED_NAME),
        definition_names[PROFILE_PUBLIC],definition_names[PROFILE_SECURE],
        path[0].name));
    return 0;
}
/* C1 pushes ints for int, byte, bool and short members, a float for float
 * members and a string for string and enum members. Its short read sign
 * extends the stored 16 bits; the byte and bool reads do not. */
static void push_field(const char *method,const char *owner,
    const structured_definition_t *d,const structured_path_t *path,unsigned count,
    const uint8_t *record,size_t capacity,const structured_field_t *field)
{
    uint32_t value;const char *text;float number;
    if(field->type.kind==3 || field->type.kind==4) {
        if(!StructuredReadText(d,path,count,record,capacity,&text)) {
            Scr_Error(va("%s: %s in %s",method,
                StructuredStatusText(STRUCTURED_TYPE),owner));
            return;
        }
        Scr_AddString(text);return;
    }
    if(!StructuredReadScalar(&d->layout,field->type.kind,field->logical_index,
           record,capacity,&value)) {
        Scr_Error(va("%s: %s in %s",method,
            StructuredStatusText(STRUCTURED_TYPE),owner));
        return;
    }
    switch(field->type.kind) {
    case 2: Scr_AddInt(value ? 1 : 0);return;
    case 8: memcpy(&number,&value,sizeof(number));Scr_AddFloat(number);return;
    case 9: Scr_AddInt((int)(int16_t)(uint16_t)value);return;
    default: Scr_AddInt((int)value);return;
    }
}
/* Both accessors require the blob of the routed buffer. A member the routed
 * definition owns is never answered from the other blob. */
static int routed_view(const char *method,unsigned client,profile_buffer_t buffer,
    const structured_path_t *path,const structured_definition_t **d,
    const uint8_t **record,size_t *capacity)
{
    if(ScriptProfileView(client,buffer,d,record,capacity))return 1;
    if(!ScriptProfileBlob(client,buffer))
        Scr_Error(va("%s: cannot read persistent data: \"%s\" is stored in %s and this player "
            "has no %s",method,path[0].name,definition_names[buffer],
            buffer==PROFILE_SECURE ? "attached secure profile" : "complete profile upload"));
    else
        Scr_Error(va("%s: this player's %s does not match any definition in %s",method,
            buffer==PROFILE_SECURE ? "secure profile" : "profile upload",
            definition_names[buffer]));
    return 0;
}
static void get_player_data(scr_entref_t ref,int secure_only)
{
    const char *method=secure_only ? "getSecurePlayerData" : "getPlayerData";
    structured_path_t path[PROFILE_MAX_PATH];
    const structured_definition_t *d;const uint8_t *record;size_t capacity;
    structured_field_t field;profile_buffer_t buffer;unsigned client,count;
    int parameters=Scr_GetNumParam();
    if(!profile_entity(ref,method,&client))return;
    if(parameters<1){Scr_Error(va("%s requires at least one data name",method));return;}
    count=(unsigned)parameters;
    if(!build_path(method,count,path))return;
    if(legacy_routed(secure_only,path[0].name)){legacy_get(client,path,count);return;}
    if(!route_buffer(method,secure_only,path,&buffer))return;
    if(!routed_view(method,client,buffer,path,&d,&record,&capacity))return;
    if(!ResolveStructuredFieldStatus(d,path,count,&field,NULL)) {
        path_error(method,definition_names[buffer],d,path,count);return;
    }
    push_field(method,definition_names[buffer],d,path,count,record,capacity,&field);
}
void Scr_GetPlayerData(scr_entref_t ref){get_player_data(ref,0);}
void Scr_GetSecurePlayerData(scr_entref_t ref){get_player_data(ref,1);}
/* IW4's setPlayerData takes the same path plus a trailing value, converted by
 * the member's own type: ints for int, byte, bool and short members, a float
 * for float members and a string for string and enum members.
 *
 * The write lands in the profile core's session copy of the routed blob. Every
 * profile reader observes it, the uploaded bytes and the attached secure
 * profile are preserved for upload accounting and authentication, and a fresh
 * upload from the client discards the edited view. Nothing here uploads,
 * stores or re-authenticates a profile. */
void Scr_SetPlayerData(scr_entref_t ref)
{
    static const char method[]="setPlayerData";
    structured_path_t path[PROFILE_MAX_PATH];
    const structured_definition_t *d;const uint8_t *record;size_t capacity;
    structured_field_t field;profile_buffer_t buffer;
    unsigned client,count,value_param;uint8_t *blob;uint32_t raw;int written;
    size_t text_length=0;
    int parameters=Scr_GetNumParam();
    if(!profile_entity(ref,method,&client))return;
    if(parameters<2) {
        Scr_Error(va("%s requires at least one data name and a value",method));return;
    }
    count=(unsigned)parameters-1;value_param=count;
    if(!build_path(method,count,path))return;
    if(legacy_routed(0,path[0].name)){legacy_set(client,path,count,value_param);return;}
    if(!route_buffer(method,0,path,&buffer))return;
    if(!routed_view(method,client,buffer,path,&d,&record,&capacity))return;
    if(!ResolveStructuredFieldStatus(d,path,count,&field,NULL)) {
        path_error(method,definition_names[buffer],d,path,count);return;
    }
    blob=buffer==PROFILE_SECURE ? ClientSecureProfileWritable(client)
                                      : ClientStatsWritable(client);
    if(!blob) {
        Scr_Error(va("%s: cannot write \"%s\" for this player",method,path[0].name));return;
    }
    /* The session copy keeps the stored header, so it selects the definition
     * this call already resolved against. */
    if(!ProfileRecord(blob,STATS_SIZE,ScriptProfileSchema(buffer),
           &d,&record,&capacity)) {
        Scr_Error(va("%s: cannot write \"%s\" for this player",method,path[0].name));return;
    }
    /* Do not accept a write if its dirty-state preimage cannot be acquired. */
    if(buffer==PROFILE_PUBLIC && !BeginProfileUpdate(client)) {
        Scr_Error("setPlayerData: cannot track the public profile write");return;
    }
    if(field.type.kind==3 || field.type.kind==4) {
        const char *text=Scr_GetString(value_param);
        if(!text){Scr_ParamError((int)value_param,va("%s: expected a string value",method));return;}
        if(field.type.kind==3)text_length=strlen(text);
        written=StructuredWriteText(d,path,count,blob+4,capacity,text);
        if(!written) {
            Scr_ParamError((int)value_param,va("%s: %s",method,StructuredStatusText(
                field.type.kind==4 ? STRUCTURED_ENUM : STRUCTURED_STRING_LENGTH)));
            return;
        }
    } else if(field.type.kind==8) {
        float number=Scr_GetFloat(value_param);
        memcpy(&raw,&number,sizeof(raw));
        written=StructuredWriteScalar(&d->layout,field.type.kind,field.logical_index,
            blob+4,capacity,raw);
        if(!written) {
            Scr_Error(va("%s: %s in %s",method,
                StructuredStatusText(STRUCTURED_TYPE),definition_names[buffer]));
            return;
        }
    } else {
        int number=Scr_GetInt(value_param);
        if(field.type.kind==1 && (number<0 || number>255)) {
            Scr_ParamError((int)value_param,va("%s: %s",method,
                StructuredStatusText(STRUCTURED_BYTE_RANGE)));
            return;
        }
        if(field.type.kind==2)raw=number ? 1u : 0u;
        else if(field.type.kind==9)raw=(uint32_t)number&0xffffu;
        else raw=(uint32_t)number;
        written=StructuredWriteScalar(&d->layout,field.type.kind,field.logical_index,
            blob+4,capacity,raw);
        if(!written) {
            Scr_Error(va("%s: %s in %s",method,
                StructuredStatusText(STRUCTURED_TYPE),definition_names[buffer]));
            return;
        }
    }
    if(buffer==PROFILE_PUBLIC)
        MarkProfileField(client,&d->layout,&field,text_length);
    seal_session(blob);
}
#endif

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentScriptProfilesEmpty(void) {return resident_storage_zero(definitions,sizeof(definitions));}
