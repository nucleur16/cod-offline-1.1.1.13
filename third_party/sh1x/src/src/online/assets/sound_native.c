/* CoDOL sound assets in C1's own memory layout (feat/iw4-asset-layout, stage 2).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * The alias list, its 108-byte aliases, sound files, loaded sounds, curves and speaker maps are built exactly as
 * C1's loaders leave them in memory (xassets/sounds.h): the serialized alias record is copied whole and only its
 * pointer words are set (the five names, the sound file, the curve, the speaker map), which is the pointer
 * fix-up C1's loader performs. Values are still read and range-checked by sound_source.c. An in-process
 * host can therefore hand the server C1's own alias lists unchanged. Nothing here plays audio. */
#include "sound_native.h"
#include "assets.h"
#include "xassets/weapondef.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(snd_alias_list_t)==12,"C1 alias list (0x4b8750)");
_Static_assert(sizeof(struct snd_alias_t)==108,"C1 alias (0x4b84b0)");
_Static_assert(offsetof(struct snd_alias_t,mixerName)==16,"C1 alias fifth name");
_Static_assert(offsetof(struct snd_alias_t,soundFile)==20,"C1 alias sound file");
_Static_assert(offsetof(struct snd_alias_t,flags)==56,"C1 alias flags");
_Static_assert(offsetof(struct snd_alias_t,priority)==60,"C1 alias priority");
_Static_assert(offsetof(struct snd_alias_t,probability)==72,"C1 alias probability");
_Static_assert(offsetof(struct snd_alias_t,startDelay)==84,"C1 alias start delay");
_Static_assert(offsetof(struct snd_alias_t,volumeFalloffCurve)==88,"C1 alias curve");
_Static_assert(offsetof(struct snd_alias_t,speakerMap)==104,"C1 alias speaker map");
_Static_assert(sizeof(SoundFile_t)==12,"C1 sound file (0x4b8240)");
_Static_assert(sizeof(LoadedSound_t)==48,"C1 loaded sound (0x4b80b0)");
_Static_assert(offsetof(LoadedSound_t,data)==44,"C1 loaded sound payload");
_Static_assert(SND_CURVE_KNOTS==16 && sizeof(struct SndCurve)==136,"C1 curve (0x4b82b0)");
_Static_assert(sizeof(SpeakerMap_t)==408,"C1 speaker map");
typedef struct { uint8_t *data; size_t pos; } packing_t;
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
/* Last failure site in this module, for the engine error path. Pure: no output. */
static int snd_fail_line;
static int snd_fail(int line) { if(!snd_fail_line) snd_fail_line=line; return -1; }
static int snd_fail0(int line) { if(!snd_fail_line) snd_fail_line=line; return 0; }
int NativeSoundFailureLine(void) { return snd_fail_line; }
static char snd_fail_name[64];
const char *NativeSoundFailureName(void) { return snd_fail_name; }
static int finite_float(uint32_t bits,float *out)
{
    float value;
    memcpy(&value,&bits,4);
    if(!isfinite(value)) return snd_fail0(__LINE__);
    *out=value; return 1;
}
static int part(packing_t *p,const void *data,size_t size,unsigned alignment,void **out)
{
    size_t at=(p->pos+alignment-1)&~(size_t)(alignment-1);
    if(at>ASSET_LIMIT || size>ASSET_LIMIT-at) return snd_fail0(__LINE__);
    *out=p->data ? p->data+at : NULL;
    if(p->data && data && size) memcpy(*out,data,size);
    p->pos=at+size; return 1;
}
static int copy_string(packing_t *p,const char *text,void **out)
{
    if(!text) { *out=NULL; return 1; }
    return part(p,text,strlen(text)+1,1,out);
}
/* Serialized speaker maps are C1's ChannelMap_t layout: an entry count and six SpeakerLevels_t records of a
 * speaker index, a level count and two levels. Values are range checked, then the 400 bytes are kept as is. */
static int check_speakers(const speaker_map_t *source)
{
    unsigned map,entry;
    if(!source->channel_maps) return snd_fail0(__LINE__);
    for(map=0;map<4;++map) {
        const uint8_t *record=source->channel_maps+100*(size_t)map;
        uint32_t entries=u32(record);
        if(entries>6) return snd_fail0(__LINE__);
        for(entry=0;entry<6;++entry) {
            const uint8_t *levels=record+4+16*(size_t)entry;
            uint32_t speaker=u32(levels),count=u32(levels+4);
            float value[2];
            if(!finite_float(u32(levels+8),value) || !finite_float(u32(levels+12),value+1)) return snd_fail0(__LINE__);
            if(entry<entries && (count>2 || speaker>5)) return snd_fail0(__LINE__);
        }
    }
    return 1;
}
static int pack_alias(packing_t *p,const sound_alias_t *source,struct snd_alias_t *out,
    native_sound_t *stats)
{
    void *names[5],*file=NULL,*loaded=NULL,*curve=NULL,*speakers=NULL,*text=NULL;
    void *stream_dir=NULL,*stream_name=NULL;
    unsigned i;
    if(!source->names[0] || !*source->names[0] || !source->record) return snd_fail0(__LINE__);
    for(i=0;i<5;++i) if(!copy_string(p,source->names[i],names+i)) return snd_fail0(__LINE__);
    if(source->has_file) {
        if(!part(p,NULL,sizeof(SoundFile_t),4,&file)) return snd_fail0(__LINE__);
        if(source->file.has_loaded) {
            const loaded_sound_t *sound=&source->file.loaded;
            if(!sound->name || !*sound->name) return snd_fail0(__LINE__);
            /* Shipped zones carry empty placeholder sounds (",null.wav" with a
             * zero length and no payload); they keep a null payload. A nonzero
             * length without recovered bytes is malformed. */
            if(sound->data_length && !sound->payload) return snd_fail0(__LINE__);
            if(!part(p,NULL,sizeof(LoadedSound_t),4,&loaded) ||
                !copy_string(p,sound->name,&text)) return snd_fail0(__LINE__);
            if(p->data) {
                LoadedSound_t *c1=loaded;
                c1->name=text;
                c1->info.format=(int32_t)sound->format;
                c1->info.data_ptr=sound->payload;
                c1->info.data_len=sound->data_length;
                c1->info.rate=sound->rate;
                c1->info.bits=(int32_t)sound->bits;
                c1->info.channels=(int32_t)sound->channels;
                c1->info.sentinel=sound->sentinel;
                c1->info.samples=sound->samples;
                c1->info.block_size=sound->block_size;
                c1->info.initial_ptr=sound->payload;
                c1->data=(void *)(uintptr_t)sound->payload;
            }
            ++stats->loaded_sounds;
        } else {
            /* C1 0x7384e0 validates the name and tolerates an empty
             * directory, so only the name is required here. */
            if(!source->file.name || !*source->file.name) return snd_fail0(__LINE__);
            if(!copy_string(p,source->file.dir,&stream_dir) ||
                !copy_string(p,source->file.name,&stream_name)) return snd_fail0(__LINE__);
            ++stats->streams;
        }
        if(p->data) {
            SoundFile_t *c1=file;
            c1->type=source->file.type; c1->exists=source->file.exists;
            if(source->file.has_loaded) c1->u.loadSnd=loaded;
            else { c1->u.stream.dir=stream_dir; c1->u.stream.name=stream_name; }
        }
    }
    if(source->has_curve) {
        if(source->curve.knot_count>SND_CURVE_KNOTS) return snd_fail0(__LINE__);
        if(!part(p,NULL,sizeof(struct SndCurve),4,&curve) ||
            !copy_string(p,source->curve.name,&text)) return snd_fail0(__LINE__);
        if(p->data) {
            struct SndCurve *c1=curve;
            c1->filename=text; c1->knotCount=(uint16_t)source->curve.knot_count;
            memcpy(c1->knots,source->curve.knots,sizeof(float)*2*source->curve.knot_count);
        }
        ++stats->curves;
    }
    if(source->has_speakers) {
        if(!check_speakers(&source->speakers)) return snd_fail0(__LINE__);
        if(!part(p,NULL,sizeof(SpeakerMap_t),4,&speakers) ||
            !copy_string(p,source->speakers.name,&text)) return snd_fail0(__LINE__);
        if(p->data) {
            SpeakerMap_t *c1=speakers;
            c1->isDefault=source->speakers.is_default; c1->name=text;
            memcpy(c1->channelMaps,source->speakers.channel_maps,sizeof(c1->channelMaps));
        }
        ++stats->speaker_maps;
    }
    if(out) {
        /* The record as C1 holds it, then its pointer words. */
        memcpy(out,source->record,sizeof(*out));
        out->aliasName=names[0]; out->subtitle=names[1];
        out->secondaryAliasName=names[2]; out->chainAliasName=names[3];
        out->mixerName=names[4];
        out->soundFile=file;
        out->volumeFalloffCurve=curve; out->speakerMap=speakers;
    }
    ++stats->aliases; return 1;
}
static int pack(const sound_alias_list_t *source,native_sound_t *stats,packing_t *p)
{
    snd_alias_list_t *list=(snd_alias_list_t *)p->data;
    struct snd_alias_t *aliases; void *memory,*name; unsigned i;
    p->pos=sizeof(snd_alias_list_t);
    stats->aliases=stats->loaded_sounds=stats->streams=stats->curves=stats->speaker_maps=0;
    if(!copy_string(p,source->name,&name)) return snd_fail0(__LINE__);
    if(!part(p,NULL,(size_t)source->count*sizeof(struct snd_alias_t),4,&memory)) return snd_fail0(__LINE__);
    aliases=memory;
    for(i=0;i<source->count;++i)
        if(!pack_alias(p,source->aliases+i,aliases ? aliases+i : NULL,stats)) return snd_fail0(__LINE__);
    if(p->pos>INT32_MAX) return snd_fail0(__LINE__);
    if(list) { list->aliasName=name; list->head=aliases; list->count=(int)source->count; }
    return 1;
}
void FreeNativeSound(native_sound_t *out)
{ if(out) { free(out->storage); memset(out,0,sizeof(*out)); } }
int BuildNativeSoundAliasList(const sound_alias_list_t *source,native_sound_t *out)
{
    packing_t p={0}; native_sound_t result={0};
    if(!source || !out || !source->name || !*source->name || *source->name==',' ||
        !source->count || !source->aliases ||
        source->count>ASSET_LIMIT/sizeof(struct snd_alias_t)) return snd_fail0(__LINE__);
    if(!pack(source,&result,&p)) return snd_fail0(__LINE__);
    result.size=p.pos;
    if(!(result.storage=calloc(1,result.size))) return snd_fail0(__LINE__);
    p.data=result.storage;
    if(!pack(source,&result,&p) || p.pos!=result.size) { free(result.storage); return snd_fail0(__LINE__); }
    result.list=result.storage;
    FreeNativeSound(out); *out=result; return 1;
}
snd_alias_list_t *FindEngineSoundAlias(const char *name)
{
    const snd_alias_list_t *list=NULL;
    if(!name || !*name) return NULL;
    /* Absent and malformed both yield null here: the engine's find-alias
     * contract has no way to report a broken definition. */
    return LoadEngineNativeSound(name,&list)==1 ? (snd_alias_list_t *)list : NULL;
}
typedef struct weapon_sound_cache_s {
    char *name;
    weapon_sound_bindings_t bindings;
    struct weapon_sound_cache_s *next;
} weapon_sound_cache_t;
static weapon_sound_cache_t *weapon_sounds;
static int bind_sound(const char *name,snd_alias_list_t **slot,weapon_sound_bindings_t *out)
{
    const snd_alias_list_t *alias; int status;
    if(!name || !*name) return 1;
    ++out->requested;
    status=LoadEngineNativeSound(name,&alias);
    if(status<0) { strncpy(snd_fail_name,name,sizeof(snd_fail_name)-1); return snd_fail0(__LINE__); }
    /* CoDOL keeps alias assets in sound zones a dedicated server need not
     * mount. An unknown name is not a malformed definition. */
    if(!status) { ++out->absent; return 1; }
    *slot=(snd_alias_list_t *)alias; ++out->resolved; return 1;
}
int BuildNativeWeaponSoundBindings(const weapon_sounds_t *sounds,
    const weapon_tail_t *tail,weapon_sound_bindings_t *out)
{
    weapon_sound_bindings_t bindings={0};
    unsigned i,j;
    if(!sounds || !tail || !out || tail->count>64) return snd_fail(__LINE__);
    for(i=0;i<WEAPON_SOUND_SLOTS;++i)
        if(!bind_sound(sounds->names[i],bindings.names+i,&bindings)) return snd_fail(__LINE__);
    for(i=0;i<2;++i) for(j=0;j<WEAPON_SOUND_SURFACES;++j)
        if(!bind_sound(sounds->arrays[i][j],bindings.arrays[i]+j,&bindings)) return snd_fail(__LINE__);
    for(i=0;i<tail->count;++i) {
        if(tail->entries[i].kind!=WEAPON_DEP_SOUND) continue;
        if(bindings.tail_count>=WEAPON_SOUND_TAIL) return snd_fail(__LINE__);
        bindings.tail_offsets[bindings.tail_count]=tail->entries[i].offset;
        if(!bind_sound(tail->entries[i].name,bindings.tail+bindings.tail_count,&bindings)) return snd_fail(__LINE__);
        ++bindings.tail_count;
    }
    *out=bindings; return 1;
}
int LoadEngineWeaponSounds(const char *weapon,const weapon_sounds_t *sounds,
    const weapon_tail_t *tail,const weapon_sound_bindings_t **out)
{
    weapon_sound_bindings_t bindings;
    weapon_sound_cache_t *cached;
    snd_fail_line=0; /* Report the innermost failure of this request. */
    snd_fail_name[0]=0;
    ResetSoundSourceFailure();
    if(!weapon || !*weapon || !sounds || !tail || !out || tail->count>64) return snd_fail(__LINE__);
    for(cached=weapon_sounds;cached;cached=cached->next)
        if(!strcmp(cached->name,weapon)) { *out=&cached->bindings; return 1; }
    if(BuildNativeWeaponSoundBindings(sounds,tail,&bindings)!=1) return snd_fail(__LINE__);
    if(!(cached=calloc(1,sizeof(*cached)))) return snd_fail(__LINE__);
    if(!(cached->name=malloc(strlen(weapon)+1))) { free(cached); return snd_fail(__LINE__); }
    memcpy(cached->name,weapon,strlen(weapon)+1);
    cached->bindings=bindings; cached->next=weapon_sounds; weapon_sounds=cached;
    *out=&cached->bindings; return 1;
}
int ApplyNativeWeaponSounds(const weapon_sound_bindings_t *bindings,struct WeaponDef *out)
{
    /* CoDOL's 53 slots are IW3's 45 fields in the same order with three extra
     * slots interleaved (9 the akimbo fire variant, 12/13 the fire-loop end
     * pair) and five appended. Roles are established by C1 consumers and by
     * which shipped weapons populate each slot; see the pass report. */
    static const struct { unsigned slot; size_t field; } mapping[]={
        {0,offsetof(struct WeaponDef,pickupSound)},
        {1,offsetof(struct WeaponDef,pickupSoundPlayer)},
        {2,offsetof(struct WeaponDef,ammoPickupSound)},
        {3,offsetof(struct WeaponDef,ammoPickupSoundPlayer)},
        {4,offsetof(struct WeaponDef,projectileSound)},
        {5,offsetof(struct WeaponDef,pullbackSound)},
        {6,offsetof(struct WeaponDef,pullbackSoundPlayer)},
        {7,offsetof(struct WeaponDef,fireSound)},
        {8,offsetof(struct WeaponDef,fireSoundPlayer)},
        {10,offsetof(struct WeaponDef,fireLoopSound)},
        {11,offsetof(struct WeaponDef,fireLoopSoundPlayer)},
        {14,offsetof(struct WeaponDef,fireStopSound)},
        {15,offsetof(struct WeaponDef,fireStopSoundPlayer)},
        {16,offsetof(struct WeaponDef,fireLastSound)},
        {17,offsetof(struct WeaponDef,fireLastSoundPlayer)},
        {18,offsetof(struct WeaponDef,emptyFireSound)},
        {19,offsetof(struct WeaponDef,emptyFireSoundPlayer)},
        {20,offsetof(struct WeaponDef,meleeSwipeSound)},
        {21,offsetof(struct WeaponDef,meleeSwipeSoundPlayer)},
        {22,offsetof(struct WeaponDef,meleeHitSound)},
        {23,offsetof(struct WeaponDef,meleeMissSound)},
        {24,offsetof(struct WeaponDef,rechamberSound)},
        {25,offsetof(struct WeaponDef,rechamberSoundPlayer)},
        {26,offsetof(struct WeaponDef,reloadSound)},
        {27,offsetof(struct WeaponDef,reloadSoundPlayer)},
        {28,offsetof(struct WeaponDef,reloadEmptySound)},
        {29,offsetof(struct WeaponDef,reloadEmptySoundPlayer)},
        {30,offsetof(struct WeaponDef,reloadStartSound)},
        {31,offsetof(struct WeaponDef,reloadStartSoundPlayer)},
        {32,offsetof(struct WeaponDef,reloadEndSound)},
        {33,offsetof(struct WeaponDef,reloadEndSoundPlayer)},
        {34,offsetof(struct WeaponDef,detonateSound)},
        {35,offsetof(struct WeaponDef,detonateSoundPlayer)},
        {36,offsetof(struct WeaponDef,nightVisionWearSound)},
        {37,offsetof(struct WeaponDef,nightVisionWearSoundPlayer)},
        {38,offsetof(struct WeaponDef,nightVisionRemoveSound)},
        {39,offsetof(struct WeaponDef,nightVisionRemoveSoundPlayer)},
        {40,offsetof(struct WeaponDef,altSwitchSound)},
        {41,offsetof(struct WeaponDef,altSwitchSoundPlayer)},
        {42,offsetof(struct WeaponDef,raiseSound)},
        {43,offsetof(struct WeaponDef,raiseSoundPlayer)},
        {44,offsetof(struct WeaponDef,firstRaiseSound)},
        {45,offsetof(struct WeaponDef,firstRaiseSoundPlayer)},
        {46,offsetof(struct WeaponDef,putawaySound)},
        {47,offsetof(struct WeaponDef,putawaySoundPlayer)}
    };
    unsigned i,bounce=0;
    if(!bindings || !out || bindings->tail_count>WEAPON_SOUND_TAIL) return 0;
    for(i=0;i<sizeof(mapping)/sizeof(mapping[0]);++i)
        *(snd_alias_list_t **)((char *)out+mapping[i].field)=bindings->names[mapping[i].slot];
    for(i=0;i<bindings->tail_count;++i) switch(bindings->tail_offsets[i]) {
    case 0x7c0: out->projExplosionSound=bindings->tail[i]; break;
    case 0x7c4: out->projDudSound=bindings->tail[i]; break;
    case 0x8f0: out->projIgnitionSound=bindings->tail[i]; break;
    default: break;
    }
    /* Surface bounce sounds keep the source's 31 entries. The caller retains
     * these bindings at a stable address for the definition's lifetime. */
    for(i=0;i<WEAPON_SOUND_SURFACES;++i) if(bindings->arrays[0][i]) ++bounce;
    out->bounceSound=bounce ? (struct snd_alias_list_t **)bindings->arrays[0] : NULL;
    return 1;
}
void ResetNativeWeaponSounds(void)
{
    while(weapon_sounds) {
        weapon_sound_cache_t *entry=weapon_sounds;
        weapon_sounds=entry->next; free(entry->name); free(entry);
    }
}

/* ---- C1's own alias lists (in-process host) -------------------------------------------------------------------- */
typedef struct { uint8_t *data; size_t pos; } c1_copy_t;
static void *c1_take(c1_copy_t *c,const void *src,size_t n)
{
    void *at; size_t aligned=(c->pos+3)&~(size_t)3;
    at=c->data ? c->data+aligned : NULL;
    if(at) { if(src) memcpy(at,src,n); else memset(at,0,n); }
    c->pos=aligned+n; return at;
}
static const char *c1_text(c1_copy_t *c,const char *s)
{
    size_t n; char *at;
    if(!s) return NULL;
    n=strlen(s)+1; at=c->data ? (char *)c->data+c->pos : NULL;
    if(at) memcpy(at,s,n);
    c->pos+=n; return at;
}
/* C1 sizes: SndCurve 136 (name at +0), SpeakerMap 408 (name at +4), LoadedSound 48 (name at +0, payload +0x2c). */
static int c1_copy_list(c1_copy_t *c,const snd_alias_list_t *src,snd_alias_list_t **out)
{
    snd_alias_list_t *list; struct snd_alias_t *aliases; int i;
    if(!src || !src->aliasName || src->count<=0 || src->count>4096 || !src->head) return 0;
    list=c1_take(c,src,sizeof(*src));
    aliases=c1_take(c,src->head,(size_t)src->count*sizeof(*src->head));
    if(list) list->head=aliases;
    { const char *n=c1_text(c,src->aliasName); if(list) list->aliasName=n; }
    for(i=0;i<src->count;++i) {
        const struct snd_alias_t *a=src->head+i; struct snd_alias_t *d=aliases ? aliases+i : NULL;
        const char *t;
        t=c1_text(c,a->aliasName); if(d) d->aliasName=t;
        t=c1_text(c,a->subtitle); if(d) d->subtitle=t;
        t=c1_text(c,a->secondaryAliasName); if(d) d->secondaryAliasName=t;
        t=c1_text(c,a->chainAliasName); if(d) d->chainAliasName=t;
        t=c1_text(c,a->mixerName); if(d) d->mixerName=t;
        if(a->soundFile) {
            SoundFile_t *f=c1_take(c,a->soundFile,sizeof(SoundFile_t));
            if(d) d->soundFile=f;
            if(a->soundFile->type==1) {
                if(a->soundFile->u.loadSnd) {
                    LoadedSound_t *l=c1_take(c,a->soundFile->u.loadSnd,48);
                    t=c1_text(c,a->soundFile->u.loadSnd->name);
                    if(f) f->u.loadSnd=l;
                    if(l) { l->name=t; l->data=NULL; }
                }
            } else {
                const char *dir=c1_text(c,a->soundFile->u.stream.dir),*name=c1_text(c,a->soundFile->u.stream.name);
                if(f) { f->u.stream.dir=dir; f->u.stream.name=name; }
            }
        }
        if(a->volumeFalloffCurve) {
            uint8_t *curve=c1_take(c,a->volumeFalloffCurve,136); const char *n; memcpy(&n,a->volumeFalloffCurve,sizeof(n));
            t=c1_text(c,n);
            if(curve) memcpy(curve,&t,sizeof(t));
            if(d) d->volumeFalloffCurve=(struct SndCurve *)curve;
        }
        if(a->speakerMap) {
            uint8_t *map=c1_take(c,a->speakerMap,408); const char *n; memcpy(&n,(const uint8_t *)a->speakerMap+4,sizeof(n));
            t=c1_text(c,n);
            if(map) memcpy(map+4,&t,sizeof(t));
            if(d) d->speakerMap=(SpeakerMap_t *)map;
        }
    }
    if(out) *out=list;
    return 1;
}
int CopyC1SoundAliasList(const snd_alias_list_t *source,native_sound_t *out)
{
    c1_copy_t c={0}; native_sound_t result={0};
    if(!source || !out || !c1_copy_list(&c,source,NULL)) return 0;
    if(!(c.data=calloc(1,c.pos ? c.pos : 1))) return 0;
    result.size=c.pos; c.pos=0;
    if(!c1_copy_list(&c,source,&result.list) || c.pos!=result.size) { free(c.data); return 0; }
    result.storage=c.data; result.aliases=(unsigned)source->count;
    FreeNativeSound(out); *out=result; return 1;
}
